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

#include "constants.h"
#include "operations.h"
#include "parser.h"
#include "filehandler.h"

#define TRUE (1)
#define FALSE (0)

#define ALL (-1)
#define NONE (-2)

typedef struct {
  int fd_jobs;
  int fd_out;
  pthread_mutex_t read_lock;
  pthread_mutex_t data_lock;
  pthread_mutex_t wait_lock;
  unsigned int *wait;
  unsigned int MAX_THREADS;
} thread_args;

thread_args t_args;

void *execute_commands(void *arg){
  unsigned int event_id, delay, thread_id;
  size_t num_rows, num_columns, num_coords;
  size_t xs[MAX_RESERVATION_SIZE], ys[MAX_RESERVATION_SIZE];

  // Thread ID
  unsigned int id = *(unsigned int *)arg;

  while(1){
    pthread_mutex_lock(&(t_args.wait_lock));
    if(t_args.wait[id - 1] != 0){ //pararem todos ao mesmo tempo?
      printf("Waiting...\n");
      ems_wait(t_args.wait[id - 1]);
      t_args.wait[id-1] = 0;
    }
    pthread_mutex_unlock(&(t_args.wait_lock));

    pthread_mutex_lock(&(t_args.read_lock));
    switch (get_next(t_args.fd_jobs)) {
      case CMD_CREATE:
        if (parse_create(t_args.fd_jobs, &event_id, &num_rows, &num_columns) != 0) {
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          pthread_mutex_unlock(&(t_args.read_lock));
          continue;
        }
        pthread_mutex_unlock(&(t_args.read_lock));
        pthread_mutex_lock(&(t_args.data_lock));
        if (ems_create(event_id, num_rows, num_columns)) {
          fprintf(stderr, "Failed to create event\n");
          pthread_mutex_unlock(&(t_args.data_lock));
        }
        pthread_mutex_unlock(&(t_args.data_lock));
        break;

      case CMD_RESERVE:
        num_coords = parse_reserve(t_args.fd_jobs, MAX_RESERVATION_SIZE, &event_id, xs, ys);
        if (num_coords == 0) {
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          pthread_mutex_unlock(&(t_args.read_lock));
          continue;
        }
        pthread_mutex_unlock(&(t_args.read_lock));
        pthread_mutex_lock(&(t_args.data_lock));
        if (ems_reserve(event_id, num_coords, xs, ys)) {
          fprintf(stderr, "Failed to reserve seats\n");
          pthread_mutex_unlock(&(t_args.data_lock));
        }
        pthread_mutex_unlock(&(t_args.data_lock));
        break;

      case CMD_SHOW:
        
        if (parse_show(t_args.fd_jobs, &event_id) != 0) {
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          pthread_mutex_unlock(&(t_args.read_lock));
          continue;
        }
        pthread_mutex_unlock(&(t_args.read_lock));
        pthread_mutex_lock(&(t_args.data_lock));
        if (ems_show(event_id, t_args.fd_out)) {
          fprintf(stderr, "Failed to show event\n");
          pthread_mutex_unlock(&(t_args.data_lock));
        }
        pthread_mutex_unlock(&(t_args.data_lock));
        break;

      case CMD_LIST_EVENTS:
        pthread_mutex_unlock(&(t_args.read_lock));
        pthread_mutex_lock(&(t_args.data_lock));
        if (ems_list_events(t_args.fd_out)) {
          fprintf(stderr, "Failed to list events\n");
          pthread_mutex_unlock(&(t_args.data_lock));
        }
        pthread_mutex_unlock(&(t_args.data_lock));
        break;

      case CMD_WAIT:
        int wait_ret = parse_wait(t_args.fd_jobs, &delay, &thread_id);
        pthread_mutex_unlock(&(t_args.read_lock));

        if (wait_ret == -1) {
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          continue;
        }
        else if(wait_ret == 0){
          if(delay > 0){
            pthread_mutex_lock(&(t_args.wait_lock));
            for(unsigned int i = 0; i < t_args.MAX_THREADS; i++){
              t_args.wait[i] = delay;
            }
            pthread_mutex_unlock(&(t_args.wait_lock));
          }
        }
        else if(wait_ret == 1){
          if(thread_id <= t_args.MAX_THREADS){
            if(delay > 0){
              pthread_mutex_lock(&(t_args.wait_lock));
              t_args.wait[thread_id - 1] = delay;
              pthread_mutex_unlock(&(t_args.wait_lock));
            }
          }
          else{
            fprintf(stderr, "Invalid thread_id\n");
            continue;
          }
        }
        
        break;

      case CMD_INVALID:
        pthread_mutex_unlock(&(t_args.read_lock));
        fprintf(stderr, "Invalid command. See HELP for usage\n");
        break;

      case CMD_HELP:
        pthread_mutex_unlock(&(t_args.read_lock));
        printf(
            "Available commands:\n"
            "  CREATE <event_id> <num_rows> <num_columns>\n"
            "  RESERVE <event_id> [(<x1>,<y1>) (<x2>,<y2>) ...]\n"
            "  SHOW <event_id>\n"
            "  LIST\n"
            "  WAIT <delay_ms> [thread_id]\n"
            "  BARRIER\n"                      // Not implemented
            "  HELP\n");
        break;

      case CMD_BARRIER:  // Not implemented
        pthread_mutex_unlock(&(t_args.read_lock));
        break;
      case CMD_EMPTY:
        pthread_mutex_unlock(&(t_args.read_lock));
        break;

      case EOC:
        pthread_mutex_unlock(&(t_args.read_lock));
        return NULL;
        break;
    }
  }
  return NULL;
}


