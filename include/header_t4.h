#include <stdint.h>
#include <semaphore.h>
#include <sys/types.h>
#define MAX_JOBS 1024
#define MAX_PATH 2048
#define MAX_RESULTS 512

typedef struct {
  char path[MAX_PATH];
  uint64_t size;
  uint64_t mtime;
  uint32_t mode;
  uint32_t uid;
  uint32_t gid;
  unsigned char sha256[32];
} file_record;

typedef struct {
  int worker_id;
  pid_t pid;
  int exit_status;
  uint32_t jobs_processed;
  uint32_t files_emitted;
  uint64_t wall_time_ms;
  uint64_t bytes_emitted;
  uint64_t user_cpu_us;
  uint64_t sys_cpu_us;
} worker_stats;

typedef struct {
  char paths[MAX_JOBS][MAX_PATH];
  int head;
  int tail;
  
  sem_t sem_tail_head; //semafor pt a proteja head si tail
  sem_t free_slots_cnt; //cnt cate locuri libere
  sem_t items_available_cnt; //cnt cate itemuri valabile pt workeri
} personal_queue;

typedef struct {
    char magic[4];
    uint32_t version;
    int worker_count;
    int active_jobs; 
    sem_t counter_mutex; //pentru active jobs

    personal_queue queue;

    file_record results[MAX_RESULTS];
    int res_head;
    int res_tail;
    sem_t results_semaphor; //semafor blocare results
    sem_t free_slots_in_results; //semafor cate rez mai pot fi puse 
    sem_t items_avail_in_results; //semafor cate rez sunt gata de citit

    worker_stats stats[256]; 
} mapped_data;

typedef struct {
  char magic[4];
  uint32_t format_version;
  uint32_t flag;
  uint32_t file_record_count;
  uint64_t worker_count;
} db_header;
