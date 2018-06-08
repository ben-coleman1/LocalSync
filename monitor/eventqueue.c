/*
 * eventqueue.c: Implementation of a thread-safe mechanism to queue
 * FileEvents as they occur and make them available to a consumer.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "eventqueue.h"

// create and return a new EventQueue
EventQueue *eventqueue_init() {
  EventQueue *q = calloc(1, sizeof(EventQueue));
  if (q == NULL) {
    return NULL;
  }

  // initialize the lock/cvar
  pthread_mutex_init(&q->lock, NULL);
  pthread_cond_init(&q->len_cv, NULL);

  return q;
}

int eventqueue_remove_name(EventQueue *q, char *filename) {
  if (q == NULL || filename == NULL) {
    return -1;
  }

  pthread_mutex_lock(&q->lock);

  FileEvent *e = q->head;
  FileEvent *prev = NULL;
  while (e != NULL) {
    if (strcmp(e->file->filepath, filename) == 0) {
      printf("Removing event in queue\n");
      prev->next = e->next;
      fileevent_destroy(e);
      q->length--;
    }

    prev = e;
    e = e->next;
  }

  pthread_mutex_unlock(&q->lock);

  return 1;
}

// add a fileevent to the end of the specified queue.
// Does not clone event, and destroys event->next.
// @return 1 on success, -1 on failure
int eventqueue_add(EventQueue *q, FileEvent *event) {
  if (q == NULL || event == NULL) {
    return -1;
  }

  // event will become new tail, so we need its next item to be empty
  event->next = NULL;

  pthread_mutex_lock(&q->lock);

  // when queue is empty, insert at the head
  // and update both head and tail
  if (q->head == NULL) {
    q->head = event;
    q->tail = q->head;
  } else {
    // otherwise, insert at the end
    q->tail->next = event;
    q->tail = q->tail->next;
  }

  q->length++;

  // signal anyone who's waiting that we now have items
  pthread_cond_signal(&q->len_cv);

  pthread_mutex_unlock(&q->lock);

  return 1;
}

// returns all items in the queue, emptying the queue
// should only be called while holding lock
FileEvent *eventqueue_remove_all(EventQueue *q) {
  if (q == NULL) {
    return NULL;
  }


  FileEvent *tmp = q->head;
  q->head = NULL;
  q->tail = NULL;
  q->length = 0;

  pthread_mutex_unlock(&q->lock);

  return tmp;
}

// blocks until queue is non-empty, then return whatever was in the queue
FileEvent *eventqueue_get_blocking(EventQueue *q) {
  if (q == NULL) {
    return NULL;
  }

  pthread_mutex_lock(&q->lock);
  while (q->length == 0) {
    pthread_cond_wait(&q->len_cv, &q->lock);
  }

  // when here, guaranteed to hold lock and have non-empty queue
  // so we can go ahead and grab all items in the queue
  FileEvent *events = eventqueue_remove_all(q);

  pthread_mutex_unlock(&q->lock);

  return events;
}

void eventqueue_destroy(EventQueue *q) {
  if (q == NULL) {
    return;
  }

  // clean out whatever is left in the queue
  FileEvent *tmp;
  while (q->head != NULL) {
    tmp = q->head->next;
    fileevent_destroy(q->head);
    q->head = tmp;
  }

  // then destroy the queue itself
  free(q);
}
