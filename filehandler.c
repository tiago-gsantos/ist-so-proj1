#include "filehandler.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>


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