#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/mman.h>   //shared memory
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>           /* For O_* constants */

#define PERMS 0666
#define SHNAME "shmem"
#define SEMSV "server"
#define MUTX "mutex"
#define NUM_FILES 10
#define NUM_CLIENTS 10
#define L 3

#define ROWS 10
#define MAX_CHAR 128    // maximum amount of characters per line


typedef struct {
    char shm_client[11];    // shared memory name of client
    char file[12];  // file name chosen by client
    int from;
    int to;
    char clientsem[8];   // unique sempaphore for each client
}data;

typedef struct {
    data node[NUM_CLIENTS*L];
    int start;
    int end;
}buffer;

sem_t* mutx;    // mutual exclusion; one process at a time has access to the buffer
// typedef struct mem{
//     data arr;
// }Mem;

void init(buffer *buf);
void send_request(char* shm_name_client, char* clientsem, char* filename, int from, int to, buffer*);
void* read_request(void* shm_serv_ptr);