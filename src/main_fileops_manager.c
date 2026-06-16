#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include "../include/header_t4.h"
#include <sys/mman.h>
#include <semaphore.h>
#include <sys/wait.h>
#include <openssl/sha.h>
#include <signal.h>
//#define __STDC_FORMAT_MACROS
#include <inttypes.h>


char *directory;
int n;
char *ipc_path;
char *db_path;
int max_depth;
int ms;
int graceful_timeout;
char *pid_file;
volatile sig_atomic_t flag_for_status = 0;
volatile sig_atomic_t flag_for_shutdown = 0;

void handle_sigurs1(int sig) {
  flag_for_status = 1;
}

void handle_sigint_sigterm(int sig) {
  flag_for_shutdown = 1;
}

void handle_sigchld(int sig) {
  while(waitpid(-1, NULL, WNOHANG) > 0);
}

void print_format_posibile() {
  printf("Argumente insuficiente!\n");
  printf("Moduri posibile de rulare : \n");
  printf("./tools/fileops.sh run -- fileops_manager --root <dir> --workers <N> [--ipc data/ipc.mmap] [--db data/inventory.db] \
     [--max-depth <D>] [--simulate-work-ms <ms>] \n");
  printf("./tools/fileops.sh run -- fileops_manager --db data/inventory.db --verify\n");
  printf("./tools/fileops.sh run -- fileops_manager --db data/inventory.db --dump\n");
}

