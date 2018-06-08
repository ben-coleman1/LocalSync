/*
 * fileset.h: types and functions for working with a set of filepaths 
 *
 * Used by the monitor to maintain sets of files who should have their
 * events be ignored.
 *
 * written by team Fleetwood MAC
 * CS60, May 2018.
 */

#ifndef FILESET_H
#define FILESET_H

#include <pthread.h>
#include "fileinfo.h"

// set items essentially just form a list of strings
typedef struct FileSetItem {
  char filepath[FILEPATH_LEN];
  struct FileSetItem *next;
} FileSetItem;

typedef struct {
  FileSetItem *items;
  pthread_mutex_t lock;
} FileSet;


/*
 * Creates and returns a new FileSet
 * @return FileSet* on success, NULL on error
 */
FileSet *fileset_init();

/*
 * Insert the specified path into the set.
 * @return 1 on success, 0 on error
 */
int fileset_insert(FileSet *set, char *filepath);

/*
 * Check if the path exists in the set.
 * @return 1 if yes, 0 else
 */
int fileset_contains(FileSet *set, char *filepath);

/*
 * Remove the path from the set.
 * @return 1 on success, 0 on error
 */
int fileset_remove(FileSet *set, char *filepath);

/*
 * Print out a set in a nice form
 */
void fileset_print(FileSet *set);

/*
 * Release all memory consumed by a set
 */
void fileset_destroy(FileSet *set);

#endif
