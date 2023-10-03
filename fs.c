#include "fs.h"
#include <sys/stat.h>


int main(int argc, char* argv[]) {
    if(argc != 4) {
        printf("Please give: number of clients, requests and lambda\n");
       	exit(1);
    }
    int num_clients= atoi(argv[1]);
    int cl_requests= atoi(argv[2]);
    float lambda= atof(argv[3]);    // used for exponential distribution

    //  #######  IN CASE OF EXIT_FAILURE  ########
    sem_unlink(SEMCL);
    sem_unlink(MUTX);
    sem_unlink(SEMSV);
    shm_unlink(SHNAME);
    char nam_[11]="client";
    char s_[8]="sem";
    for(int i= 0; i< num_clients; i++) {
        sprintf(nam_+6, "%d", i);
        shm_unlink(nam_);
        sprintf(s_+3, "%d", i);
        sem_unlink(s_);
    }
    //  ###########################


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
    buffer_consume= sem_open(SEMCL,  O_CREAT | O_EXCL, PERMS, SIZE);   //  client waits for server to free buffer node
    mutx= sem_open(MUTX, O_CREAT | O_EXCL, PERMS, 1);    //  mutual exclusion; one process at a time has access to the buffer


    //  CREATE FS/CLIENTS   //
    pid_t pid;
    int i;
    for(i= 0; i< num_clients; i++) {
        pid= fork();
        if(pid == 0)
            break;
    }

    char file_[]= "fil0.txt";
    char shm_name_cl[11]= "client"; // client****\0
    char sem_cl[8]= "sem";  // sem****\0
    struct timeval sent;
    struct timeval received;
    FILE *f;
    //  ### CLIENTS ###
    if(pid == 0) {
        srand(getpid());
        record rec;
        rec_init(&rec);

        sem_t* client_wait;     // unique semaphore for each client
        sprintf(sem_cl+3, "%d", i);
        if((client_wait= sem_open(sem_cl,  O_CREAT, PERMS, 0))== SEM_FAILED){
            perror("SEM_FAILED");
            exit(EXIT_FAILURE);
        }
        sprintf(shm_name_cl+6, "%d", i);
        char logf[8]= "ans";    // ans****\0
        sprintf(logf+3, "%d", i);
        int j= 0;
        while(j++ < cl_requests) {

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
            
            int file_num= rand()%NUM_FILES; // choose random file name
            file_[3]=file_num+'0';
            sem_wait(buffer_consume);   // wait if buffer is full
            send_request(shm_name_cl, sem_cl, file_, from, to, shm_ptr);
            
            gettimeofday(&sent, NULL);
            sem_post(server_wait);  // notify server of new request

            sem_wait(client_wait);   //  client waits for server to respond to the request
            gettimeofday(&received, NULL);



            if((f= fopen(logf, "a+")) == NULL) {
                perror("fopen error");
                exit(EXIT_FAILURE);
            }
            fprintf(f, "Sent at: %ld    Received at: %ld\n", sent.tv_usec, received.tv_usec);
            fclose(f);

            munmap(shm_ptr_cl,size);
            shm_unlink(shm_name_cl);

            long diff= waited(sent.tv_usec, received.tv_usec);  //  response time
            rec_do(&rec, to-from+1, file_num, diff);
            
            usleep(1-exp(-lambda*j));
        }
        sem_unlink(sem_cl);
        

        if((f= fopen(logf, "a+")) == NULL) {
                perror("fopen error");
                exit(EXIT_FAILURE);
        }
        fprintf(f, "\nTotal lines requested: %d\nTotal files requested: %d\nAverage time response: %ld", rec.lines, rec.files, rec.avg_response_time/cl_requests);
        fclose(f);
        
        exit(0);
    }

    int err;
    pthread_t thr[num_clients*cl_requests]; // thread for each request
    int rec= 0;
    // ###  FILE SERVER  ###
    while(rec< num_clients*cl_requests) {
        sem_wait(server_wait);  // wait if buffer is empty
        if(err= pthread_create(&thr[rec], NULL, read_request, (void*) shm_ptr)) {
            perror("pthread_create");
            exit(1);
        }

        rec++;
    }

    for(i= 0; i < num_clients*cl_requests; i++) {
        if(err=pthread_join(thr[i],NULL)) {
            perror("pthread_join");
            exit(1);
        }
    }

    printf("COMPLETE\n");
    
    if(shm_unlink(SHNAME) == -1)
        perror("shmem unlink failed");
    if(sem_unlink(SEMSV) < 0)
        perror("server sem unlink failed");
    if(sem_unlink(MUTX) == -1)
        perror("mutx sem unlink failed");
    sem_unlink(SEMCL);

    return 0;
}