int main(int argc, char *argv[]) {
  unsigned int state_access_delay_ms = STATE_ACCESS_DELAY_MS;

  if(argc < 4){
    fprintf(stderr, "Insufficient arguments\n");
    return 1;
  }

  const unsigned int MAX_PROC = (unsigned int)atoi(argv[2]);
  t_args.MAX_THREADS = (unsigned int)atoi(argv[3]);

  // Set delay
  if (argc == 5) {
    char *endptr;
    unsigned long int delay = strtoul(argv[4], &endptr, 10);

    if (*endptr != '\0' || delay > UINT_MAX) {
      fprintf(stderr, "Invalid delay value or value too large\n");
      return 1;
    }

    state_access_delay_ms = (unsigned int)delay;
  }

  // Initialize EMS
  if (ems_init(state_access_delay_ms)) {
    fprintf(stderr, "Failed to initialize EMS\n");
    return 1;
  }
  
  // Add forward slash to directory path
  if(argv[1][strlen(argv[1])-1] != '/'){
    strcat(argv[1], "/");
  }

  // Open directory
  DIR *dir = opendir(argv[1]);
  if(dir == NULL){
    fprintf(stderr, "Failed to open directory.\n");
    return 1;
  }
  struct dirent *entry;

  unsigned int num_active_proc = 0; 

  while ((entry = readdir(dir))!= NULL) {
    if(strstr(entry->d_name, ".jobs") != NULL){
      // Wait for child processes to terminate
      if(num_active_proc >= MAX_PROC){
        int status;
        pid_t child_pid = wait(&status);

        if(WIFEXITED(status)){
          if(WEXITSTATUS(status) == -1){
              fprintf(stderr,"Error while terminating child processor\n");
              return 1;
          }
          // Print child termination status
          fprintf(stdout, "Child process %d terminated with status %d\n", child_pid, status);
          num_active_proc--;
        }
        else{
            fprintf(stderr, "Failed to terminate child processor\n");
            return 1;
        }
      }
      
      num_active_proc++;
      pid_t pid = fork();

      if(pid < 0){
        fprintf(stderr, "Failed to create process.\n");
        return 1;
      }
      else if(pid == 0){ // Child process
        
        // Open .jobs file and create respective .out file
        if(open_file(argv[1], entry->d_name, &(t_args.fd_jobs), &(t_args.fd_out)) < 0){
          fprintf(stderr, "Failed to open file.\n");
          return 1;
        }

        pthread_mutex_init(&(t_args.read_lock), NULL);
        pthread_mutex_init(&(t_args.data_lock), NULL);
        pthread_mutex_init(&(t_args.wait_lock), NULL);
        
        pthread_t threads[t_args.MAX_THREADS];
        unsigned int thread_ids[t_args.MAX_THREADS];

        t_args.wait = (unsigned int *)malloc(t_args.MAX_THREADS * sizeof(unsigned int));
        for(unsigned int i = 0; i < t_args.MAX_THREADS; i++){
          t_args.wait[i] = 0;
        }

        // Create and execute threads
        for(unsigned int i = 0; i < t_args.MAX_THREADS; i++){
          thread_ids[i] = i+1;
          pthread_create(&threads[i], NULL, &execute_commands, (void *)&thread_ids[i]);
        }
        for(unsigned int i = 0; i < t_args.MAX_THREADS; i++){
          pthread_join(threads[i], NULL);
        }

        close(t_args.fd_jobs);
        close(t_args.fd_out);
        pthread_mutex_destroy(&(t_args.read_lock));
        pthread_mutex_destroy(&(t_args.data_lock));
        pthread_mutex_destroy(&(t_args.wait_lock));
        free(t_args.wait);

        exit(0);
      }
    }
  }

  // Wait for remaining child processes to terminate
  for(unsigned int i = 0; i < MAX_PROC; i++){
    int status;
    pid_t child_pid = wait(&status);

    if(WIFEXITED(status)){
      if(WEXITSTATUS(status) == -1){
          fprintf(stderr,"Error while terminating child processor\n");
          return 1;
      }
      // Print child termination status
      fprintf(stdout, "Child process %d terminated with status %d\n", child_pid, status);
      num_active_proc--;
    }
    else{
        fprintf(stderr, "Failed to terminate child processor\n");
        return 1;
    }
  }
  closedir(dir);
  ems_terminate();
}
