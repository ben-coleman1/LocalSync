/*
*
* functionality for pulling files from multiple clients.
* to be integrated into localSync.
* CS60, May 2018
* fleetwood MAC
*
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <math.h>
#include <time.h>
#include <utime.h>
#include <sys/types.h>
#include "upload_download.h"
#include "../filetable/filetable.h"
#include "../monitor/fileinfo.h"
#include "../peer/peer.h"
#include <signal.h>

#define MAX_DOWNLOADS 255

/******************* function primitives *******************/
int seg_handler(SequenceInfo *seqInf, int sock);
void *peer_thread(void *sequenceInfos);
void *file_saver(void *chunk);
int peer_connect(IP *ip);
// void *download(void *peerInfo);
void *download(void *handlingInformation);


/******************* globals *******************/
char *baseDir;
int data_len; // TODO: why not just define this as a macro?
int stream_num;

void download_complete_callback(TableEntry *fileentry);


/******************* init_download() *******************/
/*
* wrapper funtion for calling download thread method
* arg is the tableentry for the file that we're requesting
*/
void init_download(char *dir, TableEntry *tableEntry, int seglen, int numstreams)
{
  data_len = seglen;
	stream_num = numstreams;
  // if file size is 0
  if (tableEntry->file->size == 0) {
    // return immediately
    download_complete_callback(tableEntry);
    return;
  }

  // allocate for a handlingInfo struct
  HandlingInfo *handlingInfo = calloc(1,sizeof(HandlingInfo));
  handlingInfo->entry = tableEntry; // package the table entry

  // set the basedir
  baseDir = dir;

  // create and call the download thread with peer info
  pthread_t download_thread;
  pthread_create(&download_thread, NULL, &download, (void *) handlingInfo);
  pthread_detach(download_thread);
}



/******************* download() *******************/
/*
* thread method for handling download request
* arg is the TableEntry for the file that we're requesting
* there is one download thread per file that we are requesting
*/
void *download(void *handlingInformation)
{
  // grab and cast the handling info
  HandlingInfo *handlingInfo = (HandlingInfo *)handlingInformation;

  // init the mutexes
  handlingInfo->queue_mutex = calloc(1, sizeof(pthread_mutex_t));
  handlingInfo->sheepMutex = calloc(1, sizeof(pthread_mutex_t));
  pthread_mutex_init(handlingInfo->queue_mutex, NULL);
  pthread_mutex_init(handlingInfo->sheepMutex, NULL);

  // grab the table entry
  TableEntry *peerInf = handlingInfo->entry;

  handlingInfo->entry = peerInf;

  // instantiate the sheep counter
  handlingInfo->sheep = 0;

  // get the number of segments that we'll be asking for
  int segNum = ceil((float)peerInf->file->size / (float)data_len);

  // get the head of the IP addres linked list
  IP *ipHead = peerInf->iphead;

  printf("Starting download, %d segs for %d bytes of %s\n", segNum, peerInf->file->size, peerInf->file->filepath);

  int segLength;
  // Make all the sequences to get
  SequenceInfoArr *seqInfos = calloc(1, sizeof(SequenceInfoArr));
  if (seqInfos == NULL) {
    fprintf(stderr, "malloc err\n");
    exit(1);
  }
  seqInfos->arr = calloc(segNum, sizeof(SequenceInfo*));
  if (seqInfos->arr == NULL) {
    fprintf(stderr, "malloc err\n");
    exit(1);
  }
  pthread_mutex_init(&seqInfos->seq_lock, NULL);
  seqInfos->numSegs = segNum;
  seqInfos->numPeers = peerInf->numpeers;
  seqInfos->peers = calloc(peerInf->numpeers * stream_num, sizeof(int));

  // Allocate each sequence
  for (int i = 0; i < segNum; i++) {
    seqInfos->arr[i] = calloc(1, sizeof(SequenceInfo));
    if (seqInfos->arr[i] == NULL) {
      fprintf(stderr, "malloc err\n");
      exit(1);
    }
    // Fill the SequenceInfo with everything but peer from which to download
    // 	Get the beginning of the sequence that we want
    seqInfos->arr[i]->initSeg = i * data_len;

    // 	Put the table entry in there
    seqInfos->arr[i]->tableEntry = peerInf;

    //	Put the handling info in there
    seqInfos->arr[i]->handlingInfo = handlingInfo;

    //	Get the length of the segment that we want
    // 	If not the last chunk, then default segment size
    if (i < segNum - 1) {
      segLength = data_len;
    }
    // A number smaller than or equal to data_len
    else {
      segLength = peerInf->file->size - (i * data_len);
    }
    seqInfos->arr[i]->length = segLength;

    // 	Set the status to unsent
    seqInfos->arr[i]->status = UNDOWNLOADED;
  }

  // Start the file saver
  pthread_t save_file;
  pthread_create(&save_file, NULL, &file_saver, (void *) handlingInfo);
  pthread_detach(save_file);

  // Start 1 process per peer to download
  IP *curPeer = ipHead;
  // Iterate through the peers
  for (int i = 0; i < peerInf->numpeers; i++) {
    // Can specify multiple sockets per peer to speed up download
    for (int j = 0; j < stream_num; j++) {
      // Pass in peer-specific socket, which they will copy for further use
      seqInfos->peers[i * stream_num + j] = peer_connect(curPeer);

      //printf("%d, %d, %d, %d, %d", seqInfos->peers[0], seqInfos->peers[1], seqInfos->peers[2], seqInfos->peers[3], seqInfos->peers[4]);
      // Begin peer-specific thread
      pthread_t handle_segs;

      pthread_create(&handle_segs, NULL, &peer_thread, (void *) seqInfos);
      pthread_detach(handle_segs);
    }

    // go to the next peer
    curPeer = curPeer->next;
  }
  pthread_exit(NULL);
}



