/*
 * filetable.c for mantaining a file table
 *
 * written by team Fleetwood MAC
 * CS60, May 2018
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

#include "filetable.h"
#include "fileinfo.h"
#include "fileevent.h"


/*          Local function declarations           */

static void ip_destroy(IP *iphead);
void tableentry_destroy(TableEntry *entry);


/*      Public functions                */

// filetable_init
FileTable *filetable_init()
{
  FileTable *ft = calloc(1, sizeof(FileTable));
  if (ft == NULL) {
    fprintf(stderr, "malloc\n");
    exit(1);
  }
  ft->head = NULL;
  ft->numfiles = 0;

  ft->lock = calloc(1, sizeof(pthread_mutex_t));
  pthread_mutex_init(ft->lock, NULL);

  return ft;
}

// release all memory associated with a filetable
void filetable_destroy(FileTable *ft)
{
  if (ft == NULL) {
    return;
  }

  // Destroy each entry
  for (TableEntry *cur = ft->head; cur != NULL; cur = ft->head) {
    ft->head = cur->next;
    tableentry_destroy(cur);
  }

  // Destroy the lock
  if(ft->lock != NULL) {
    pthread_mutex_destroy(ft->lock);
    free(ft->lock);
  }

  // Free the struct
  free(ft);
}

// insert file into the table, with peer listening at ip/port
int filetable_insert(FileTable *ft, FileInfo_FS *file, char *creationip, int creationport)
{
  if (ft == NULL || file == NULL || creationip == NULL) {
    return -1;
  }

  // if a dotfile in main dir, do nothing
  if (file->filepath[0] == '.') {
    return 1;
  }

  // always make size 0 for directories
  if (file->is_dir) {
    file->size = 0;
  }

  // if file already exists, just update it instead of inserting twice
  if (filetable_updateMod(ft, file, creationip, creationport) != -1) {
    return 1;
  }

  // Get the file's place in the filetable
  TableEntry *prv = NULL;
  TableEntry *cur;
  for (cur = ft->head; cur != NULL; cur = cur->next) {
    if (strcmp(file->filepath, cur->file->filepath) < 0) {
      break;
    }
    prv = cur;
  }

  // Look and see if file already exists (check if prv == file)
  if (prv != NULL &&
      (strcmp(file->filepath, prv->file->filepath) == 0)) {
    return -1;
  }

  // Copy the file
  FileInfo_FS *file_copy = fileinfo_init();
  if (file_copy == NULL) {
    fprintf(stderr, "malloc err\n");
    exit(1);
  }

  memcpy(file_copy, file, sizeof(FileInfo_FS));

  // Create the table entry and file info
  TableEntry *entry = calloc(1, sizeof(TableEntry));
  if (entry == NULL) {
    fprintf(stderr, "malloc\n");
    exit(1);
  }

  entry->file = file_copy;

  // Create the ip address linked list
  IP *ip = calloc(1, sizeof(IP));
  if (ip == NULL) {
    fprintf(stderr, "malloc\n");
    exit(1);
  }
  // Copy the ip address
  strcpy(ip->ip, creationip);
  // Set the port
  ip->port = creationport;
  ip->next = NULL;

  // Set the number of peers
  entry->numpeers = 1;

  // Link ip address to entry
  entry->iphead = ip;

  // now insert the file in the correct place
  if (prv == NULL) {
    entry->next = ft->head; // Insert at the head
    ft->head = entry;
  }
  else {
    entry->next = prv->next; // Insert between prv and prv->next
    prv->next = entry;
  }
  // Increment number of files
  ft->numfiles++;
  return 0;
}

// remove given filename from the file table
int filetable_remove(FileTable *ft, char *filename)
{
  if (ft == NULL || filename == NULL) {
    return -1;
  }
  // Iterate through files
  TableEntry *prv = NULL;
  TableEntry *cur;
  int cmp;
  for (cur = ft->head; cur != NULL; cur = cur->next) {
    cmp = strcmp(filename, cur->file->filepath);
    if (cmp == 0) {
      break;
    }
    prv = cur;
  }

  // Filename not found
  if (cur == NULL) {
    return -1;
  }

  // If removing a directory
  if (cur->file->is_dir) {
    char *dirname = calloc(1, sizeof(cur->file->filepath) + 1);
    if(dirname == NULL) {
      return -1;
    }

    strcpy(dirname, cur->file->filepath);

    // Delete all the files in the directory, recursively
    int dirlen = strlen(dirname);
    cur->file->is_dir = false; // prevent infinite recursive loop
    for (TableEntry *sub = ft->head; sub != NULL; sub = sub->next) {
      if (strncmp(dirname, sub->file->filepath, dirlen) == 0 && strcmp(dirname, sub->file->filepath) != 0) {
        printf("remove sub file %s\n", sub->file->filepath);
        filetable_remove(ft, sub->file->filepath);
      }
    }

    free(dirname);
  }

  // if deleted the head, update the head
  if (prv == NULL) {
    ft->head = cur->next;   // The file was first in the list
  }
  else { // otherwise skip over removed file in the list 
    prv->next = cur->next;
  }

  // Destroy cur
  tableentry_destroy(cur);

  // Decrement the number of files
  ft->numfiles--;

  return 0;
}

