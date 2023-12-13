#ifndef EMS_FILEHANDLER_H
#define EMS_FILEHANDLER_H

#include <stddef.h>

int open_file(char *dir_path, char *file_name, int *fd_jobs, int *fd_out);

int write_to_file(int fd, char *buffer);

#endif  // EMS_FILEHANDLER_H
