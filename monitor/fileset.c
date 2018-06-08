/*
 * fileset.c: implementation of file set functions 
 *
 * See full header comments in fileset.h
 *
 * written by team Fleetwood MAC
 * CS60, May 2018.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fileset.h"

FileSet *fileset_init() {
  FileSet *set = calloc(1, sizeof(FileSet));
  if (set == NULL) {
    return NULL;
  }

  pthread_mutex_init(&set->lock, NULL);

  return set;
}

// check if filepath exists in set
int fileset_contains(FileSet *set, char *filepath) {
  if (set == NULL || filepath == NULL) {
    return 0;
  }

  FileSetItem *cur = set->items;
  while (cur != NULL) { 
    if (strcmp(cur->filepath, filepath) == 0) {
      return 1;
    }

    cur = cur->next;
  }

  return 0;
}

// put filepath in set
int fileset_insert(FileSet *set, char *filepath) {
  if (set == NULL || filepath == NULL) {
    return 0;
  }

  // otherwise, create a new entry 
  FileSetItem *new_item = calloc(1, sizeof(FileSetItem));
  if (new_item == NULL) {
    return 0;
  }

  // copy in the filepath
  strcpy(new_item->filepath, filepath);
  
  // then link it at the head
  new_item->next = set->items;
  set->items = new_item;

  return 1;
}

// remove filepath from set
int fileset_remove(FileSet *set, char *filepath) {
  if (set == NULL || filepath == NULL) {
    return 0;
  }

  FileSetItem *prev = NULL;
  FileSetItem *cur = set->items;

  // look through all items for what we want
  while (cur != NULL) { 
    if (strcmp(cur->filepath, filepath) == 0) {
      // we need to remove cur
      // if prev is null, we're at the head
      if (prev == NULL) {
        set->items = set->items->next;
        free(cur);
        return 1;
      } else {
        // otherwise, skip over cur from prev
        prev->next = cur->next;
        free(cur);
        return 1;
      }
    }

    prev = cur;
    cur = cur->next;
  }

  // we didn't find it
  return 0;
}

void fileset_destroy(FileSet *set) {
  if (set == NULL) {
    return;
  }

  // free everything in the list
  FileSetItem *tmp;
  while (set->items != NULL) {
    tmp = set->items->next;
    free(set->items);
    set->items = tmp;
  }

  // free the struct itself
  free(set);
}

void fileset_print(FileSet *set) {
  if (set == NULL) {
    return;
  }

  printf("===== file set ====== \n");

  // free everything in the list
  FileSetItem *item = set->items;
  while (item != NULL) {
    printf("%s \n", item->filepath);
    item = item->next;
  }

  printf("===================== \n");
}