// update filetable with file that has been modified
// @return 0 on success
int filetable_updateMod(FileTable *ft, FileInfo_FS *file, char *ip, int port)
{
  if (ft == NULL || file == NULL) {//filename == NULL || ip == NULL) {
    return -1;
  }

  int iplen = strlen(ip);
  if (iplen + 1 > 40) {
    return -2;
  }

  // iterate through the filetable to find the file
  TableEntry *cur;
  for (cur = ft->head; cur != NULL; cur = cur->next) {
    if (strcmp(file->filepath, cur->file->filepath) == 0) {
      break;
    }
  }

  // File not found
  if (cur == NULL) {
    return -1;
  }

  // Update the timestamp and size
  cur->file->last_modified = file->last_modified;
  cur->file->size = file->size;

  // delete every peer except the one that modified it
  ip_destroy(cur->iphead); // Delete the peer list

  // then insert the peer that modified it as the only valid peeer
  cur->iphead = calloc(1, sizeof(IP)); 
  if (cur->iphead == NULL) {
    fprintf(stderr, "malloc\n");
    exit(1);
  }

  memcpy(cur->iphead->ip, ip, iplen + 1); // Copy the ip
  cur->iphead->port = port;
  cur->iphead->next = NULL;
  
  // reset the number of peers to 1
  cur->numpeers = 1;

  return 0;
}

// add peer listening on ip/port to the specified file
// this can essentially be viewed as a DOWNLOAD_COMPLETE handler, hence the size arg
int filetable_addPeer(FileTable *ft, char *filename, char *ip, int port, int size)
{
  if (ft == NULL || filename == NULL || ip == NULL) {
    return -1;
  }

  TableEntry *entry = filetable_getEntry(ft, filename);
  // File not found
  if (entry == NULL) {
    return -1;
  }

  // only add peer if it has the full file
  if (entry->file->size != size) {
    return 0;
  }

  // if this peer already in the list, don't add as a duplicate
  IP *cur = entry->iphead;
  while (cur != NULL) {
    if (strcmp(cur->ip, ip) == 0) {
      return 0;
    }
    cur = cur->next;
  }

  // Add a peer to the peer list
  IP *newpeer = calloc(1, sizeof(IP));
  if (newpeer == NULL) {
    fprintf(stderr, "malloc\n");
    exit(1);
  }
  //  Copy the ip
  int iplen = strlen(ip);
  memcpy(newpeer->ip, ip, iplen + 1);
  //  Set the port
  newpeer->port = port;

  //  Link to the head of the list
  newpeer->next = entry->iphead;
  entry->iphead = newpeer;
  entry->numpeers++;

  return 0;
}

// check if the entry contains the specified peer 
// @return 0 if it does not, 1 if it does
int filetable_entryContainsPeer(TableEntry *entry, char *ip, int port)
{
  if (entry == NULL || ip == NULL) {
    return 0;
  }

  IP *head = entry->iphead;
  while (head != NULL) {
    if (strcmp(head->ip, ip) == 0 && port == head->port) {
      return 1;
    }
    head = head->next;
  }

  return 0;
}

// remove peer ip/port from this entry if it exists
int filetable_entryRemovePeer(TableEntry *entry, char *ip, int port)
{
  if (entry == NULL || ip == NULL) {
    return -1;
  }

  IP *prev = NULL;
  IP *cur = entry->iphead;
  while (cur != NULL) {
    if (strcmp(ip, cur->ip) == 0 && port == cur->port) {
      // when we find it, break
      break;
    }

    prev = cur;
    cur = cur->next;
  }

  // peer not found, return
  if (cur == NULL) {
    return 0;
  }

  if (prev == NULL) {
    entry->iphead = entry->iphead->next;
  } else {
    prev->next = cur->next;
  }

  entry->numpeers--;

  free(cur);

  return 0;
}

// remove peer from all places it appears
int filetable_removePeerAll(FileTable *ft, char *ip, int port)
{
  if (ft == NULL || ip == NULL) {
    return -1;
  }

  TableEntry *cur = ft->head;
  while (cur != NULL) {
    filetable_entryRemovePeer(cur, ip, port);
    cur = cur->next;
  }

  return 0;
}

