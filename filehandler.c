#include "filehandler.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

int open_file(char *dir_path, char *file_name, int *fd_jobs, int *fd_out){
  // .jobs file path = directory path + name of file
  size_t path_len = strlen(dir_path) + strlen(file_name) + 1;
  char file_path[path_len];

  strcpy(file_path, dir_path);
  strcat(file_path, file_name);

  *fd_jobs = open(file_path, O_RDONLY);
  if(*fd_jobs < 0){
    return -1;
  }  

  // .out file path = .jobs file path - ".jobs" + ".out"
  file_path[path_len - 6] = '\0';
  strcat(file_path, ".out");

  *fd_out = open(file_path, O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR);
  if(*fd_out < 0){
    return -1;
  }

  return 0;
}

int write_to_file(int fd, char *buffer){ 
  size_t len = strlen(buffer);
  long int done = 0;
  
  while(len > 0){
    long int bytes_written = write(fd, buffer + done, len);

    if(bytes_written < 0){
      return 1;
    }

    len -= (size_t)bytes_written;
    done += bytes_written;
  }

  return 0;
}