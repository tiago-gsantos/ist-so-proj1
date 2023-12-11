#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "constants.h"
#include "operations.h"
#include "parser.h"

int main(int argc, char *argv[]) {
  unsigned int state_access_delay_ms = STATE_ACCESS_DELAY_MS;

  const unsigned int MAX_PROC = (unsigned int)atoi(argv[2]);

  if(argc < 1){
    fprintf(stderr, "Too few arguments\n");
    return 1;
  }

  // Set delay
  if (argc == 4) {
    char *endptr;
    unsigned long int delay = strtoul(argv[2], &endptr, 10);

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

  size_t dir_path_len = strlen(argv[1]);
  
  // Add forward slash to directory path
  if(argv[1][dir_path_len-1] != '/'){
    strcat(argv[1], "/");
    dir_path_len++;
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
      if(num_active_proc >= MAX_PROC){
        int status;
        pid_t child_pid = wait(&status);

        if(WIFEXITED(status)){
          if(WEXITSTATUS(status) == -1){
              fprintf(stderr,"Error while terminating child processor\n");
              return -1;
          }
          
          fprintf(stdout, "Child process %d terminated with status %d\n", child_pid, status);

          num_active_proc--;
        }
        else{
            fprintf(stderr, "Failed to terminate child processor\n");
            return -1;
        }
      }
      
      num_active_proc++;
      pid_t pid = fork();

      if(pid < 0){
        fprintf(stderr, "Failed to create process.\n");
        return 1;
      }
      else if(pid == 0){
        unsigned int event_id, delay;
        size_t num_rows, num_columns, num_coords;
        size_t xs[MAX_RESERVATION_SIZE], ys[MAX_RESERVATION_SIZE];
        
        size_t path_len = dir_path_len + strlen(entry->d_name) + 1;
        char file_path[path_len];

        strcpy(file_path, argv[1]);
        strcat(file_path, entry->d_name);

        int fdjobs = open(file_path, O_RDONLY);
        if(fdjobs < 0){
          fprintf(stderr, "Failed to open file.\n");
          return 1;
        }  

        file_path[path_len - 6] = '\0';
        strcat(file_path, ".out");

        int fdout = open(file_path, O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR);
        if(fdout < 0){
          fprintf(stderr, "Failed to open file.\n");
          return 1;
        }  

        int fileEnded = 0;
        while(1){
          switch (get_next(fdjobs)) {
            case CMD_CREATE:
              if (parse_create(fdjobs, &event_id, &num_rows, &num_columns) != 0) {
                fprintf(stderr, "Invalid command. See HELP for usage\n");
                continue;
              }

              if (ems_create(event_id, num_rows, num_columns)) {
                fprintf(stderr, "Failed to create event\n");
              }

              break;

            case CMD_RESERVE:
              num_coords = parse_reserve(fdjobs, MAX_RESERVATION_SIZE, &event_id, xs, ys);

              if (num_coords == 0) {
                fprintf(stderr, "Invalid command. See HELP for usage\n");
                continue;
              }

              if (ems_reserve(event_id, num_coords, xs, ys)) {
                fprintf(stderr, "Failed to reserve seats\n");
              }

              break;

            case CMD_SHOW:
              if (parse_show(fdjobs, &event_id) != 0) {
                fprintf(stderr, "Invalid command. See HELP for usage\n");
                continue;
              }

              if (ems_show(event_id, fdout)) {
                fprintf(stderr, "Failed to show event\n");
              }

              break;

            case CMD_LIST_EVENTS:
              if (ems_list_events(fdout)) {
                fprintf(stderr, "Failed to list events\n");
              }

              break;

            case CMD_WAIT:
              if (parse_wait(fdjobs, &delay, NULL) == -1) {  // thread_id is not implemented
                fprintf(stderr, "Invalid command. See HELP for usage\n");
                continue;
              }

              if (delay > 0) {
                printf("Waiting...\n");
                ems_wait(delay);
              }

              break;

            case CMD_INVALID:
              fprintf(stderr, "Invalid command. See HELP for usage\n");
              break;

            case CMD_HELP:
              printf(
                  "Available commands:\n"
                  "  CREATE <event_id> <num_rows> <num_columns>\n"
                  "  RESERVE <event_id> [(<x1>,<y1>) (<x2>,<y2>) ...]\n"
                  "  SHOW <event_id>\n"
                  "  LIST\n"
                  "  WAIT <delay_ms> [thread_id]\n"  // thread_id is not implemented
                  "  BARRIER\n"                      // Not implemented
                  "  HELP\n");

              break;

            case CMD_BARRIER:  // Not implemented
            case CMD_EMPTY:
              break;

            case EOC:
              fileEnded = 1;
              break;
          }
          if(fileEnded)
            break;
        }
        close(fdjobs);
        close(fdout);

        exit(0);
      }
    }
  }

  // Wait for remaining processes to terminate
  for(unsigned int i = 0; i < MAX_PROC; i++){
    int status;
    pid_t child_pid = wait(&status);

    if(WIFEXITED(status)){
      if(WEXITSTATUS(status) == -1){
          fprintf(stderr,"Error while terminating child processor\n");
          return -1;
      }
      
      fprintf(stdout, "Child process %d terminated with status %d\n", child_pid, status);
    }
    else{
        fprintf(stderr, "Failed to terminate child processor\n");
        return -1;
    }
  }

  closedir(dir);
  ems_terminate();
}
