/*
 * Methods and structures to handle communication between the tracker
 * and clients.
 *
 * Final project, CS60, Spring 2018
 *
 */


#include <sys/types.h>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <time.h>
#include <stdlib.h>
#include "segment.h"

// Method to create an update packet. Called by the peer.
/*void create_update_packet(seg_peer_t *segment, int port,
  FileTable *file_table) {
  segment->type = 0;
  segment->port = port;
  memcpy(segment->peer_ip, get_my_ip(), IP_LEN);
  segment->file_table = file_table;
}*/

/*
 * FileTable send/receive helpers
 */

int receive_table_update(int fd, Message *msg) {
  // create space for body
  TableUpdateBody *body = calloc(1, sizeof(TableUpdateBody));

  // read the body in
  if (recv(fd, body, sizeof(TableUpdateBody), MSG_WAITALL) < 0) {
    return -1;
  }

  FileTable *table = filetable_receive(fd);
  body->table = table;

  // attach to the message
  msg->body = body;

  return 1;
}

int send_table_update(int fd, FileTable *table) {
  if (table == NULL) { 
    return -1;
  }

  Message header;
  memset(&header, 0, sizeof(header));

  header.type = TABLE_UPDATE;

  if (send(fd, &header, sizeof(header), 0) < 0) {
    return -1;
  }  

  TableUpdateBody body;
  memset(&body, 0, sizeof(body));

  if (send(fd, &body, sizeof(body), 0) < 0) {
    perror("error sending");
    return -1;
  }

  if (filetable_send(fd, table) < 0) {
    printf("Error sending tabe\n");
  }

  return 1;
}


/*
 * REGISTER_ACK send/receive functions
 */

// tracker sends acknowledgement to peer with interval and piece length
int send_register_ack(int fd, int interval, int piece_len) {
  Message header;
  memset(&header, 0, sizeof(header));

  header.type = REGISTER_ACK;

  if (send(fd, &header, sizeof(header), 0) < 0) {
    perror("error sending");
    return -1;
  }  

  RegisterAckBody body;
  memset(&body, 0, sizeof(body));

  body.interval = interval;
  body.piece_len = piece_len;

  if (send(fd, &body, sizeof(body), 0) < 0) {
    perror("error sending");
    return -1;
  }
  
  return 1;
}

int receive_register_ack(int fd, Message *msg) {
  // create space for body
  RegisterBody *body = calloc(1, sizeof(RegisterAckBody));

  // read the body in
  if (recv(fd, body, sizeof(RegisterAckBody), MSG_WAITALL) < 0) {
    return -1;
  }

  // attach to the message
  msg->body = body;

  return 1;
}

/*
 * REGISTER send/receive functions
 */

// send a REGISTER message to the tracker, along with all files we currently have
int send_register(int fd, int listen_port, FileInfo_FS *files) {
  Message header;
  memset(&header, 0, sizeof(header));

  header.type = REGISTER;

  if (send(fd, &header, sizeof(header), 0) < 0) {
    perror("error sending");
    return -1;
  }  
  
  // count how many files
  int n_files = 0;
  FileInfo_FS *tmp = files;
  while (tmp != NULL) {
    n_files++;
    tmp = tmp->next;
  }

  // create body with number of files and port we're listening on  
  RegisterBody body;
  memset(&body, 0, sizeof(body));

  body.n_files = n_files;
  body.listen_port = listen_port;

  // send it off
  if (send(fd, &body, sizeof(body), 0) < 0) {
    return -1;
  }

  // and also send the list of files
  fileinfo_send_all(fd, files);

  return 1;
}

// populate a register message body
int receive_register(int fd, Message *msg) {
  // read the body
  RegisterBody *body = calloc(1, sizeof(RegisterBody));

  if (recv(fd, body, sizeof(RegisterBody), MSG_WAITALL) < 0) {
    perror("error receiving");
    return -1;
  }

  // a register message is followed by the file info list 
  body->files = fileinfo_receive(fd, body->n_files);

  msg->body = body;

  return 1;
}

/*
 * FILE_UPDATE send/receive functions
 */
int send_file_update(int fd, FileEvent *events) {
  Message header;
  memset(&header, 0, sizeof(header));

  header.type = FILE_UPDATE;

  if (send(fd, &header, sizeof(header), 0) < 0) {
    return -1;
  }  

  // count how many events
  int n_events = 0;
  FileEvent *tmp = events;
  while (tmp != NULL) {
    n_events++;
    tmp = tmp->next;
  }
  
  // create body with number of events 
  FileUpdateBody body;
  memset(&body, 0, sizeof(body));

  body.n_events = n_events;

  // send it off
  if (send(fd, &body, sizeof(body), 0) < 0) {
    return -1;
  }

  // and then finally send all the actual events
  fileevent_send_all(fd, events);

  return 1;
}

// populate a file update message body
int receive_file_update(int fd, Message *msg) {
  // read the body
  FileUpdateBody *body = calloc(1, sizeof(FileUpdateBody));

  if (recv(fd, body, sizeof(FileUpdateBody), MSG_WAITALL) < 0) {
    perror("error receiving");
    return -1;
  }

  // a register message is followed by the file info list 
  body->events = fileevent_receive(fd, body->n_events);

  msg->body = body;

  return 1;
}

/*
 * KEEP_ALIVE send function
 */
int send_keep_alive(int fd) {
  Message header;
  memset(&header, 0, sizeof(header));

  header.type = KEEP_ALIVE;

  if (send(fd, &header, sizeof(header), 0) < 0) {
    return -1;
  }  

  return 1;
}


// do-it-all receive a message. First reads a message type, then
// receives whatever structures are associated with that message.
// pairs with send_message. Idea is that tracker/client can send/receive
// linked structs that have pointers and get everything all at once, without
// knowing about underlying transport

int recv_message(int fd, Message *msg) {
  // attempt to read a message in 
  int n_read = recv(fd, msg, sizeof(Message), MSG_WAITALL);

  // client hung up
  if (n_read == 0) {
    return -1;
  }

  if (n_read <= 0) {
    perror("error receiving");
    return -1;
  }

  // now reconstruct body per message type
  switch (msg->type) {
    case REGISTER:
      receive_register(fd, msg);
      break;

    case REGISTER_ACK:
      receive_register_ack(fd, msg);
      break;

    case FILE_UPDATE:
      receive_file_update(fd, msg);
      break;

    case TABLE_UPDATE:
      receive_table_update(fd, msg);
      break;

    case KEEP_ALIVE:
      // no body content for keep-alive messages
      break;

    default:
    break;
  }

  return 1;
}

char *get_my_ip() {
  struct utsname uname_struct;
  if(uname(&uname_struct) != 0) {
    return NULL;
  }

  struct hostent *host = gethostbyname(uname_struct.nodename);
  if (host == NULL) {
    fprintf(stderr, "unknown host '%s'\n", uname_struct.nodename);
    return NULL;
  }

  char* ip = inet_ntoa(*(struct in_addr *)(host->h_addr_list[0]));
  return ip;
}

