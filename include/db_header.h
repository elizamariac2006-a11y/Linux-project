#include <stdint.h>

typedef struct {
  char magic[4];
  uint32_t format_version;
  uint64_t snapshot_id;
  uint8_t snapshot_state; // open = 1, sealed = 0
  uint32_t active_writers;
  uint32_t record_count;
} db_header;

typedef struct {
  char cale[512];
  uint8_t type; //regulat - 0, symlink - 1, fifo - 2  
  uint64_t size;
  uint64_t mtime;
  uint32_t checksum;
  uint64_t st_dev;   
  uint64_t st_ino;
} db_indexer;

typedef struct {
  uint32_t pid;
  uint32_t ppid;
  char comm[512];
  char state;
  char cmdline[512];
  uint64_t utime;
  uint64_t stime;
  uint64_t rss;
} db_proc;