void mod_inventariere() {
  if(pid_file != NULL) {
    FILE *f = fopen(pid_file, "w");
    if(f) {
      fprintf(f, "%d\n", getpid());
      fclose(f);
    }
  }

  int fd_ipc;
  signal(SIGUSR1, handle_sigurs1);
  signal(SIGINT, handle_sigint_sigterm);
  signal(SIGTERM, handle_sigint_sigterm);
  signal(SIGCHLD, handle_sigchld);
  
  int worker_pipes[MAX_JOBS];
  if(-1 == (fd_ipc = open(ipc_path, O_CREAT | O_RDWR | O_TRUNC , 0666))) {
    printf("Eroare la deschiderea fisierului ipc\n");
    exit(1);
  }
  if(-1 == (ftruncate(fd_ipc, sizeof(mapped_data)))) {
    printf("Eroare la trunchierea fisierului ipc!\n");
    exit(1);
  }

  mapped_data *shared_memory = mmap(NULL, sizeof(mapped_data), PROT_READ | PROT_WRITE, MAP_SHARED, fd_ipc, 0);
  if(shared_memory == MAP_FAILED) {
    printf("Eroare la mmap!\n");
    exit(1);
  }

  if(-1 == (close(fd_ipc))) {
    printf("Eroare la inchiderea fisierului ipc!\n");
    exit(1);
  }
  
  memset(shared_memory, 0, sizeof(mapped_data));
  memcpy(shared_memory->magic, "INV4", 4);
  shared_memory->version = 1;
  shared_memory->worker_count = n;
  shared_memory->active_jobs = 0;

  sem_init(&shared_memory->counter_mutex, 1, 1);
  sem_init(&shared_memory->queue.sem_tail_head, 1, 1);
  sem_init(&shared_memory->queue.free_slots_cnt, 1, MAX_JOBS); //initial am MAX_JOBS locuri libere
  sem_init(&shared_memory->queue.items_available_cnt, 1, 0); //n am item uri inca, 0 joburi
  sem_init(&shared_memory->results_semaphor, 1, 1);
  sem_init(&shared_memory->free_slots_in_results, 1, MAX_RESULTS); //0 rezultate scrise, toate sloturile libere
  sem_init(&shared_memory->items_avail_in_results, 1, 0); //0 rezultate scrise
  
  //         0    1
  //queue = dir
  strncpy(shared_memory->queue.paths[0], directory, MAX_PATH);
  shared_memory->queue.tail = 1;
  shared_memory->queue.head = 0;
  shared_memory->active_jobs = 1; //directorul dat
  
  sem_post(&shared_memory->queue.items_available_cnt); //1 item valabil
  sem_wait(&shared_memory->queue.free_slots_cnt); //-1 sloturi libere
  
  for(int i = 0; i < n; i++) {
    int fd_pipe[2];
    if(-1 == pipe(fd_pipe)) {
      printf("Eroare la pipe la pasul %d\n", i);
      exit(1);
    }
    
    pid_t pid;
    if(-1 == (pid = fork())) {
      printf("Eroare la fork la pasul %d\n", i);
      exit(1);
    }
    if(pid == 0) { //sunt in fiu
      //0 - citire, 1 - scriere
      close(fd_pipe[0]); //inchid citirea ca sa scrie
      
      char id[12];
      char depth[10];
      char ms_char[10];
      char fd_pipe_char[10];
      
      
      sprintf(depth, "%d", max_depth);
      sprintf(id, "%d", i);
      sprintf(ms_char, "%d", ms);
      sprintf(fd_pipe_char, "%d", fd_pipe[1]); //canalul de scriere

      execl("./bin/fileops_worker", "fileops_worker", "--worker-id", id, "--ipc", ipc_path, "--root", directory, 
         "--max-depth", depth, "--simulate-work-ms", ms_char, "--control-fd", fd_pipe_char, NULL);
      perror("Eroare la execl\n");
      exit(i * 10);
    } else {
      //sunt in parinte
      //el citeste nu si scrie - blochez canalul de scriere(gen 1)
      close(fd_pipe[1]);
      fcntl(fd_pipe[0], F_SETFL, O_NONBLOCK); //neblocantt sa nu mi crape
      worker_pipes[i] = fd_pipe[0]; //salvez capatul de citire 
      
      shared_memory->stats[i].pid = pid;
      shared_memory->stats[i].worker_id = i;
    } 
  }

  char db_path_temporary[256];
  int len = strlen(db_path);
  if(len >= 3 && strcmp(&db_path[len - 3], ".db") == 0) {
    strncpy(db_path_temporary, db_path, len - 3);
    db_path_temporary[len-3] = '\0';
    strcat(db_path_temporary, "_tmp.db");
  } else {
    sprintf(db_path_temporary, "%s.tmp", db_path);
  }

//  printf("%s\n", db_path_temporary);
  
  FILE *f_temporary = fopen(db_path_temporary, "wb");
  if(!f_temporary) {
    printf("Eroare la deschiderea fisierului pentru baza de date temporara\n");
    exit(1);
  }
  
  db_header header = {0};
  fwrite(&header, sizeof(db_header), 1, f_temporary);
  
  uint32_t total_number_of_files = 0;
  uint64_t total_number_of_bytes = 0;
  int is_completed = 1;

  while(1) {
    if(flag_for_status == 1) { //se cere statusul
      flag_for_status = 0; //resetez

      int queued_jobs = 0;
      sem_wait(&shared_memory->queue.sem_tail_head);
      if(shared_memory->queue.tail >= shared_memory->queue.head) {
        queued_jobs = shared_memory->queue.tail - shared_memory->queue.head;
      } else {
        queued_jobs = MAX_JOBS - shared_memory->queue.head + shared_memory->queue.tail;
      }
      sem_post(&shared_memory->queue.sem_tail_head);
      int workers_still_alive = 0;
      for(int i = 0; i < n; i++) {
        if(waitpid(shared_memory->stats[i].pid, NULL, WNOHANG) == 0) {
          workers_still_alive++;
        }
      }
      
      printf("STATUS queued_jobs=%d active_jobs=%d files=%d bytes=%" PRIu64 " workers_alive=%d complete=%d\n", 
        queued_jobs, shared_memory->active_jobs, total_number_of_files, total_number_of_bytes, workers_still_alive, is_completed);
      fflush(stdout);
    }

    if(flag_for_shutdown == 1) {
      is_completed = 0;
      break;
    }

    for(int i = 0; i < n; i++) {
      char pipe_buffer[4096]; 
      ssize_t bytes_read = read(worker_pipes[i], pipe_buffer, sizeof(pipe_buffer) - 1); //trebuie si \0 sa pun 
      if(bytes_read > 0) {
        //am ce citi din pipe
        pipe_buffer[bytes_read] = '\0';
        
        //ce e in pipe 
        printf("[Manager, pipe] %s", pipe_buffer); 
        fflush(stdout);
      }
    }

    sem_wait(&shared_memory->counter_mutex); //vr sa citesc cate joburi sunt active
    if(shared_memory->active_jobs == 0 && shared_memory->res_head == shared_memory->res_tail) {
      sem_post(&shared_memory->counter_mutex);
      break;
    }
    sem_post(&shared_memory->counter_mutex);

    if (sem_trywait(&shared_memory->items_avail_in_results) != 0) {
      usleep(1000); 
      continue;
    }    

    //vr sa citesc un record din results
//    sem_wait(&shared_memory->items_avail_in_results);
    sem_wait(&shared_memory->results_semaphor);
    
    file_record record = shared_memory->results[shared_memory->res_head];
    shared_memory->res_head = ( shared_memory->res_head + 1 ) % MAX_RESULTS;
    
    sem_post(&shared_memory->free_slots_in_results);
    sem_post(&shared_memory->results_semaphor);

    fwrite(&record, sizeof(file_record), 1, f_temporary);
    total_number_of_files++;
    total_number_of_bytes += record.size;
  }

  if(is_completed == 0) {
    for(int i = 0; i < n; i++) {
      kill(shared_memory->stats[i].pid, SIGTERM); //ii opresc
    }
    
    int secunde = 0;
    int workers_still_alive = n;
    while (secunde < graceful_timeout && workers_still_alive > 0) {
      workers_still_alive = 0;
      for(int i = 0; i < n; i++) {
        if(waitpid(shared_memory->stats[i].pid, &shared_memory->stats[i].exit_status, WNOHANG) == 0) {
          workers_still_alive++;
        }
      }
      if(workers_still_alive > 0) {
        sleep(1);
        secunde++;
      }
    }
    if(workers_still_alive > 0) { //mai poat ss ramana
      for(int i = 0; i < n; i++) {
        kill(shared_memory->stats[i].pid, SIGKILL);
        waitpid(shared_memory->stats[i].pid, &shared_memory->stats[i].exit_status, 0);
      }
    }
  } else {
    for(int i = 0; i < n; i++) {
      sem_wait(&shared_memory->queue.free_slots_cnt);
      sem_wait(&shared_memory->queue.sem_tail_head);
      strncpy(shared_memory->queue.paths[shared_memory->queue.tail], "STOP", MAX_PATH);
      shared_memory->queue.tail = (shared_memory->queue.tail + 1) % MAX_JOBS;
      sem_post(&shared_memory->queue.sem_tail_head);
      sem_post(&shared_memory->queue.items_available_cnt);
    }
  
    for(int i = 0; i < n; i++) {
      int status;
      waitpid(shared_memory->stats[i].pid, &status, 0);
      shared_memory->stats[i].exit_status = status;
    
      fwrite(&shared_memory->stats[i], sizeof(worker_stats), 1, f_temporary);
    }
  }

  //trb inchise capetele de citire - 0
  for(int i = 0; i < n; i++) {
    close(worker_pipes[i]);
  }   

  memcpy(header.magic, "INV4", 4);
  header.format_version = 1;
  header.flag = is_completed;
  header.file_record_count = total_number_of_files;
  header.worker_count = n;
  
  fseek(f_temporary, 0, SEEK_SET);
  fwrite(&header, sizeof(db_header), 1, f_temporary);
  
  if(-1 == fclose(f_temporary)) {
    printf("Eroare la inchiderea fisierului pentru baza de date temporara\n");
    exit(1);
  }
  
  if(rename(db_path_temporary, db_path) != 0) {
    printf("Eroare la rename\n");
    exit(1);
  }

  sem_destroy(&shared_memory->queue.sem_tail_head);
  sem_destroy(&shared_memory->queue.free_slots_cnt);
  sem_destroy(&shared_memory->queue.items_available_cnt);
  sem_destroy(&shared_memory->counter_mutex);
  sem_destroy(&shared_memory->results_semaphor);
  sem_destroy(&shared_memory->free_slots_in_results);
  sem_destroy(&shared_memory->items_avail_in_results);
  
  if(-1 == munmap(shared_memory, sizeof(mapped_data))) {
    printf("Eroare la munmap!\n");
    exit(1);
  }

  printf("Gata managrul\n");

//  printf("[Test] am ajuns aici deci n am erori\n");
}

