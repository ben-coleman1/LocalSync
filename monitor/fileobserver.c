/*
 * fileobserver.c: linux-specific implementation of functions to track files
 * in a directory and be notified about creations, modifications, and deletions.
 *
 * Implemented as a wrapping layer around inotify.
 *
 * written by team Fleetwood MAC
 * CS60, May 2018.
 */

#include <sys/inotify.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <stdbool.h>
#include <stdio.h>
#include <poll.h>
#include <unistd.h>
#include "fileobserver.h"

// quick n dirty for recursive watching
int dir_wd[255];
char *dir_name[255];
int n_dirs = 0;

void watch_subdir(FileObserver *observer, FileInfo_FS *file) {
  if (!file->is_dir) {
    return;
  }

  // todo free these
  dir_name[n_dirs] = get_full_filepath(observer->dir, file->filepath);
  dir_wd[n_dirs] = inotify_add_watch(observer->fd, dir_name[n_dirs],
    IN_MODIFY | IN_CREATE | IN_DELETE | IN_MOVED_TO | IN_MOVED_FROM);

  n_dirs += 1;
}

// converts an inotify event into our standardized internal FileEvent
FileEvent *inotify_to_fileevent(char *dir, const struct inotify_event *inotify_event) {
  FileEvent *event = fileevent_init();
  if (event == NULL) {
    return NULL;
  }

  if (inotify_event->name[0] == '.') {
    return NULL;
  }

  char event_filename[FILEPATH_LEN];
  // default it to just the event file name
  strcpy(event_filename, inotify_event->name);

  // figure out if file from a subdir
  for (int i = 0; i < n_dirs; i++) {
    if (inotify_event->wd == dir_wd[i]) {
      memset(event_filename, 0, sizeof(event_filename));
      strcpy(event_filename, dir_name[i] + strlen(dir) + 1);
      strcat(event_filename, "/");
      strcat(event_filename, inotify_event->name);
    }
  }

  // set the event type based on the inotify mask
  if (inotify_event->mask & IN_MODIFY) {
    event->action = FILE_MODIFIED;
  } else if ((inotify_event->mask & IN_CREATE) | (inotify_event->mask & IN_MOVED_TO)) {
    event->action = FILE_CREATED;
  } else if ((inotify_event->mask & IN_DELETE) | (inotify_event->mask & IN_MOVED_FROM)) {
    event->action = FILE_DELETED;
  }

  // if file was deleted, use now as the time and don't attempt to
  // read from disk
  if (event->action == FILE_DELETED) {
    event->file = fileinfo_init();
    if (event->file == NULL) {
      free(event);
      return NULL;
    }
    // set fileinfo name from inotify event
    strcpy(event->file->filepath, event_filename);
    event->file->size = 0;
    event->file->last_modified = time(NULL);
  } else {
    // otherwise get info about the modified file from disk
    event->file = fileinfo_get_by_name(dir, event_filename);
    if (event->file == NULL) {
      //printf("no info on file %s\n", event_filename);
      free(event);
      return NULL;
    }
  }

  return event;
}

// initialize inotify to watch specified directory
FileObserver *fileobserver_init(char *dir, FileInfo_FS *files) {
  FileObserver *observer = calloc(1, sizeof(FileObserver));
  if (observer == NULL) {
    return NULL;
  }

  // set the base directory we're observing
  observer->dir = dir;

  // open inotify file descriptor
  observer->fd = inotify_init();
  if (observer->fd == -1) {
    perror("inotify_init");
    return NULL;
  }

  // add our directory to the watch set
  observer->wd = inotify_add_watch(observer->fd, dir,
      IN_MODIFY | IN_CREATE | IN_DELETE | IN_MOVED_TO | IN_MOVED_FROM); // listen only for modify, create, delete

  n_dirs = 0;
  FileInfo_FS *tmp = files;
  while (files != NULL) {
    // watch this file as a subdir (function will do nothing if not a dir) 
    watch_subdir(observer, files);
   
    tmp = files->next;
    fileinfo_destroy(files);
    files = tmp;
  }


  if (observer->wd == -1) {
    fprintf(stderr, "Cannot watch dir\n");
    perror("inotify_add_watch");
    free(observer);
    return NULL;
  }

  return observer;
}

// get the next file events, in standardized form, for the watched directory
FileEvent *fileobserver_get_events(FileObserver *observer) {
  if (observer == NULL) {
    return NULL;
  }

  char buf[4096]
    __attribute__ ((aligned(__alignof__(struct inotify_event))));
  const struct inotify_event *event;
  ssize_t len;
  char *ptr;

  // read from inotify into our buffer
  len = read(observer->fd, buf, sizeof buf);
  if (len == -1 && errno != EAGAIN) {
    perror("read");
    exit(EXIT_FAILURE);
  }

  if (len <= 0) {
    return NULL;
  }

  FileEvent *head = NULL;

  for (ptr = buf; ptr < buf + len;
      ptr += sizeof(struct inotify_event) + event->len) {

    event = (const struct inotify_event *) ptr;

    FileEvent *e = inotify_to_fileevent(observer->dir, event);
    if (e == NULL) {
      //fprintf(stderr, "Error: couldn't handle inotify event properly.\n");
      continue;
    }

    if (e->action == FILE_CREATED && e->file->is_dir) {
      watch_subdir(observer, e->file);  
    }

    e->next = head;
    head = e;
  }

  return head;
}

// clean up a fileobserver
void fileobserver_destroy(FileObserver *observer) {
  if (observer == NULL) {
    return;
  }

  close(observer->fd);
  free(observer);
}
