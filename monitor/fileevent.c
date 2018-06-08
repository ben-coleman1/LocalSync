/*
 * fileevent.c: implementations of FileEvent functions.
 *
 * written by team Fleetwood MAC 
 * CS60, May 2018. 
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include "fileevent.h"

// creates and returns a new FileEvent
FileEvent *fileevent_init() {
  FileEvent *event = calloc(1, sizeof(FileEvent));
  if (event == NULL) {
    return NULL;
  }

  return event;
}

// prints out a FileEvent  
void fileevent_print(FileEvent *event) {
  printf("%s: ", ActionName[event->action]);
  fileinfo_print(event->file);
}


void fileevent_destroy(FileEvent *event) {
  if (event == NULL) {
    return;
  }

  fileinfo_destroy(event->file);
  free(event);
}

// frees all memory associated with a list of FileInfo_FSs
void fileevent_destroy_all(FileEvent *event) {
  if (event == NULL) {
    return;
  }

  // free the next item
  fileevent_destroy_all(event->next);

  // then destroy myself
  fileevent_destroy(event);
}

/*
 * FileEvent send/receive helpers
 */

FileEvent *fileevent_receive(int fd, int n_events) {
  if (n_events <= 0) {
    return NULL;
  }

  FileEvent *head = NULL;

  for (int i = 0; i < n_events; i++) {
    // create space for fileinfo for this event 
    FileInfo_FS *info = fileinfo_init();
    if (info == NULL) {
      return NULL;
    }

    // read in the fileinfo
    if (recv(fd, info, sizeof(FileInfo_FS), MSG_WAITALL) < 0) {
      return NULL;
    }


    // create space for the event itself
    FileEvent *event = fileevent_init();
    if (event == NULL) {
      return NULL;
    }
    
    // read in the event
    if (recv(fd, event, sizeof(FileEvent), MSG_WAITALL) < 0) {
      return NULL;
    }

    // set the file pointer to the received fileinfo
    event->file = info;

    // add this event at the front of the received list
    event->next = head;
    head = event;
  }

  return head;

  return NULL;
}

int fileevent_send_all(int fd, FileEvent *event) {
  if (event == NULL) {
    return -1;
  }

  while (event != NULL) {
    // send fileinfo first
    if (send(fd, event->file, sizeof(FileInfo_FS), 0) <= 0) {
      perror("error sending");
      return -1;
    }

    // followed by the rest of the event data
    if (send(fd, event, sizeof(FileEvent), 0) <= 0) {
      perror("error sending");
      return -1;
    }
    event = event->next;
  }

  return 1;
}