// filetable_getEntry
TableEntry *filetable_getEntry(FileTable *ft, char *filename)
{
  if (ft == NULL || filename == NULL) {
    return NULL;
  }
  // Iterate through the filetable to find the file
  TableEntry *cur;
  for (cur = ft->head; cur != NULL; cur = cur->next) {
    if (strcmp(filename, cur->file->filepath) == 0) {
      break;
    }
  }
  // Return the result. Could be null if entry not found
  return cur;
}

// filetable_getPeers
IP *filetable_getPeers(FileTable *ft, char *filename)
{
  TableEntry *entry = filetable_getEntry(ft, filename);
  if (entry == NULL) {
    return NULL;
  }
  return entry->iphead;
}

// filetable_getNumPeers
int filetable_getNumPeers(FileTable *ft, char *filename)
{
  TableEntry *entry = filetable_getEntry(ft, filename);
  if (entry == NULL) {
    return 0;
  }
  return entry->numpeers;
}

// filetable_merge
// used to merge a complete list of files on a peer into a file table
// @return the FileEvents needed to make the table match, which can then be broadcasted
FileEvent *filetable_merge(FileTable *ft, FileInfo_FS *peer_files, char *ip, int port)
{
  FileEvent *events = filetable_fileDiff(ft, peer_files);
  filetable_eventMerge(ft, events, ip, port);
  // Free events
  /*for (FileEvent *cur = events; cur != NULL; cur = events) {
    fileevent_print(cur);
    events = events->next;
  }*/
  return events;
}

// filetable_fileDiff
// compare files against what's currently in table, and generate the correct
// FileEvents needed to make table match the files
FileEvent *filetable_fileDiff(FileTable *ft, FileInfo_FS *files)
{
  // Nothing to add
  if (files == NULL) {
    return NULL;
  }

  FileInfo_FS *tomerge = files;
  FileEvent *events = NULL;

  // Iterate through files to merge
  while (tomerge != NULL) {
    // event for the current fileinfo
    FileEvent *evt = NULL;

    // first, see if the entry is in the table
    TableEntry *entry = filetable_getEntry(ft, tomerge->filepath);

    // if it isn't, insert and make a CREATE event
    if (entry == NULL) {
      // FILE_CREATED file event
      evt = fileevent_init();
      if (evt == NULL) {
        fprintf(stderr, "malloc\n");
        exit(1);
      }

      evt->action = FILE_CREATED;
      evt->file = tomerge;
    } else {
      // there's an entry with that filename
      // if the file on client is newer, use it instead
      if (entry->file->last_modified < tomerge->last_modified) {
        // FILE_MODIFIED file event
        evt = fileevent_init();
        if (evt == NULL) {
          fprintf(stderr, "malloc\n");
          exit(1);
        }
        evt->action = FILE_MODIFIED;
        evt->file = tomerge;
      } else if (entry->file->last_modified == tomerge->last_modified) {

        // if they're the same last modified, we add this peer as having the latest
        // by using the download complete event

        evt = fileevent_init();
        if (evt == NULL) {
          fprintf(stderr, "malloc\n");
          exit(1);
        }
        evt->action = DOWNLOAD_COMPLETE;
        evt->file = tomerge;

      }
    }

    // Insert the file event at the head
    if (evt != NULL) {
      if (events != NULL) {
        evt->next = events;
        events = evt;
      }
      else {
        evt->next = NULL;
        events = evt;
      }
    }

    // move to next file
    tomerge = tomerge->next;
  }

  return events;
}


// update table based on events from specified peer w/ ip/port
void filetable_eventMerge(FileTable *ft, FileEvent *e, char *ip, int port)
{
  if (ft == NULL || e == NULL || ip == NULL) {
    return;
  }

  // Iterate through events 
  for (FileEvent *cur = e; cur != NULL; cur = cur->next) {
    // for each event, just update, insert, remove, or add peer based on the
    // event action type
    switch(cur->action) {
      case FILE_MODIFIED:
        filetable_updateMod(ft, cur->file, ip, port);
        break;

      case FILE_CREATED:
        filetable_insert(ft, cur->file, ip, port);
        break;

      case FILE_DELETED:
        filetable_remove(ft, cur->file->filepath);
        break;

      case DOWNLOAD_COMPLETE:
        filetable_addPeer(ft, cur->file->filepath, ip, port, cur->file->size);
        break;
    }
  }
}



// print the entire file table, with peers and entries
void filetable_print(FileTable *ft)
{
  printf("=============== filetable (%4d entries) ===============\n", ft->numfiles);
  if (ft != NULL) {
    printf("# Peers | Size     | Last Modified | Filepath \n");
    printf("--------------------------------------------\n");
    for (TableEntry *cur = ft->head; cur != NULL; cur = cur->next) {
      filetable_entryprint(cur);
    }
  }
  else {
    printf("               (null)\n");
  }
  printf("==========================================================\n");
}