/******************* peer_connect() *******************/
/*
* handles connecting to a peer. returns socket FD.
*/
int peer_connect(IP *ip)
{

  // get peer info
  struct sockaddr_in peer_addr;
  peer_addr.sin_family = AF_INET;
  peer_addr.sin_port = htons(ip->port);

  // get the peer address info
  struct hostent *hostp = gethostbyname(ip->ip);

  if (hostp == NULL) {
    perror("Error getting peer address");
    exit(1);
  }

  // copy peer address into address struct
  memcpy(&peer_addr.sin_addr, hostp->h_addr_list[0], hostp->h_length);

  // create socket
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    perror("Error opening socket for peer");
    exit(1);
  }

  // connect to peer
  if (connect(sock, (struct sockaddr *) &peer_addr, sizeof(peer_addr)) < 0) {
    perror("error connecting to peer");
    return -1;
    //exit(1);
  }

  // return the socket fd
  return sock;
}



/******************* peer_thread() *******************/
/*
* thread for each peer that has the file we're requesting.
* while we're still trying to get pieces of the file, calls seg_handler,
* updates the metadata of the segment we're requesting
* when done, writes a termination method to peer and cleans up
*/
void *peer_thread(void *sequenceInfos)
{

  // Snag and cast the passed struct
  SequenceInfoArr *seqInfos = (SequenceInfoArr *) sequenceInfos;

  // Communication socketi/ip to download
  int sock;
  int peer;
  int stream;
  bool done = false;

  // Get a peer socket descriptor
  for (peer = 0; !done && peer < (seqInfos->numPeers); peer++) {
    for (stream = 0; !done && stream < stream_num; stream++) {

      pthread_mutex_lock(&seqInfos->seq_lock);
      if (seqInfos->peers[peer * stream_num + stream] >= 0) {

        // Copy the communication socket
        sock = seqInfos->peers[peer * stream_num + stream];

        // Set the socket to -1 so next peer takes the next one
        seqInfos->peers[peer * stream_num + stream] = -1;
        pthread_mutex_unlock(&seqInfos->seq_lock);
        done = true;
        break;
      }
      pthread_mutex_unlock(&seqInfos->seq_lock);
    }
  }

  int curSeg;

  // loop until the number of downloaded segments matches the total number of segments
  while (seqInfos->numReceived < seqInfos->numSegs) {
    for (curSeg = 0; curSeg < seqInfos->numSegs; curSeg++) {
      pthread_mutex_lock(&seqInfos->seq_lock);

      // don't start redownloading things
      if (seqInfos->arr[curSeg]->status == DOWNLOADING) {
        pthread_mutex_unlock(&seqInfos->seq_lock);
        continue;
      }

      if (seqInfos->arr[curSeg]->status == UNDOWNLOADED) {
        //printf("Downloading segment %d/%d from peer:stream %d:%d\n", curSeg, seqInfos->numSegs, peer, stream);
        fflush(stdout);
        // Set the status of the segment to DOWNLOADING
        seqInfos->arr[curSeg]->status = DOWNLOADING;
        // Set the socket of the segment

        pthread_mutex_unlock(&seqInfos->seq_lock);

        // Begin downloading this segment
        if (sock != -1 && seg_handler(seqInfos->arr[curSeg], sock) == -1) {
          //fprintf(stderr, "Download stopped by peer\n");
          seqInfos->arr[curSeg]->status = UNDOWNLOADED; // set back to undownloaded
          break;
        }

        // Once downloaded, set status to DOWNLOADED
        pthread_mutex_lock(&seqInfos->seq_lock);

        seqInfos->arr[curSeg]->status = DOWNLOADED;
        seqInfos->numReceived++;
      }

      pthread_mutex_unlock(&seqInfos->seq_lock);
    }
  }

  // Write termination message
  seqInfos->arr[0]->initSeg = -1;
  if (write(sock, seqInfos->arr[0], sizeof(SequenceInfo)) < 0) {
    fprintf(stderr, "Download stopped by peer\n");
  }
  // Close the socket
  close(sock);

  // Use peer[0] to signal how many threads/peers are done
  pthread_mutex_lock(&seqInfos->seq_lock);

  seqInfos->peers[0]--;

  // The last thread to exit
  if (seqInfos->peers[0] == -1*(seqInfos->numPeers * stream_num)-1) {
    pthread_mutex_unlock(&seqInfos->seq_lock);
    // free all peer-shared memory
    for (int i = 0; i < seqInfos->numSegs; i++) {
      free(seqInfos->arr[i]);
    }
    free(seqInfos->arr);
    free(seqInfos->peers);
    pthread_mutex_destroy(&seqInfos->seq_lock);
    free(seqInfos);
  }
  else {
    pthread_mutex_unlock(&seqInfos->seq_lock);
  }

  pthread_exit(NULL);
}



