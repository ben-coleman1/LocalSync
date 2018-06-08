#define _XOPEN_SOURCE 500

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <fts.h>
#include <ftw.h>
#include "monitor.h"
#include "fileobserver.h"
#include "fileevent.h"
#include "fileset.h"
#include "fileinfo.h"
#include "eventqueue.h"

// since nftw only calls a function for each file, we need these
// globals to get information out of it
pthread_mutex_t nftw_lock = PTHREAD_MUTEX_INITIALIZER;
FileInfo_FS *dir_list;
int prefix_len;
char *root_dir;

// initializes monitor for specified directory
// @return pointer to a newly-allocated monitor instance or NULL
monitor *monitor_init(char *rootDir) {
  // allocate space for the file monitor and all its necessary parts
  monitor *m = calloc(1, sizeof(monitor));
  if (m == NULL) {
    return NULL;
  }

  m->event_queue = eventqueue_init();
  if (m->event_queue == NULL) {
    return NULL;
  }

  m->dir = rootDir;

  FileInfo_FS *curFiles = monitor_get_current_files(m);
  m->observer = fileobserver_init(rootDir, curFiles);
  if (m->observer == NULL) {
    return NULL;
  }

  m->ignore_modify = fileset_init();
  if (m->ignore_modify == NULL) {
    return NULL;
  }

  m->ignore_delete = fileset_init();
  if (m->ignore_delete == NULL) {
    return NULL;
  }

  return m;
}


// callback for nftw
static int display_info(const char *fpath, const struct stat *sb,
            int tflag, struct FTW *ftwbuf)
{
   // if the depth is above the max sub dirs, continue to next file
   if (ftwbuf->level == 0 || ftwbuf->level > MAX_DEPTH) {
       return 0;
   }

   FileInfo_FS *cur = fileinfo_get_by_name(root_dir, fpath + prefix_len);
   if (cur == NULL) {
     return 0;
   }

   cur->next = dir_list;
   dir_list = cur;

   return 0; // continue reading
}

// returns ground truth of all files currently in the directory
FileInfo_FS *monitor_get_current_files(monitor *m) {
  if (m == NULL) {
    return NULL;
  }

  // lock before setting globals and starting nftw procedure
  pthread_mutex_lock(&nftw_lock);

  // reset the base dir, the length of the prefix, and the generated list
  dir_list = NULL;
  prefix_len = strlen(m->dir) + 1;
  root_dir = m->dir;

  // do the directory traverse
  if (nftw(m->dir, display_info, 20, 0) == -1) {
    perror("nftw");
    exit(EXIT_FAILURE);
  }

  // create a new pointer to the head of the list, since any other call to this
  // will destroy it
  FileInfo_FS *head = dir_list;

  pthread_mutex_unlock(&nftw_lock);

  return head;
}

// main thread updating the event queue
void *monitor_thread(void *args) {
  //filewatch_init();
  monitor *m = (monitor *)args;

  FileEvent *event;
  FileEvent *tmp;

  // loop until thread is cancelled
  while (1) {
    // get the next events from the observer
    event = fileobserver_get_events(m->observer);

    // no messing with the ignore sets when we're checking
    pthread_mutex_lock(&m->ignore_modify->lock);
    pthread_mutex_lock(&m->ignore_delete->lock);

    // for each event...
    while (event != NULL) {
      tmp = event->next; // store the next item, since adding will destory the next pointer

      if (event->file->filepath[0] == '.') {
        fileevent_destroy(event);
        event = tmp;
        continue;
      }

      // confirm that this event isn't on an ignored file
      switch (event->action) {
        case FILE_MODIFIED:
        case FILE_CREATED:
          if (fileset_contains(m->ignore_modify, event->file->filepath)) {
            fileevent_destroy(event);
            event = tmp;
            continue;
          }
          break;

        case FILE_DELETED:
          if (fileset_contains(m->ignore_delete, event->file->filepath)) {
            fileevent_destroy(event);
            event = tmp;
            continue;
          }
          break;

        default:
          break;
      }

      // add the item to the queue for external consumption
      eventqueue_add(m->event_queue, event);

      // advance to next item
      event = tmp;
    }

    pthread_mutex_unlock(&m->ignore_modify->lock);
    pthread_mutex_unlock(&m->ignore_delete->lock);

  }

  pthread_exit(NULL);
}

// starts the monitor's thread
// @return 1 on success, -1 on error
int monitor_start_watching(monitor *m) {
  if (m == NULL) {
    return -1;
  }

  // start thread to put events into the queue
  int t = pthread_create(&(m->watch_thread), NULL, monitor_thread, (void *)m);
  if (t != 0) {
    fprintf(stderr, "Error launching file monitor.\n");
    return -1;
  }


  return 1;
}

// stops the monitor's thread
int monitor_stop_watching(monitor *m) {
  if (m == NULL) {
    return -1;
  }

  // stop the monitoring thread by cancelling and joining it
  pthread_cancel(m->watch_thread);
  pthread_join(m->watch_thread, NULL);

  return 1;
}

// start ignoring a file, so that events from it won't appear in
// the event queue
int monitor_ignore_modify(monitor *m, char *filepath) {
  if (m == NULL) {
    return -1;
  }

  pthread_mutex_lock(&m->ignore_modify->lock);
  int res = fileset_insert(m->ignore_modify, filepath);
  pthread_mutex_unlock(&m->ignore_modify->lock);

  return res;
}

// stop ignoring a file, so that events from it appear in
// the event queue again
int monitor_resume_modify(monitor *m, char *filepath) {
  if (m == NULL) {
    return -1;
  }

  pthread_mutex_lock(&m->ignore_modify->lock);
  int res = fileset_remove(m->ignore_modify, filepath);
  pthread_mutex_unlock(&m->ignore_modify->lock);

  return res;
}



// start ignoring a file, so that events from it won't appear in
// the event queue
int monitor_ignore_delete(monitor *m, char *filepath) {
  if (m == NULL) {
    return -1;
  }

  pthread_mutex_lock(&m->ignore_delete->lock);
  int res = fileset_insert(m->ignore_delete, filepath);
  pthread_mutex_unlock(&m->ignore_delete->lock);

  return res;
}

// stop ignoring a file, so that events from it appear in
// the event queue again
int monitor_resume_delete(monitor *m, char *filepath) {
  if (m == NULL) {
    return -1;
  }

  pthread_mutex_lock(&m->ignore_delete->lock);
  int res = fileset_remove(m->ignore_delete, filepath);
  pthread_mutex_unlock(&m->ignore_delete->lock);

  return res;
}


// dequeues and returns all file events currently in the queue
// blocks until at least one event exists
// caller must free
FileEvent *monitor_get_events(monitor *m) {
  if (m == NULL) {
    return NULL;
  }

  return eventqueue_get_blocking(m->event_queue);
}

// destroy a monitor and all its associated memory
void monitor_destroy(monitor *m) {
  if (m == NULL) {
    return;
  }

  eventqueue_destroy(m->event_queue);
  fileobserver_destroy(m->observer);

  free(m->ignore_modify);
  free(m->ignore_delete);

  free(m);
}
