/*
 * fileobserver-mac.c: mac-specific implementation of functions to track files
 * in a directory and be notified about creations, modifications, and deletions.
 *
 * Implemented as a wrapping layer around FSEvent.
 * Based off Stack Overflow answer at
 * https://stackoverflow.com/questions/18415285/osx-fseventstreameventflags-not-working-correctly
 *
 * written by team Fleetwood MAC
 * CS60, May 2018.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <CoreServices/CoreServices.h>
#include "fileobserver.h"

// alias the annoyingly long mac event names
#define eventCreated  kFSEventStreamEventFlagItemCreated
#define eventRenamed  kFSEventStreamEventFlagItemRenamed
#define eventRemoved  kFSEventStreamEventFlagItemRemoved
#define eventModified kFSEventStreamEventFlagItemModified

// forward declarations
void eventHandler(FSEventStreamRef,
    void*,
    size_t ,
    void *,
    const FSEventStreamEventFlags[],
    const FSEventStreamEventId[]);
void *fsevent_thread(void *arg);

// because of the callback structure, we unfortunately need these globals
// and means this is not a generically usable, thread-safe file observer :(
// but plenty good enough to watch 1 directory
FileObserver *main_obs;
pthread_t loop_thread;

// initialize FSEvent to watch specified directory
FileObserver *fileobserver_init(char *dir, FileInfo_FS *files) {
  FileObserver *observer = calloc(1, sizeof(FileObserver));
  if (observer == NULL) {
    return NULL;
  }
  
  // set the base directory we're observing
  observer->dir = dir;

  // set up the stream from the fsevent api
  CFStringRef dirPath = CFStringCreateWithCString( NULL, observer->dir, kCFStringEncodingUTF8);
  CFArrayRef pathsToWatch = CFArrayCreate( NULL, ( const void ** ) &dirPath, 1, NULL );
  CFAbsoluteTime latency = 0.1; // how long before kernel pushes info to us

  // create stream to watch for filesystem events
  observer->stream = FSEventStreamCreate(
      NULL,
      (FSEventStreamCallback) eventHandler,
      NULL,
      pathsToWatch,
      kFSEventStreamEventIdSinceNow, 
      latency,
      kFSEventStreamCreateFlagFileEvents 
      );

  observer->q = eventqueue_init();
  if (observer->q == NULL) {
    return NULL;
  }

  main_obs = observer;
  pthread_create(&loop_thread, NULL, fsevent_thread, observer);

  return observer;
}

// simple thread to loop forever and allow fsevents to occur
void *fsevent_thread(void *arg) {
  FileObserver *observer = (FileObserver *)arg;

  // start the stream
  FSEventStreamScheduleWithRunLoop(observer->stream, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
  FSEventStreamStart(observer->stream);
  CFRunLoopRun();

  pthread_exit(NULL);
}

// called when FEvent detects an event
void eventHandler(
    FSEventStreamRef streamRef,
    void *arg,
    size_t numEvents,
    void *eventPaths,
    const FSEventStreamEventFlags eventFlags[],
    const FSEventStreamEventId eventIds[]
  ) {
  FileObserver *obs = main_obs;
  EventQueue *q = obs->q;

  // get the full path to the directory
  char dir_fullpath[3024]; // long enough
  realpath(obs->dir, dir_fullpath);

  char **paths = eventPaths;

  for (int i=0; i < numEvents; i++) {
    // skip dot-prefixed files/folders
    if (paths[i][0] == '.') {
      continue;
    }

    FileEvent *event = fileevent_init();
    if (event == NULL) {
      return;
    }

    // first, set the event type 
    
    // since the flags are kinda iffy, check if file exists
    struct stat tmp;  // tmp stat used only for file existence
    if (stat(paths[i], &tmp) == -1 && errno == ENOENT) {
      event->action = FILE_DELETED;
    } else if ( (eventFlags[i] & eventRenamed) || (eventFlags[i] & eventCreated) ) {
      event->action = FILE_CREATED;
    } else if (eventFlags[i] & eventModified) {
      event->action = FILE_MODIFIED;
    } else {
      // other event, ignore
      continue;
    }

    // determine the path of the file relative to the directory watched
    char *relative_name = paths[i] + strlen(dir_fullpath) + 1;

    // if file was deleted, use now as the time and don't attempt to
    // read from disk
    if (event->action == FILE_DELETED) {
      event->file = fileinfo_init();
      if (event->file == NULL) {
        free(event);
        fprintf(stderr, "error initting fileinfo\n");
        return;
      }
      // set fileinfo name 
      strcpy(event->file->filepath, relative_name);
      event->file->size = 0;
      event->file->last_modified = time(NULL);
    } else {
      // otherwise get info about the modified file from disk
      event->file = fileinfo_get_by_name(obs->dir, relative_name);
      if (event->file == NULL) {
        free(event);
        fprintf(stderr, "error reading fileinfo\n");
        return;
      }
    }

    // put it in the queue of events we've read
    eventqueue_add(q, event);
  }

  FSEventStreamFlushSync(streamRef);
}

// get the next file events, in standardized form, for the watched directory
FileEvent *fileobserver_get_events(FileObserver *observer) {
  if (observer == NULL) {
    return NULL;
  }

  FileEvent *e = eventqueue_get_blocking(observer->q);
  return e;
}

void fileobserver_destroy(FileObserver *observer) {
  if (observer == NULL) {
    return;
  }

  FSEventStreamStop(observer->stream);
  FSEventStreamInvalidate(observer->stream);
  FSEventStreamRelease(observer->stream);

  free(observer);
}


