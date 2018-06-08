/*
 * Methods to initialize a tracker node. 
 * Maintains and upates a file table and peer table, accepting events
 * and sending peers the updated table as needed. 
 */

#ifndef TRACKER_H
#define TRACKER_H

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
#include <pthread.h>
#include "../messaging/segment.h"
#include "../filetable/filetable.h"
#include "peertable.h"

// Definitions
#define MAX_PEERS 100
#define INTERVAL 5
#define PIECE_LENGTH 2048
#define IP_LEN INET_ADDRSTRLEN

// A method to start listening on the handshake_port.
int start_listening();

// Started by the main thread when a new peer connects to the tracker. Adds the
// peer to the peer table then listens for update messages from it.
void *handshake_thread(void *arg);

// Monitors and accepts alive messages from peers. Removes dead peers from the
// peer table.
void *monitor_thread(void *arg);

// Method to clean up after tracker is done.
void end_tracker();

#endif 
