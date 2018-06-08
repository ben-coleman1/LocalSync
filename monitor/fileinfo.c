/*
 * fileinfo.c: implementatinos of FileInfo_FS functions
 *
 * written by team Fleetwood MAC 
 * CS60, May 2018. 
 */

#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include "fileinfo.h"

// creates and returns a new FileInfo_FS
FileInfo_FS *fileinfo_init() {
  FileInfo_FS *info = calloc(1, sizeof(FileInfo_FS));
  if (info == NULL) {
    return NULL;
  }

  return info;
}

// creates, populates, and returns a FileInfo_FS struct 
FileInfo_FS *fileinfo_get_by_name(char *directory, const char *filepath) {
  if (directory == NULL || filepath == NULL) {
    return NULL;
  }

  FileInfo_FS *info = fileinfo_init();
  if (info == NULL) {
    return NULL;
  }

  char *fullpath = calloc(strlen(filepath) + 1 + strlen(directory) + 1, sizeof(char));
  strcat(fullpath, directory);
  strcat(fullpath, "/");
  strcat(fullpath, filepath);

  // get info about the requested file
  struct stat attr;
  int res = stat(fullpath, &attr);

  // if it's not a regular file, do nothing and return
  if (res < 0) {
    free(info);
    free(fullpath);
    return NULL;
  }

  // set last modified time and size
  info->last_modified = attr.st_mtime;
  info->size = attr.st_size;
  info->is_dir = !S_ISREG(attr.st_mode);

  // consider directories to have size 0
  if (info->is_dir) {
    info->size = 0;
  }

  // copy in the path to the file
  strcpy(info->filepath, filepath);

  // free the full path we created
  free(fullpath);

  return info;
}

void fileinfo_print(FileInfo_FS *info) {
  if (info == NULL) {
    return;
  }

  printf("%s, size: %d, last modified: %s", info->filepath, info->size, ctime(&info->last_modified));
}

// frees all memory associated with a FileInfo_FS
void fileinfo_destroy(FileInfo_FS *info) {
  if (info == NULL) {
    return;
  }

  free(info);
}

// frees all memory associated with a list of FileInfo_FSs
void fileinfo_destroy_all(FileInfo_FS *info) {
  if (info == NULL) {
    return;
  }

  // free the next item
  fileinfo_destroy_all(info->next);

  // then destroy myself
  fileinfo_destroy(info);
}

/*
 * functions to send/receive a list of FileInfo_FSs over a socket  
 */

// reads n_files many FileInfo_FS structs over the socket and reconstructs
// them into a linked list
FileInfo_FS *fileinfo_receive(int fd, int n_files) {
  if (n_files <= 0) {
    return NULL;
  }

  FileInfo_FS *head = NULL;

  for (int i = 0; i < n_files; i++) {
    // create space for new file and put it at the front of the list
    FileInfo_FS *cur = fileinfo_init();

    // read into the new fileinfo
    if (recv(fd, cur, sizeof(FileInfo_FS), MSG_WAITALL) < 0) {
      return NULL;
    }

    cur->next = head;

    // prep for next file to come over the socket
    head = cur;
  }

  return head;
}

// send all files in the list over the socket
int fileinfo_send_all(int fd, FileInfo_FS *file) {
  while (file != NULL) {
    fileinfo_print(file);
    if (send(fd, file, sizeof(FileInfo_FS), 0) <= 0) {
      perror("error sending");
      return -1;
    }
    file = file->next;
  }

  return 1;
}

// allocates and returns a string containing path to the file
char *get_full_filepath(char *dir, char *filepath) {
  if (filepath == NULL) {
    return NULL;
  }

  char *filename = calloc(strlen(filepath) + strlen(dir) + 2, sizeof(char));
  if (filename == NULL) {
    return NULL;
  }

  strcat(filename, dir);
  strcat(filename, "/");
  strcat(filename, filepath);

  return filename;
}
