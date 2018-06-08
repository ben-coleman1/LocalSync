/*
 * eventqueue.h: An EventQueue is the primary structure that makes a monitor easy
 * to use by a consumer. This queue stores FileEvents and returns them as requested.
 * Provides an easy, thread-safe API with a blocking call to get all events from the queue.
 *
 * written by team Fleetwood MAC 
 * CS60, May 2018. 
 */

#ifndef EVENTQUEUE_H
#define EVENTQUEUE_H

#include <pthread.h>
#include "fileevent.h"

typedef struct {
  FileEvent *head;      // head of the queue
  FileEvent *tail;      // tail of the queue
  unsigned int length;  // # of items currently in the queue

  pthread_mutex_t lock; // lock for queue updates
  pthread_cond_t len_cv;// cvar used to notify when queue has new events
} EventQueue;

/*
 * Creates and returns a new queue
 * @return EventQueue* or NULL on error
 */
EventQueue *eventqueue_init();

/*
 * Internal function to dequeue all items in the queue.
 * Should only be called when holding the queue's lock, and most likely only 
 * for internal use. Use eventqueue_get_blocking as the primary external method.
 * @return FileEvent* or NULL on error.
 */
FileEvent *eventqueue_remove_all(EventQueue *q);

/*
 * Thread-safe function to append a FileEvent to the end of the queue. 
 * @return 1 on success, -1 on error
 */
FileEvent *eventqueue_get_blocking(EventQueue *q);

/*
 * Thread-safe function to append a FileEvent to the end of the queue. 
 * @return 1 on success, -1 on error
 */
int eventqueue_add(EventQueue *q, FileEvent *event);

/*
 * Release all memory consumed by an EventQueue.
 * Also frees any items in the queue at destroy time.
 * Should only be called when nobody listening to queue.
 */
void eventqueue_destroy(EventQueue *q);

#endif
