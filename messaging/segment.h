/*
 * Methods and structures to handle communication between the tracker
 * and clients. It also defines methods to create and initalize segments.
 *
 * Final project, CS60, Spring 2018
 *
 */

#ifndef SEGMENT_H
#define SEGMENT_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include "../monitor/fileinfo.h"
#include "../monitor/fileevent.h"
#include "../filetable/filetable.h"

// Segment type declarations for segments sent from peer to tracker:
typedef enum {
  ERROR,              // type should never be 0
  REGISTER,
  REGISTER_ACK,
  KEEP_ALIVE,
  TABLE_UPDATE,
  FILE_UPDATE,        // used by peer to inform server of changes, and then broadcasted to others
} MessageType;

#define HANDSHAKE_PORT 9571
#define IP_LEN INET_ADDRSTRLEN

// layer of encapsulation to hide away how the structs are actually sent/reconstructed
// over the socket. Depending on what `type` is, body will point to the corresponding Body
// struct as defined below. The function `recv_message` will handle loading the body correctly,
// so the external consumer only needs to call that and then can trust that the body will be
// populated as needed.

// generic message struct that will hold all communication sent between tracker/peer
typedef struct {
  MessageType type;     // the type of the message being sent
  void *body;           // will hold the correct body for the message type; one of the below 
} Message;

typedef struct {
  int listen_port;      // port that this peer is listening for p2p connections
  int n_files;          // number of files that are about to be sent
  FileInfo_FS *files;      // files on the tracker at initialization 
} RegisterBody;

typedef struct {
  int interval;         // port that this peer is listening for p2p connections
  int piece_len;        // how large the chunks of files are
} RegisterAckBody;

typedef struct {
  int n_events;         // number of file events 
  FileEvent *events;    // file events that occurred
} FileUpdateBody;

typedef struct {
  FileTable *table;    // the new file table
} TableUpdateBody;

// Method to get the ip address of the current peer.
char *get_my_ip();

int recv_message(int fd, Message *msg);

int send_register(int fd, int listen_port, FileInfo_FS *files);

int send_register_ack(int fd, int interval, int piece_size);

int send_keep_alive(int fd);

int send_file_update(int fd, FileEvent *event);

int send_table_update(int fd, FileTable *table);

#endif //SEGMENT_H
