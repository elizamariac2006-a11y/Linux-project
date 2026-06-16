#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include "../include/db_header.h" 
#include <string.h>

int set_lock(int fd, int type, off_t start, off_t len) {
  struct flock lock;

  lock.l_type = type;
  lock.l_whence = SEEK_SET;
  lock.l_start = start;
  lock.l_len = len;

  return fcntl(fd, F_SETLKW, &lock);
}

uint32_t calculeaza_checksum(const char *path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return 0;
    
    uint32_t sum = 0;
    unsigned char buffer[4096];
    ssize_t nr;
    while ((nr = read(fd, buffer, sizeof(buffer))) > 0) {
        for (ssize_t i = 0; i < nr; i++) {
            sum += buffer[i];
        }
    }
    close(fd);
    return sum;
}

void recursiv(const char* path, int db_fd) {
 
    DIR *d = opendir(path);
    if (!d) return;

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char full_path[512];
  snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        struct stat st;
        if (lstat(full_path, &st) == -1) continue;

        // structura baza de date
        db_indexer record;
        memset(&record, 0, sizeof(db_indexer));
        strncpy(record.cale, full_path, sizeof(record.cale) - 1);
        record.size = S_ISREG(st.st_mode) ? (uint64_t)st.st_size : 0;
        record.mtime = (uint64_t)st.st_mtime;
        record.st_dev = (uint64_t)st.st_dev;
        record.st_ino = (uint64_t)st.st_ino;
                //tip structura
        if (S_ISREG(st.st_mode)) {
            record.type = 0; 
            record.checksum = calculeaza_checksum(full_path);
        } else if (S_ISLNK(st.st_mode)) {
            record.type = 1; 
            record.checksum = 0;
        } else if (S_ISFIFO(st.st_mode)) {
            record.type = 2; 
            record.checksum = 0;
        } else if (S_ISDIR(st.st_mode)) {
            record.type = 3; 
            record.checksum = 0;
        }

        int found = 0;
        db_header current_head;

        set_lock(db_fd, F_RDLCK, 0, sizeof(db_header));
        lseek(db_fd, 0, SEEK_SET);
        read(db_fd, &current_head, sizeof(db_header));
        set_lock(db_fd, F_UNLCK, 0, sizeof(db_header));

        for (uint32_t i = 0; i < current_head.record_count; i++) {
            db_indexer existing;
            off_t offset = sizeof(db_header) + (i * sizeof(db_indexer));

            lseek(db_fd, offset, SEEK_SET);
 read(db_fd, &existing, sizeof(db_indexer));

            if (strcmp(existing.cale, record.cale) == 0) {
                set_lock(db_fd, F_WRLCK, offset, sizeof(db_indexer));
                lseek(db_fd, offset, SEEK_SET);
                write(db_fd, &record, sizeof(db_indexer));
                set_lock(db_fd, F_UNLCK, offset, sizeof(db_indexer));
                found = 1;
                break;
            }
        }

        if (!found) {
            set_lock(db_fd, F_WRLCK, 0, sizeof(db_header)); 
            lseek(db_fd, 0, SEEK_SET);
            read(db_fd, &current_head, sizeof(db_header));

            off_t new_offset = sizeof(db_header) + (current_head.record_count * sizeof(db_indexer));

            set_lock(db_fd, F_WRLCK, new_offset, sizeof(db_indexer));
            lseek(db_fd, new_offset, SEEK_SET);
            write(db_fd, &record, sizeof(db_indexer));
            set_lock(db_fd, F_UNLCK, new_offset, sizeof(db_indexer));

            current_head.record_count++;
            lseek(db_fd, 0, SEEK_SET);
            write(db_fd, &current_head, sizeof(db_header));
            set_lock(db_fd, F_UNLCK, 0, sizeof(db_header));
        }

        if (S_ISDIR(st.st_mode) && !S_ISLNK(st.st_mode)) 
            recursiv(full_path, db_fd);

    }
    closedir(d);
}



int main(int argc, char *argv[]) {
  char* directory = NULL;
  char* db_path = "data/index.db";
  
  if(argc > 1) {
for(int i = 1; i < argc; i++) {
      if(strcmp(argv[i], "--root") == 0) {
        if(i == argc - 1) {
          printf("Eroare !");
          return 1;
        } else {
          directory = argv[i + 1];
          i++;
        }
      }
      if(strcmp(argv[i], "--db") == 0) {
        if(i != argc - 1) {
          db_path = argv[i + 1];
          i++;
        }
      }
    }
  }

  if( directory == NULL ) {
    return 1;
  }
  
  int fd = open(db_path, O_CREAT | O_RDWR, 0777);
  set_lock(fd, F_WRLCK, 0, sizeof(db_header)); //am pus lacatu peste header ,
  //primii octeti din fisier
  
  db_header head;
  ssize_t r = read(fd, &head, sizeof(db_header));
  
  // open 1 sealed 0
  //empty sauu new
  if( r == sizeof(db_header) && head.snapshot_state == 0) {
    printf("Nu se mai poate modifica nimic (sealed). Opresc programul\n");
    close(fd);
    return 1;
  }

  if(r < sizeof(db_header) || head.snapshot_state == 0) {
    lseek(fd, 0, SEEK_SET);
    ftruncate(fd, 0); 

    memcpy(head.magic, "IDX1", 4);
    head.format_version = 1;
    head.snapshot_id = time(NULL);
    head.snapshot_state = 1;
    head.active_writers = 1;
 head.record_count = 0;
  } else {
    head.active_writers++;
  }

  lseek(fd, 0, SEEK_SET);
  write(fd, &head, sizeof(db_header)); //baza de date incepe cu headerul
  
  set_lock(fd, F_UNLCK, 0, sizeof(db_header));

  sleep(3);

  recursiv(directory, fd);

  //reblochez headerul pt a scadea nr de scriirori
  set_lock(fd, F_WRLCK, 0, sizeof(db_header));
  lseek(fd, 0, SEEK_SET);
  read(fd, &head, sizeof(db_header));

  head.active_writers--;
  if (head.active_writers == 0) {
      head.snapshot_state = 0; //a fost ultimul scriitor
  }

  lseek(fd, 0, SEEK_SET);
  write(fd, &head, sizeof(db_header));
  set_lock(fd, F_UNLCK, 0, sizeof(db_header));
  
  close(fd);
  return 0;
}
