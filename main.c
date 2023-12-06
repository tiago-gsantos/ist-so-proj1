#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "constants.h"
#include "operations.h"
#include "parser.h"

int main(int argc, char *argv[]) {
  unsigned int state_access_delay_ms = STATE_ACCESS_DELAY_MS;

  if(argc == 1){
    fprintf(stderr, "Too few arguments\n");
    return 1;
  } 

  if (argc == 3) {
    char *endptr;
    unsigned long int delay = strtoul(argv[2], &endptr, 10);

    if (*endptr != '\0' || delay > UINT_MAX) {
      fprintf(stderr, "Invalid delay value or value too large\n");
      return 1;
    }

    state_access_delay_ms = (unsigned int)delay;
  }

  if (ems_init(state_access_delay_ms)) {
    fprintf(stderr, "Failed to initialize EMS\n");
    return 1;
  }

  DIR *dir = opendir(argv[1]);
  if(dir == NULL){
    fprintf(stderr, "Failed to open directory.\n");
    return 1;
  }

  struct dirent *entry;

  size_t dirlen = strlen(argv[1]);

  if(argv[1][dirlen-1] != '/'){
    strcat(argv[1], "/");
    dirlen++;
  }

  while ((entry = readdir(dir))!= NULL) {
    size_t pathnamesize = dirlen + strlen(entry->d_name) + 1;
    char filename[pathnamesize];

    unsigned int event_id, delay;
    size_t num_rows, num_columns, num_coords;
    size_t xs[MAX_RESERVATION_SIZE], ys[MAX_RESERVATION_SIZE];
    
    strcpy(filename, argv[1]);
    strcat(filename, entry->d_name);

    if(strstr(filename, ".jobs") != NULL){
      int fdjobs = open(filename, O_RDONLY);
      if(fdjobs < 0){
        fprintf(stderr, "Failed to open file.\n");
        return 1;
      }  

      filename[pathnamesize - 6] = '\0';
      strcat(filename, ".out");

      int fdout = open(filename, O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR);
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
    }
  }
  closedir(dir);
  ems_terminate();
}
