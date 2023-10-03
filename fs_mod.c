#include "fs.h"

void init(buffer *buf) {
    buf->start= 0;
    buf->end= -1;
}

void rec_init(record* rec) {
    rec->files= 0;
    rec->lines= 0;
    rec->avg_response_time= 0;
    for(int i= 0; i< 10; i++)
        rec->unique_files[i]= 0;
}

void send_request(char* shm_name_client, char *sem, char* filename, int from, int to, buffer* shm_ptr) {
    sem_wait(mutx);
        shm_ptr->end= (shm_ptr->end+1) % (SIZE);
        int end= shm_ptr->end;
    sem_post(mutx);

    strcpy(shm_ptr->node[end].shm_client, shm_name_client);
    strcpy(shm_ptr->node[end].file, filename);
    strcpy(shm_ptr->node[end].clientsem, sem);
    shm_ptr->node[end].from= from;
    shm_ptr->node[end].to= to;
}

void* read_request(void* arg) {
    buffer* shm_ptr= (buffer*)arg;

    sem_wait(mutx);
        data request= shm_ptr->node[shm_ptr->start];
        shm_ptr->start= (shm_ptr->start+1) % (SIZE);
    sem_post(mutx);
    sem_post(buffer_consume);   // previous start position is available for any new request

    int fd;
    if( (fd= shm_open(request.shm_client, O_RDWR, PERMS)) == -1) {
        perror("read_request: shm_open error");
        exit(EXIT_FAILURE);
    }
    int from= request.from;
    int to= request.to;
    
    char* clientptr;
    int total_lines= to-from+1;
    if((clientptr= mmap(NULL, total_lines*MAX_CHAR, PROT_WRITE, MAP_SHARED, fd, 0)) == MAP_FAILED) {
        perror("MAP_FAILED");
        exit(EXIT_FAILURE);
    }
    int size= total_lines*MAX_CHAR;

    FILE* f;
    if((f= fopen(request.file, "r"))== NULL) {
        perror("read_request: fopen error");
        exit(EXIT_FAILURE);
    }
    char line[128];     // dummy array
    do {                                // Manually set the file pointer to the beginning of the first line, since each line
        fgets(line, 129, f);            // has a different amount of characters (max 128)
    }while(--from >1);

    do {
        fgets(clientptr+strlen(clientptr), 129, f); // each time add the new text segment at the end of clientptr
    }while(--total_lines);
    
    sem_t* semcl;
    if((semcl= sem_open(request.clientsem, O_CREAT)) == SEM_FAILED) {
        perror("read_request: sem_open error");
        exit(EXIT_FAILURE);
    }

    sem_post(semcl);    // tells client the request has been completed
    if(sem_close(semcl) == -1) {
        perror("read_request: sem_close error");
        exit(EXIT_FAILURE);
    }

    fclose(f);
    munmap(request.shm_client, size);
    pthread_exit(NULL);
}

void rec_do(record* rec, int lines, int file, long int waited) {
    rec->lines+= lines;
    if(rec->unique_files[file] == 0) {
        rec->unique_files[file]= 1;
        rec->files++;
    }
    rec->avg_response_time+= waited;
}

long waited(long sent_at, long received_at) {
    if(sent_at > received_at)     //  received.tv_usec has reached past 10^6, so it cycles back
        return 1000000- sent_at + received_at;
    else {
        long diff= received_at- sent_at;
        if(diff < 0)
            diff*=-1;   // absolute difference
        return diff;
    }
}