/*
 * monitor.h: main parent structure and associated queues/sets to
 * make it easy for a peer or other client to watch a directory and
 * read events containing info on what happened in the dir
 *
 * allows abstraction from actual file monitoring code, so that
 * linux, mac, and windows clients can all run through these same structures
 * and API
 *
 * written by team Fleetwood MAC 
 * CS60, May 2018. 
 */

#ifndef MONITOR_H
#define MONITOR_H
#include <pthread.h>
#include <stdbool.h>
#include "fileobserver.h"
#include "fileset.h"
#include "eventqueue.h"

typedef struct {
  // files that we should ignore the create/modify event of
  FileSet *ignore_modify; 

  // files that we should ignore the delete event of
  FileSet *ignore_delete; 

  // actions that have been taken by the user on the files. A client/peer should be reading
  // from this queue to send updates to the server
  EventQueue *event_queue; 

  char *dir;  // the directory that we are monitoring

  FileObserver *observer; // platform-dependent implementation to actually watch files and output FileEvents 
  pthread_t watch_thread; // thread that watches files and puts events into the queue
} monitor;

/*
 * Allocate, initialize, and return a monitor to watch files within baseDir
 * @return monitor* on success, NULL on error
 */
monitor *monitor_init(char *baseDir);

/*
 * Return a list of the info about all files in the directory
 * @return FileInfo_FS* list, or NULL on error
 */
FileInfo_FS *monitor_get_current_files(monitor *m);

/*
 * Start watching the directory
 * @return 1 on success, -1 on error
 */
int monitor_start_watching(monitor *m);

/*
 * Stop watching the directory
 * @return 1 on success, -1 on error
 */
int monitor_stop_watching(monitor *m);

/*
 * Begin ignoring a delete event at filepath in the watched directory
 * @return 1 on success, -1 on error
 */
int monitor_ignore_delete(monitor *m, char *filepath);

/*
 * Stop ignoring a delete event at filepath in the watched directory
 * @return 1 on success, -1 on error
 */
int monitor_resume_delete(monitor *m, char *filepath);

/*
 * Begin ignoring modification/creation events at filepath in the watched directory
 * @return 1 on success, -1 on error
 */
int monitor_ignore_modify(monitor *m, char *filepath);

/*
 * Stop ignoring modification/creation events at filepath in the watched directory
 * @return 1 on success, -1 on error
 */
int monitor_resume_modify(monitor *m, char *filepath);

/*
 * Returns all events in the event queue. The caller must
 * free them. 
 *
 * Blocks until an event is available.
 * @return FileEvent* list on success, NULL on error
 */
FileEvent *monitor_get_events(monitor *m);

/* 
 * Free all memory used by a monitor
 */
void monitor_destroy(monitor *m);

#endif
