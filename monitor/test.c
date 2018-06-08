/*
 * Sample code demonstrating how to use the file monitor. 
 */
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

#include "monitor.h"
#include "fileevent.h"

void show_all(monitor *m) {
  FileInfo_FS *file = monitor_get_current_files(m);
  while (file != NULL) {
    FileInfo_FS *tmp = file->next;

    fileinfo_print(file);
    fileinfo_destroy(file);
    file = tmp;
  }
}

int main(int argc, char **argv) {
  // create the monitor
  monitor *m = monitor_init("../data");
  assert(m != NULL);

  // start the thread to watch files
  monitor_start_watching(m);

  printf("reading files\n");
    //show_all(m);

  //char c;
  
  while (1) {
    // buffering events until key press just to allow more interesting testing 
    // of the queueing mechanism
    /*printf("Hit a key to read events\n");
    c = getc(stdin);
    printf("Got %d\n", c);
    if (c == EOF) {
      break;
    }*/

    // get all events (blocks until there are events) and print them
    FileEvent *e = monitor_get_events(m);
    FileEvent *tmp;
    while (e != NULL) {
      tmp = e->next;
      fileevent_print(e);
      fileevent_destroy(e);
      e = tmp;
    }

    // show current state of directory
    //show_all(m);
  }

  monitor_stop_watching(m);
  monitor_destroy(m);
}