void calculeaza_sha256(const char *drum, unsigned char *output) {

    int fd = open(drum, O_RDONLY);

    if (fd == -1) 
        return;

    SHA256_CTX ctx;
    SHA256_Init(&ctx);

    unsigned char buffer[4096];
    ssize_t bytes;

    while ((bytes = read(fd, buffer, sizeof(buffer))) > 0) {
        SHA256_Update(&ctx, buffer, bytes);
    }

    SHA256_Final(output, &ctx);

    close(fd);
}

void mod_db_verify() {
  FILE *f_db = fopen(db_path, "rb");
  if(!f_db) {
    printf("Eroare la deschiderea fisierului cu baza de date!\n");
    exit(1);
  }

  db_header header;
  if(1 != ( fread(&header, sizeof(db_header), 1, f_db)) ) {
    printf("Eroare la citirea din fisier");
    exit(1);
  }
  
  if(memcmp(header.magic, "INV4", 4) != 0) {
    printf("%s\n", header.magic);
    printf("baze de date invalida la magic !\n");
    exit(1);
  }
//  printf("*\n");

  uint32_t unmodified_cnt;
  uint32_t modified_cnt;
  uint32_t deleted_cnt;
  unmodified_cnt = modified_cnt = deleted_cnt = 0;
  for(int i = 0; i < header.file_record_count; i++) {
    printf("*\n");
    file_record record;
    if(1 != fread(&record, sizeof(file_record), 1, f_db) ) {
      printf("Eroare la citirea unui record din baza de date\n");
      exit(1);
    }
    struct stat st;
    if(-1 == lstat(record.path, &st)) {
      printf("[DELETED] -> %s \n", record.path);
      deleted_cnt++;
      continue;
    } 
    
    if( ((uint64_t)st.st_size != record.size) || ( (uint64_t)st.st_mtime != record.mtime ) ) {
      printf("[MODIFIED] -> %s \n", record.path);
      modified_cnt++;
      continue;
    }

    unsigned char current_sha[32];
    calculeaza_sha256(record.path, current_sha);
    if(memcmp(current_sha, record.sha256, 32) != 0) {
      printf("[MODIFIED] -> %s \n", record.path);
      modified_cnt++;
      continue;
    }
    
    printf("[UNMODIFIED] -> %s \n", record.path);
    unmodified_cnt++;
  }
  
  printf("**Statistici verificare bd**\n");
  printf("nr. fisiere sterse : %d\n", deleted_cnt);
  printf("nr. fisiere modificate : %d\n", modified_cnt);
  printf("nr. disiere nemodificate : %d\n", unmodified_cnt);

  if(-1 == fclose(f_db)) {
    printf("Eroare la inchiderea fisierului cu baza de date!\n");
    exit(1);
  }

  printf("Verificare terminata cu succes\n");
}

