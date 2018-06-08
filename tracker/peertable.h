/*
 * peertable.h: structure and functions used by tracker to maintain 
 * a list of all the peers currently connected
 */

#ifndef PEERTABLE_H
#define PEERTABLE_H

#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>

typedef struct Peer {
  // how to access this peer
  char ip[INET_ADDRSTRLEN];
  int listen_port;
  time_t last_timestamp;  // last checkin time
  int sockfd;             // socket to talk to this peer from the tracker
  pthread_t thread_id;    // handshake thread id for the peer

  struct Peer *next;
} Peer;

typedef struct {
  Peer *head;
  pthread_mutex_t lock;
} PeerTable;

/*
 * Creates and returns new empty peer struct
 * @return Peer* on success, NULL on error
 */
Peer *peer_init();

/*
 * Destroy a peer/release all memory associated
 */
void peer_destroy(Peer *peer);

/*
 * Creates and returns a new peer table
 * @return PeerTable* on success, NULL on error
 */
PeerTable *peertable_init();

/*
 * Add the peer to the table.
 * @return 1 on success, -1 on error
 */
int peertable_add(PeerTable *table, Peer *peer);

/*
 * Remove the peer from the table.
 * @return 1 on success, -1 on error
 */
int peertable_remove(PeerTable *table, Peer *peer);

/*
 * Print the table in a human-readable way
 */
void peertable_print(PeerTable *table);

/*
 * Destroy a peer table/release all memory associated
 */
void peertable_destroy(PeerTable *table);

#endif
