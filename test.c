#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <pthread.h>

#define TRUE (1)
#define FALSE (0)

pthread_mutex_t x_lock;
int x = 0;
int is_reserving = FALSE;

void reserve(){
    pthread_mutex_lock(&x_lock);
    is_reserving = TRUE;
    x++;
    is_reserving = FALSE;
    pthread_mutex_unlock(&x_lock);
}

void show(){
    pthread_mutex_lock(&x_lock);
    while(is_reserving == TRUE);
    printf("%d\n", x);
    pthread_mutex_unlock(&x_lock);
}

void *execute_commands(){
    reserve();
    show();
    
    return NULL;
}

int main() {
    const unsigned int MAX_THREADS = 3;

    pthread_mutex_init(&x_lock, NULL);

    pthread_t threads[MAX_THREADS];

    // Create and execute threads
    for(unsigned int i = 0; i < MAX_THREADS; i++){ 
        pthread_create(&threads[i], NULL, &execute_commands, NULL);
    }
    for(unsigned int i = 0; i < MAX_THREADS; i++){
        pthread_join(threads[i], NULL);
    }

    pthread_mutex_destroy(&x_lock);
    return 0;

}