void mod_db_dump() {
  FILE *f_db = fopen(db_path, "rb");
  if(!f_db) {
    printf("Eroare la deschiderea fisierului cu baza de date!\n");
    exit(1);
  }
  
  db_header header;
  if( 1 != fread(&header, sizeof(db_header), 1, f_db) ) {
    printf("Eroare la citirea headerului din baza de date!\n");
    exit(1);
  }
  
  if(memcmp(header.magic, "INV4", 4) != 0) {
    printf("%s\n", header.magic);
    printf("Baza de date invalida la magic\n");
    exit(1);
  }
  
  printf("magic = %s\n", header.magic);
  printf("version = %" PRIu32 "\n", header.format_version);
  printf("complete = %" PRIu32 "\n", header.flag); //nu dau exit daca e 0.
  printf("file_record_count = %" PRIu32 "\n", header.file_record_count);
  printf("worker_count = %" PRIu64 "\n", header.worker_count);

  if(-1 == fclose(f_db)) {
    printf("Eroare la inchiderea fisierului cu baza de date!\n");
    exit(1);
  }
  
  printf("Dump terminat cu succes!\n");
}


/*
ce afiseaza cand dau run : 
magic = INV4
version = 1
complete = 1
file_record_count = 162
worker_count = 4
Dump terminat cu succes!
*/

