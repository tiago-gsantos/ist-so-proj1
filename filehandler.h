#ifndef EMS_FILEHANDLER_H
#define EMS_FILEHANDLER_H

#include <stddef.h>

/// Opens the .jobs with the given name in the given directory and creates a .out
/// file with the same name.
/// @param dir_path Path for the jobs directory
/// @param file_name Name of the file inside of the jobs directory
/// @param fd_jobs Pointer for the file descriptor of the .jobs file
/// @param fd_out Pointer for the file descriptor of the .out file
/// @return 0 if both files were successfuly opened and -1 if any error occured
int open_file(char *dir_path, char *file_name, int *fd_jobs, int *fd_out);

/// Writes what's in the buffer to the file that has the given file descriptor
/// @param fd File descriptor of the file we want to write
/// @param buffer buffer with the content to be written to the file
/// @return 0 if content was successfuly writen and 1 otherwise
int write_to_file(int fd, char *buffer);

#endif  // EMS_FILEHANDLER_H
