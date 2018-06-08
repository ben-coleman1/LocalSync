#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include "peertable.h"

Peer *peer_init() {
  Peer *p = calloc(1, sizeof(Peer));
  if (p == NULL) {
    return NULL;
  }


  return p;
}

void peer_print(Peer *peer) {
  if (peer == NULL) {
    return;
  }

  printf("%s:%d \t last seen: %s", peer->ip, peer->listen_port, ctime(&peer->last_timestamp));
}

void peer_destroy(Peer *peer) {
  if (peer == NULL) {
    return;
  }

  printf("Destroying peer %s\n", peer->ip);

  pthread_cancel(peer->thread_id);
  pthread_join(peer->thread_id, NULL);

  // close the socket
  close(peer->sockfd);

  free(peer);
}

PeerTable *peertable_init() {
  PeerTable *pt = calloc(1, sizeof(PeerTable));
  if (pt == NULL) {
    return NULL;
  }

  pthread_mutex_init(&pt->lock, NULL);

  return pt;
}

// insert the peer at the front of the table
int peertable_add(PeerTable *table, Peer *peer) {
  if (table == NULL || peer == NULL) {
    return -1;
  }

  pthread_mutex_lock(&table->lock);

  peer->next = table->head;
  table->head = peer;

  pthread_mutex_unlock(&table->lock);

  return 1;
}

// find and remove the specified peer from the table
int peertable_remove(PeerTable *table, Peer *peer) {
  if (table == NULL || peer == NULL) {
    return -1;
  }

  pthread_mutex_lock(&table->lock);

  Peer *cur = table->head;
  Peer *prev = NULL;

  while (cur != NULL) { 
    // find the peer we wanted to remove
    if (cur == peer) {
      // if prev is null, that means we're at the head
      if (prev == NULL) {
        // just advance the head
        table->head = table->head->next;
      } else {
        // otherwise skip over the found peer
        prev->next = cur->next;
      }
      break;
    }

    prev = cur;
    cur = cur->next;
  }


  pthread_mutex_unlock(&table->lock);

  return 1;
}

void peertable_print(PeerTable *table) {
  if (table == NULL) {
    return;
  }

  Peer *cur = table->head;

  while (cur != NULL) {
    peer_print(cur);
    cur = cur->next;
  }
}

void peertable_destroy(PeerTable *table) {
  if (table == NULL) {
    return;
  }

  Peer *tmp;
  while (table->head != NULL) {
    tmp = table->head;

    peertable_remove(table, tmp);
    peer_destroy(tmp);
  }

  free(table);
}