int main(int argc, char *argv[]) {
//  printf("Numai bine\n");
  if(argc < 4) {
    print_format_posibile();
    return 1;
  }
  //  0      1    2       3       4 
  // ... --root <dir> --workers <N>
  if(strcmp(argv[1] ,"--root") == 0) {
//    printf("1\n");
    if(argc < 5) {
      print_format_posibile();
      return 1;
    }
    directory = argv[2];
    if(strcmp(argv[3], "--workers") != 0) {
      print_format_posibile();
      return 1;
    } 
    if(1 != sscanf(argv[4], "%d", &n)) {
      printf("Numarul de workeri introdus e gresit !\n");
      return 1;
    }
    if(n < 1) {
      printf("Numarul de workeri trebuie sa fie cel putin 1!\n");
      return 1;
    }
//    printf("%d\n", n);
    max_depth = 1000;
    db_path = "data/inventory.db";
    ipc_path = "data/ipc.mmap";
    graceful_timeout = 1000;
    pid_file = "";
    ms = 0;
    //caut alea suplimentare
    for(int i = 5; i < argc; i++) {
      if(strcmp(argv[i], "--ipc") == 0) {
        if(i + 1 < argc) {
          ipc_path = argv[i + 1];
        }
      } else if(strcmp(argv[i], "--max-depth") == 0) {
         if(i + 1 < argc) {
           max_depth = atoi(argv[i + 1]);
         }
      } else if(strcmp(argv[i], "--simulate-work-ms") == 0) {
        if(i + 1 < argc) {
           ms = atoi(argv[i + 1]);
         }
      } else if(strcmp(argv[i], "--graceful-timeout") == 0) {
        if(i + 1 < argc) {
          graceful_timeout = atoi(argv[i + 1]);
        }
      } else if(strcmp(argv[i], "--pid-file") == 0) {
        if(i + 1 < argc) {
          pid_file = argv[i + 1];
        }
      } else if(strcmp(argv[i], "--db") == 0) {
        if(i + 1 < argc) {
          db_path = argv[i + 1];
        }
      }
    }
    mod_inventariere();
//    printf("graceful timeout : %d\n" , graceful_timeout);
//    printf("pid_file : %s\n", pid_file);
//    printf("simulate work ms : %d", ms);
  } else if(strcmp(argv[1], "--db") == 0) {
    db_path = argv[2];
    if(strcmp(argv[3], "--verify") == 0) {
      printf("%s\n\n", argv[3]);
      mod_db_verify();
    } else if(strcmp(argv[3], "--dump") == 0) {
      mod_db_dump();
    } else {
      print_format_posibile();
      return 1;
    }
  } else {
    print_format_posibile();
    return 1;
  }
  return 0;
}
