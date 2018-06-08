/*
 * fileinfo.h: types and function headers for working with FileInfo_FS
 *
 * Renamed FileInfo_FS since FileInfo conflicted with internal mac type.
 *
 * File structs contain information about files in the filesystem that LocalSync
 * is tracking. This provides the base underlying type that should be globally
 * used and passed between the tracker and clients.
 *
 * written by team Fleetwood MAC
 * CS60, May 2018.
 */

#ifndef FILEINFO_H
#define FILEINFO_H

#include <pthread.h>
#include <stdbool.h>

#define MAX_DEPTH 5
#define FILEPATH_LEN 255*MAX_DEPTH

typedef struct FileInfo_FS {
  char filepath[FILEPATH_LEN];// path of the file, relative to root dir being watched
  unsigned int size;          // size of the file
  time_t last_modified;       // timestamp of last modification
  int is_dir;                // true if this path is a directory
  struct FileInfo_FS *next;      // permit chaining FileInfo_FSs together into a list
} FileInfo_FS;

/*
 * Creates and returns a new FileInfo_FS
 * @return FileInfo_FS* on success, NULL on error
 */
FileInfo_FS *fileinfo_init();

/*
 * Createas a new FileInfo_FS, then populates it with information about
 * the file living at filepath, with directory as the root dir that LocalSync
 * is tracking.
 * @return FileInfo_FS* on success, NULL on error
 */
FileInfo_FS *fileinfo_get_by_name(char *directory, const char *filepath);

void fileinfo_print(FileInfo_FS *info);

/*
 * Releases all memory consumed by a FileInfo_FS
 */
void fileinfo_destroy(FileInfo_FS *info);

/*
 * Walk a list of file info, destroying them all.
 */
void fileinfo_destroy_all(FileInfo_FS *info);

/*
 * Creates and returns a string containing the path to filepath within
 * the watched directory dir.
 * @return pointer to filepath or NULL on error
 */
char *get_full_filepath(char *dir, char *filepath);

/*
 * Send a list of file info over the specified socket.
 * @return -1 on error, 1 on success
 */
int fileinfo_send_all(int fd, FileInfo_FS *file);

/*
 * Receive a list of file info over the specified socket.
 * @return a list of FileInfo on success, NULL on error
 */
FileInfo_FS *fileinfo_receive(int fd, int n_files);

#endif
