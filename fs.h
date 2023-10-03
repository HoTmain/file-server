#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/mman.h>   //shared memory
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/types.h>
#include <fcntl.h>           /* For O_* constants */
#include <math.h>   // for exp()

#define PERMS 0666
#define SHNAME "shmem"
#define SEMSV "server"
#define SEMCL "semcl"
#define MUTX "mutex"
#define NUM_FILES 10
#define SIZE 64     // total data nodes of buffer

#define ROWS 10     // total rows per file
#define MAX_CHAR 128    // maximum amount of characters per line


typedef struct {
    char shm_client[11];    // shared memory name of client
    char file[12];  // file name chosen by client
    int from;
    int to;
    char clientsem[8];   // unique sempaphore for each client
}data;

typedef struct {
    data node[SIZE];
    int start;
    int end;
}buffer;    //  Queue

typedef struct{
    int lines;
    int files;
    long int avg_response_time;
    int unique_files[10];   // keep track of which files client has selected so far
}record;

sem_t* mutx;    // mutual exclusion; one process at a time has access to the buffer
sem_t* buffer_consume;

void init(buffer*); // initialize buffer
void rec_init(record*); // initialize record
void send_request(char* shm_name_client, char* clientsem, char* filename, int from, int to, buffer*);
void* read_request(void* shm_serv_ptr);
void rec_do(record*, int lines, int file, long int waited); // record client statistics to file
long waited(long sent_at, long received_at);
