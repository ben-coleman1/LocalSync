/*
 * fileobserver.h: module that actually does the tracking of files
 * in a directory and be notified about creations, modifications, and deletions.
 *
 * This implementation will be platform specific. Right now, only linux is supported.
 * On linux, this is implemented as a wrapping layer around inotify.
 *
 * All platforms will maintain same the same API as described here; only the 
 * internals of the FileObserver struct should need to change here. The implementation
 * of the actual functions will vary greatly. 
 *
 * written by team Fleetwood MAC 
 * CS60, May 2018. 
 */

#ifndef FILEOBSERVER_H
#define FILEOBSERVER_H

#include "fileevent.h"
#include "eventqueue.h"

typedef struct FileObserver { 
  int fd;     // for unix implementation: the inotify file descriptor
  int wd;     // the watch descriptor of base dir
  char *dir;  // the base directory that we are observing
  EventQueue *q; // for mac implementation
  void *stream; // mac implementation: contains the FSEvent stream
} FileObserver;

/*
 * Creates and a returns a file observer configured to watch all files located in
 * the specified directory.
 * @return FileObserver* or NULL on error
 */
FileObserver *fileobserver_init(char *dir, FileInfo_FS *files);

/*
 * Blocks until there is at least 1 file event, then returns all events.
 * @return FileEvent* or NULL on error
 */
FileEvent *fileobserver_get_events(FileObserver *observer);

/*
 * Releases all memory associated with a file observer
 */
void fileobserver_destroy(FileObserver *observer);

#endif 
