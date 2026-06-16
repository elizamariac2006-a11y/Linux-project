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
#include<stdbool.h>

bool find_record(int fd_old, const char* cale, db_indexer* record, int numberOfRecords) {
  lseek(fd_old, sizeof(db_header) ,SEEK_SET);
  for(int i = 0; i < numberOfRecords; i++) {
    db_indexer aux;
    read(fd_old, &aux, sizeof(db_indexer));
    if(strcmp(cale, aux.cale) == 0) {
      *record = aux;
      return true;
    } 
  }
  return false;
}

void diff_file(int fd_new, db_header h_new, int fd_old, db_header h_old, FILE* out) {
  fprintf(out, "**Raport pentru fisiere**\n");

  //ma pozitionez la primul record
  lseek(fd_new, sizeof(db_header), SEEK_SET);
  int numberOfRecords = h_new.record_count;
  for(int i = 0; i < numberOfRecords; i++) {
    db_indexer record_in_new, record_in_old;
    
    //iau din new, vad ce e in old
    read(fd_new, &record_in_new, sizeof(db_indexer));
    if(!find_record(fd_old, record_in_new.cale, &record_in_old, h_old.record_count)) {
      fprintf(out, "New -> %s\n", record_in_new.cale);
    } else {
      if(record_in_new.type != record_in_old.type || record_in_new.size != record_in_old.size ||
        record_in_new.mtime != record_in_old.mtime || record_in_new.checksum != record_in_old.checksum ||
          record_in_new.st_dev != record_in_old.st_dev || record_in_new.st_ino != record_in_old.st_ino) {
        fprintf(out, "Modified -> %s\n", record_in_new.cale);
      } else {
         fprintf(out, "Old -> %s\n", record_in_new.cale);
      }
    }
  }

  lseek(fd_old, sizeof(db_header), SEEK_SET);
  numberOfRecords = h_old.record_count;
  for(int i = 0; i < numberOfRecords; i++) {
    db_indexer record_in_new, record_in_old;
    read(fd_old, &record_in_old, sizeof(db_indexer));
    
    if(!find_record(fd_new, record_in_old.cale, &record_in_new, h_new.record_count)) {
      fprintf(out, "Deleted -> %s\n", record_in_old.cale);
    }
  }
}

bool find_proc(int fd_old, int pid, db_proc* record, int numberOfRecords) {
  lseek(fd_old, sizeof(db_header), SEEK_SET);
  for(int i = 0; i < numberOfRecords; i++) {
    db_proc aux;
    read(fd_old, &aux, sizeof(db_proc));
    if(aux.pid == pid) {
      *record = aux;
      return true;
    }
  }
  return false;
}

void diff_proc(int fd_new, db_header h_new, int fd_old, db_header h_old, FILE* out) {
  fprintf(out, "**Raport procese**\n");
  
  lseek(fd_new, sizeof(db_header), SEEK_SET);
  int numberOfRecords = h_new.record_count;
  for(int i = 0; i < numberOfRecords; i++) {
    db_proc record_in_new, record_in_old;
    read(fd_new, &record_in_new, sizeof(db_proc));
    
    if(!find_proc(fd_old, record_in_new.pid, &record_in_old, h_old.record_count)) {
      fprintf(out, "New -> %d\n", record_in_new.pid);
    } else {
      if(record_in_new.rss != record_in_old.rss) {
        int difference = (abs)(record_in_new.rss - record_in_old.rss);
        if(difference > 100000) {
          fprintf(out, "Modified significantly -> %d\n", record_in_new.pid);
        } else {
          fprintf(out, "Modified -> %d\n", record_in_new.pid);
        }
      } else if(record_in_new.ppid != record_in_old.ppid) {
        fprintf(out, "Modified -> %d\n", record_in_new.pid);
      } else {
        fprintf(out, "Old -> %d\n", record_in_new.pid);
      }
    }
  }

  lseek(fd_old, sizeof(db_header), SEEK_SET);
  numberOfRecords = h_old.record_count;
  for(int i = 0; i < numberOfRecords; i++) {
    db_proc record_in_new, record_in_old;
    read(fd_old, &record_in_old, sizeof(db_proc));
    if(!find_proc(fd_new, record_in_old.pid, &record_in_new, h_new.record_count)) {
      fprintf(out, "Deleted -> %d\n", record_in_old.pid);
    }
  }
}

int main(int argc, char *argv[]) {
//    printf("Buna siua");
  // 0      1    2     3     4      5    6
  // ..  --old <db1> --new <db2> --out <path>
  if(argc < 7) {
    printf("Argumete insuficiente!\n");
    return 1;
  } 
  if(strcmp(argv[1], "--old") != 0) {
    printf("Trebuie rulat cu : ./tools/fileops.sh run -- db_diff --old <db1> --new <db2> --out <path> !!\n");
    return 1;
  }
  char* old = argv[2];
  if(strcmp(argv[3], "--new") != 0) {
    printf("Trebuie rulat cu : ./tools/fileops.sh run -- db_diff --old <db1> --new <db2> --out <path> !!\n");
    return 1;
  }
  char* new = argv[4];
  if(strcmp(argv[5], "--out") != 0) {
    printf("Trebuie rulat cu formatul : ./tools/fileops.sh run -- db_diff --old <db1> --new <db2> --out <path>\n");
    return 1;
  }
  char* out = argv[6];

  if( !new || !old  || !out ) {
    printf("Trebuie rulat cu formatul : ./tools/fileops.sh run -- db_diff --old <db1> --new <db2> --out <path>\n");
    return 1;
  } 

  int fd_new = open(new, O_RDONLY);
  int fd_old = open(old, O_RDONLY);
  FILE* f_out = fopen(out, "w");
  
  db_header h_old, h_new;
  read(fd_old, &h_old, sizeof(db_header));
  read(fd_new, &h_new, sizeof(db_header));
  
  if(memcmp(h_old.magic, h_new.magic, 4) != 0) {
    printf("La magic sunt diferite !\n");
    printf("%s *** %s", h_old.magic, h_new.magic);
    return 1;
  }
  
  if(h_old.format_version != h_new.format_version) {
    printf("La format version sunt diferite !\n");
    return 1;
  }
  
  if(memcmp(h_old.magic, "IDX1", 4) == 0) {
    diff_file(fd_new, h_new, fd_old, h_old, f_out);
  }  
  
  if(memcmp(h_old.magic, "PRC1", 4) == 0) {
    diff_proc(fd_new, h_new, fd_old, h_old, f_out);
  } 

  return 0;
}
