#include "fs.h"

static void func() {
    printf("\nCALLED FUNC\n");
}

void init(buffer *buf) {
    buf->start= 0;
    buf->end= -1;

    func();
}

void send_request(char* shm_name_client, char *sem, char* filename, int from, int to, buffer* shm_ptr) {
    sem_wait(mutx);
        shm_ptr->end= (shm_ptr->end+1) % (NUM_CLIENTS*L);
        int end= shm_ptr->end;
    sem_post(mutx);

    strcpy(shm_ptr->node[end].shm_client, shm_name_client); //client name
    strcpy(shm_ptr->node[end].file, filename);
    strcpy(shm_ptr->node[end].clientsem, sem);
    shm_ptr->node[end].from= from;
    shm_ptr->node[end].to= to;
}

void* read_request(void* arg) {
    buffer* shm_ptr= (buffer*)arg;

    sem_wait(mutx);
        data request= shm_ptr->node[shm_ptr->start];
        shm_ptr->start= (shm_ptr->start+1) % (NUM_CLIENTS*L);
    sem_post(mutx);

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

    FILE* f= fopen(request.file, "r");
    char line[128];     // dummy array
    do {                                // Manually set the file pointer to the beginning of the first line, since each line
        fgets(line, 129, f);            // has a different amount of characters (max 128)
    }while(--from >1);

    do {
        fgets(clientptr+strlen(clientptr), 129, f);
    }while(--total_lines);
    
    sem_t* semcl= sem_open(request.clientsem, O_CREAT);
    sem_post(semcl);    // tells client the request has been completed


    fclose(f);
    munmap(request.shm_client, size);
    pthread_exit(NULL);
}