// print out a file entry 
void filetable_entryprint(TableEntry *entry)
{
  if (entry == NULL) {
    return;
  }
  printf("%6d | %8d | %13ld | %s\n      \\___ ", entry->numpeers, entry->file->size,
    entry->file->last_modified, entry->file->filepath);
  filepeer_print(entry->iphead);
}



// print out a peer in human-readable form
void filepeer_print(IP *peers)
{
  if (peers != NULL) {
    for (IP *cur = peers; cur != NULL; cur = cur->next) {
      printf("%s:%d   ", cur->ip, cur->port);
    }
    printf("\n");
  }
  else {
    printf("(null)\n");
  }
}

// frees all memory associated with entry
void tableentry_destroy(TableEntry *entry)
{
  if (entry == NULL) {
    return;
  }

  fileinfo_destroy(entry->file);
  ip_destroy(entry->iphead);
  free(entry);
}


// returns a copy of the table entry
TableEntry *tableentry_clone(TableEntry *entry)
{
  TableEntry *clone = calloc(1, sizeof(TableEntry));
  if (clone == NULL) {
    return NULL;
  }

  clone->file = calloc(1, sizeof(FileInfo_FS));
  memcpy(clone->file, entry->file, sizeof(FileInfo_FS));

  clone->numpeers = entry->numpeers;

  IP *peer = entry->iphead;
  while (peer != NULL) {
    IP *clone_p = calloc(1, sizeof(IP));
    if (clone_p == NULL) {
      return NULL;
    }

    memcpy(clone_p, peer, sizeof(IP));

    clone_p->next = clone->iphead;
    clone->iphead = clone_p;

    peer = peer->next;
  }

  return clone;
}


/*                  local functions                 */

/*
 * ip_destroy
 *  Given the head of a IP linked list, delete it
 */
void ip_destroy(IP *iphead)
{
  for (IP *cur = iphead; cur != NULL; cur = iphead) {
    iphead = cur->next;
    free(cur);
  }
}



FileTable *filetable_receive(int fd) {
  FileTable *table = calloc(1, sizeof(FileTable));
  if (table == NULL) {
    return NULL;
  }

  // read in the filetable struct
  if (recv(fd, table, sizeof(FileTable), MSG_WAITALL) < 0) {
    return NULL;
  }

  // reset table pointers
  table->head = NULL;

  // for each entry we are expecting
  for (int i = 0; i < table->numfiles; i++) {

    // create space for this entry
    TableEntry *entry = calloc(1, sizeof(TableEntry));
    if (entry == NULL) {
      return NULL;
    }

    // read in the entry
    if (recv(fd, entry, sizeof(TableEntry), MSG_WAITALL) < 0) {
      return NULL;
    }

    // reset pointers
    entry->file = NULL;
    entry->iphead = NULL;

    // next thing we expect is the fileinfo struct
    FileInfo_FS *info = fileinfo_init();
    if (info == NULL) {
      return NULL;
    }

    // read in the fileinfo
    if (recv(fd, info, sizeof(FileInfo_FS), MSG_WAITALL) < 0) {
      return NULL;
    }

    // set the entry's fileinfo
    entry->file = info;

    // read in all the peers
    for (int j = 0; j < entry->numpeers; j++) {
      IP *cur_ip = calloc(1, sizeof(IP));
      if (cur_ip == NULL) {
        return NULL;
      }

      // read in this IP
      if (recv(fd, cur_ip, sizeof(IP), MSG_WAITALL) < 0) {
        return NULL;
      }

      // link it to the entry's list
      cur_ip->next = entry->iphead;
      entry->iphead = cur_ip;
    }

    // add this entry into the table
    entry->next = table->head;
    table->head = entry;
  }

  return table;
}

int filetable_send(int fd, FileTable *table) {
  if (table == NULL) {
    return -1;
  }

  // first send the table struct w/ the meta info
  if (send(fd, table, sizeof(FileTable), 0) <= 0) {
    perror("error sending");
    return -1;
  }

  TableEntry *entry = table->head;

  // then send each entry
  while (entry != NULL) {
    // send the struct
    if (send(fd, entry, sizeof(TableEntry), 0) <= 0) {
      perror("error sending");
      return -1;
    }

    // followed by the fileinfo
    if (send(fd, entry->file, sizeof(FileInfo_FS), 0) <= 0) {
      perror("error sending");
      return -1;
    }

    // and finally all of the peers
    IP *cur_ip = entry->iphead;
    while (cur_ip != NULL) {
      if (send(fd, cur_ip, sizeof(IP), 0) <= 0) {
        perror("error sending");
        return -1;
      }
      cur_ip = cur_ip->next;
    }

    entry = entry->next;
  }

  return 1;
}