/******************* seg_handler() *******************/
/*
* handles segments of a file coming in, appends them to a linked
* list with information about where they belong in the file
* file_saver then handles saving the segments out to the file
*/
int seg_handler(SequenceInfo *seqInf, int sock)
{
  // parse out info from passed struct
  int initSeg = seqInf->initSeg;
  int segLength = seqInf->length;

  // allocate a buffer to hold segment
  char *buf = calloc(segLength, sizeof(char));

  // send over the sequence info
  if (write(sock, seqInf, sizeof(SequenceInfo)) < 0) {
    return -1;
  }

  // then send over the file info
  if (write(sock, seqInf->tableEntry->file,sizeof(FileInfo_FS)) < 0) {
    return -1;
  }

  if (recv(sock, buf, segLength, MSG_WAITALL) < 0) {
    return -1;
  }

  // allocate for a new fileSeq chunk
  FileSequence *newSeq = calloc(1, sizeof(FileSequence));

  // stuff our things in there
  newSeq->initSeg = initSeg;
  newSeq->length = segLength;
  newSeq->buf = buf;
  newSeq->handlingInfo = seqInf->handlingInfo;

  // insert at the head
  pthread_mutex_lock(seqInf->handlingInfo->queue_mutex);

  newSeq->next = seqInf->handlingInfo->head;
  seqInf->handlingInfo->head = newSeq;
  pthread_mutex_unlock(seqInf->handlingInfo->queue_mutex);

  return 0;
}


/******************* file_saver() *******************/
/*
* saves file sequences out to disk as they become available
*/
void *file_saver(void *args)
{
  // snag and cast
  HandlingInfo *handlingInfo = (HandlingInfo *) args;
  //FileSequence *chunk = ((HandlingInfo *) handlingInfo)->head;

  // open a new file for writing at FILEPATH
  char *fullpath = get_full_filepath(baseDir, handlingInfo->entry->file->filepath);
  FILE *fp = fopen(fullpath, "wb");

  int n_written = 0;
  int n_expected = ceil((float)handlingInfo->entry->file->size / (float)data_len);

  // while there are still segments to read or there are still sheep out
  //while (handlingInfo->sheep > 0 || handlingInfo->head != NULL) {
  while (n_written < n_expected) {

    // if there's a chunk to deal with
    while (handlingInfo->head == NULL) {
      sleep(1); // since we're spin-locking
      continue;
    }

    n_written++;

    // lock the chunk
    pthread_mutex_lock(handlingInfo->queue_mutex);

    FileSequence *curSequence = handlingInfo->head;

    // seek to the right section in the file
    if (fseek(fp, (long int) curSequence->initSeg, SEEK_SET) != 0) {
      fprintf(stderr, "Error fseeking\n");
    }

    // write the segment to the file
    if (fwrite(curSequence->buf, sizeof(char), curSequence->length, fp) == 0) {
      fprintf(stderr, "Error fwrite\n");
    }

    // get the next chunk in the linked list
    handlingInfo->head = handlingInfo->head->next;

    // free the previous chunk
    free(curSequence->buf);
    free(curSequence);

    // unlock the curr chunk
    pthread_mutex_unlock(handlingInfo->queue_mutex);
  }

  printf("Done writing to %s\n", handlingInfo->entry->file->filepath);

  // close the file
  fclose(fp);

  // update its timestamp to last modified date
  struct utimbuf ubuf;
  memset(&ubuf, 0, sizeof(struct utimbuf));
  ubuf.modtime = handlingInfo->entry->file->last_modified;
  utime(fullpath, &ubuf);

  // callback
  download_complete_callback(handlingInfo->entry);

  // destroy/free global stuff
  pthread_mutex_destroy(handlingInfo->queue_mutex);
  pthread_mutex_destroy(handlingInfo->sheepMutex);
  free(handlingInfo->queue_mutex);
  free(handlingInfo->sheepMutex);
  free(handlingInfo->head);
  free(handlingInfo);
  free(fullpath);

  // leave
  pthread_exit(NULL);
}
