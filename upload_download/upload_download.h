#ifndef DOWNLOAD_H
#define DOWNLOAD_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "../peer/peer.h"
#include "../filetable/filetable.h"
#include "../monitor/fileinfo.h"


/****************** Macros ********************/
#define UNDOWNLOADED 0
#define DOWNLOADING 1
#define DOWNLOADED 2

/******************* function declarations *******************/
void init_upload(char *dir, int listPort);
// void init_download(char *dir, TableEntry *tableEntry);
void init_download(char *dir, TableEntry *tableEntry, int seglen, int numstreams);
void upload_destroy();


/******************* typedefs *******************/
/*
 * holds information to pass to thread about the specific peer we're
 * going to communicate, and which sequence of information that we're asking
 * for from this peer
 */
typedef struct Sequenceinfo {
  TableEntry *tableEntry; // table entry for this file
  int sock; // socket FD for peer from which we're requesting
  int initSeg; // initial sequence number of segment requested
  int length; // length of the segment requested
  struct HandlingInfo *handlingInfo; // shared mutexes and resources
  int status;
} SequenceInfo;

typedef struct SequenceinfoArr {
  SequenceInfo **arr;
  pthread_mutex_t seq_lock;
  int numSegs;
  int *peers;
  int numPeers;
  int numReceived;
} SequenceInfoArr;


/*
 * holds information about the sequence of data we've received from a peer,
 * as well as the data itself. implemented as a linked list node
 */
typedef struct FileSequence {
  TableEntry *tableEntry; // table entry for this file
  int initSeg; // initial sequence number of this chunk
  int length; // length of this chunk
  char *buf; // actual data
  struct FileSequence *next; // next chunk of the file
  struct HandlingInfo *handlingInfo; // shared mutexes and resources
} FileSequence;


/*
 * holds all of the mutexes and shared variables between threads on dwnload side
 */
typedef struct HandlingInfo {
  pthread_mutex_t *queue_mutex; // lock for queue updates
  pthread_mutex_t *sheepMutex; // mutex to lock the thread tracker
  int sheep; // keep track of how many threads have made it back to the manger
  TableEntry *entry;
  FileSequence *head; // head of the linked list of file chunks
  char *baseDir; // the path to the base directory we're syncing
  struct HandlingInfo *next; // next file in the list of files we're downloading
} HandlingInfo;


/*
 * Mutex and sock on upload side
 */
typedef struct UpHandlingInfo {
  int sock; // socket fd we're communicating on
  pthread_mutex_t *fileMutex; // mutex to lock file we're touching
} UpHandlingInfo;



#endif
