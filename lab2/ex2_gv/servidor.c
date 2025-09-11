#include <stdio.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <unistd.h>

int main(void) {
    FILE* f = fopen("msg.txt", "r");
    if (f != NULL) {
        fseek(f, 0L, SEEK_END); 

        int id = 8752;
        // printf("%ld\n", ftell(f)); 16 BYTES
        int shm = shmget(id, ftell(f), IPC_CREAT | 0666);
        char* page = (char*)shmat(shm, NULL, 0);

        fseek(f, 0L, SEEK_SET);
        
        char* buffer_aux = page;
        char c;
        while ((c = fgetc(f)) != EOF) {
            *buffer_aux = c;
            buffer_aux++;
        }

        printf("servidor escreveu: %s\n", page);
        shmdt(page);
        shmctl(id, IPC_RMID, 0);
        fclose(f);
        if (execlp("./cliente", "", (char*)NULL) == -1) {
            perror("Error executing execlp");
            exit(1); // Return a non-zero value to indicate an error
        }        
    }

    return 0;
}