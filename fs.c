#include "fs.h"
#include <sys/stat.h>


int main(void) {
    sem_unlink(MUTX);
    sem_unlink(SEMSV);
    shm_unlink(SHNAME);
    char nam_[11]="client";
    char s_[8]= "sem";
    for(int i= 0; i< NUM_CLIENTS; i++) {
        sprintf(nam_+6, "%d", i);
        shm_unlink(nam_);
        sprintf(s_+3, "%d", i);
        sem_unlink(s_);
    }
    func();

    // PREPARATIONS //
    int mem_fd;
    if( (mem_fd= shm_open(SHNAME, O_CREAT | O_EXCL | O_RDWR, PERMS)) == -1) {
        perror("shm_open error");
        exit(EXIT_FAILURE);
    }

    if (ftruncate(mem_fd, sizeof(buffer))== -1) {
        perror("ftruncate_error");
        exit(EXIT_FAILURE);
    }

    buffer* shm_ptr;
    shm_ptr= mmap(NULL, sizeof(buffer), PROT_WRITE, MAP_SHARED, mem_fd, 0);
    init(shm_ptr);

    sem_t* server_wait= sem_open(SEMSV,  O_CREAT | O_EXCL, PERMS, 0);   //  server waits for client to send request
    mutx= sem_open(MUTX, O_CREAT | O_EXCL, PERMS, 1);    //  mutual exclusion; one process at a time has access to the buffer


    //  CREATE FS/CLIENTS   //
    pid_t pid;
    int i;
    for(i= 0; i< NUM_CLIENTS; i++) {
        pid= fork();
        if(pid == 0)
            break;
    }

    char file_[]= "fil0.txt";
    char shm_name_cl[11]= "client"; // client****\0
    char sem_cl[8]= "sem";  // sem****\0
    if(pid == 0) {
        srand(getpid());
        
        sem_t* client_wait;
        sprintf(sem_cl+3, "%d", i);
        if((client_wait= sem_open(sem_cl,  O_CREAT, PERMS, 0))== SEM_FAILED){   //  client waits for server to respond to request
            perror("SEM_FAILED");
            exit(EXIT_FAILURE);
        }
        sprintf(shm_name_cl+6, "%d", i);
        char logf[8]= "ans";    // ans****\0
        sprintf(logf+3, "%d", i);
        int j= 0;
        while(j++ < L) {
            int from= rand()%ROWS+1;    // first row
            int to= rand()%(ROWS-from+1)  + from;   // last row

            // temporary client shared memory
            int cl_fd;
            if( (cl_fd= shm_open(shm_name_cl, O_CREAT | O_EXCL | O_RDWR, PERMS)) == -1) {
                perror("client shm_open error");
                exit(EXIT_FAILURE);
            }

            int size= (to-from+1)*MAX_CHAR;
            if (ftruncate(cl_fd, size)== -1) {
                perror("client ftruncate error");
                exit(EXIT_FAILURE);
            }
            char* shm_ptr_cl;
            if((shm_ptr_cl= mmap(NULL, size, PROT_WRITE, MAP_SHARED, cl_fd, 0)) == MAP_FAILED) {
                perror("MAP_FAILED");
                exit(EXIT_FAILURE);
            }
            
            file_[3]=rand()%NUM_FILES+'0';
            send_request(shm_name_cl, sem_cl, file_, from, to, shm_ptr);
            sem_post(server_wait);  // notify server of new request

            sem_wait(client_wait);  // unique sem

            printf("(%d_%d) RECIEVED %s:(%d,%d)\n", i, j, file_, from, to);

            FILE *f;
            if((f= fopen(logf, "w+")) == NULL) {
                perror("fopen error");
                exit(EXIT_FAILURE);
            }
            fprintf(f, "%s", shm_ptr_cl);

            fclose(f);
            munmap(shm_ptr_cl,size);
            shm_unlink(shm_name_cl);

            // usleep(20000);
        }
        sem_unlink(sem_cl);
        exit(0);
    }

    int err;
    pthread_t thr[NUM_CLIENTS*L];
    int rec= 0;
    while(rec< NUM_CLIENTS*L) {
        sem_wait(server_wait);  // wait if buffer is empty
        if(err= pthread_create(&thr[rec], NULL, read_request, (void*) shm_ptr)) {
            perror("pthread_create");
            exit(1);
        }

        rec++;
    }

    for(i= 0; i < NUM_CLIENTS*L; i++) {
        if(err=pthread_join(thr[i],NULL)) {
            perror("pthread_join");
            exit(1);
        }
    }

    printf("COMPLETE\n");
    
    shm_unlink(SHNAME);
    if(sem_unlink(SEMSV) < 0)
        perror("sem unlink failed");
    sem_unlink(MUTX);

    return 0;
}