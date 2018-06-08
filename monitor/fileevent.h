/*
 * fileevent.h: types and function headers for working with a FileEvent
 * FileEvent structs contain a FileInfo_FS to provide information about the file,
 * as well as additional fields to record info about the action that was taken
 * on the file. 
 *
 * FileEvents can be chained together to form lists, and are used inside EventQueue
 *
 * written by team Fleetwood MAC 
 * CS60, May 2018. 
 */

#ifndef FILEEVENT_H
#define FILEEVENT_H

#include <pthread.h>
#include "fileinfo.h"

// make it easy to get a human-readable action name from the enum
#define LIST_OF_ACTIONS \
    X(FILE_MODIFIED) \
    X(FILE_CREATED) \
    X(FILE_DELETED) \
    X(DOWNLOAD_COMPLETE) \

#define X(name) name,
enum ActionType { LIST_OF_ACTIONS };
#undef X

#define X(name) #name,
static char const * const ActionName[] = { LIST_OF_ACTIONS };
#undef X

typedef struct FileEvent {
  FileInfo_FS *file;         // information about the file
  enum ActionType action; // what action the user took on the file
  struct FileEvent *next; // next item in the list
} FileEvent;

/*
 * Creates and returns a new FileEvent
 * @return FileEvent* on success, NULL on error
 */
FileEvent *fileevent_init();

/*
 * Prints out a FileEvent in human-readable form
 */
void fileevent_print(FileEvent *event);

/*
 * Releases all memory associated with a FileEvent.
 */
void fileevent_destroy(FileEvent *event);

/*
 * Releases all memory associated with a FileEvent.
 */
void fileevent_destroy_all(FileEvent *events);

/*
 * Send a list of file info over the specified socket.
 * @return -1 on error, 1 on success
 */
int fileevent_send_all(int fd, FileEvent *event);

/*
 * Receive a list of file events over the specified socket.
 * Caller must free those events.
 * @return a list of FileEvent on success, NULL on error
 */
FileEvent *fileevent_receive(int fd, int n_events);


#endif
