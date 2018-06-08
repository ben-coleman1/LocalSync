/*
 * Methods to initialize a tracker node. 
 * Maintains and upates a file table and peer table, accepting events
 * and sending peers the updated table as needed. 
 */

#include <signal.h>
#include "tracker.h"
#include "peertable.h"


// Globals
PeerTable *peer_table;
FileTable *file_table;
pthread_t monitor_tid;

void accept_peers();

int listen_sock = -1;

int main() {
  // register handler to exit tracker nicely
  signal(SIGINT, end_tracker);

  // ignore any SIGPIPEs from the kernel
  signal(SIGPIPE, SIG_IGN);

  // Start listening on handshake_port for connections from peers.
  listen_sock = start_listening();
  if(listen_sock < 0) {
    perror("Error opening connection");
    exit(1);
  }

  printf("Listening on %d for peer connections; socket is %d.\n",
    HANDSHAKE_PORT, listen_sock);

  // Initialize file table and peer table.
  file_table = filetable_init();
  peer_table = peertable_init();

  // Create monitor thread.
  if (pthread_create(&monitor_tid, NULL, monitor_thread, NULL) < 0) {
    perror("Error creating thread");
    exit(2);
  }

  printf("Current ip is %s\n", get_my_ip());

  // loops forever, accepting peers as they connect
  accept_peers();

  // When the tracker is done, close the connection and exit.
  if (listen_sock != -1) {
    close(listen_sock);
    listen_sock = -1;
  }

  exit(0);
}

int start_peer(int peer_fd, char *peer_ipstr) {
  Peer *peer = peer_init();
  if (peer == NULL) {
    return -1;
  }

  peer->sockfd = peer_fd;
  strcpy(peer->ip, peer_ipstr); // copy the ip address into this peer

  printf("Connection started with client: %s\n", peer->ip);

  // Create a handshake thread to deal with this peer.
  if (pthread_create(&peer->thread_id, NULL, handshake_thread, peer) < 0) {
    perror("Error creating thread");
    return -1;
  }

  // if everything went well, add peer to the table
  peertable_add(peer_table, peer);

  return 1;
}

void accept_peers() {
  // Prepare to receive peer connections.
  int comm_socket;
  char addrstr[INET_ADDRSTRLEN];

  struct sockaddr_in client;            // address for this node to create socket
  socklen_t client_len = sizeof(client);


  // while the tracker is alive, continously accept new peers
  while (1) {
    comm_socket = accept(listen_sock, (struct sockaddr *) &client, &client_len);
    if (comm_socket < 0) {
      perror("Error accepting peer connection");
      return;
    }

    inet_ntop(AF_INET, &client.sin_addr, addrstr, INET_ADDRSTRLEN);

    printf("Accepting on %d\n", comm_socket);

    // for each peer, start up handshake threads etc.
    start_peer(comm_socket, addrstr);
  }
}


int start_listening() {
  struct sockaddr_in serverAddr;
  memset(&serverAddr, 0, sizeof(serverAddr));

  serverAddr.sin_family = AF_INET;
  serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serverAddr.sin_port = htons(HANDSHAKE_PORT);

  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) return -1;

  if (bind(sockfd, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) < 0) {
    return -1;
  }

  if (listen(sockfd, MAX_PEERS) < 0) {
    return -1;
  }

  return sockfd;
}

// send events to all peers except the specified one
void broadcast_table(Peer *exclude) {
  Peer *cur = peer_table->head;

  while (cur != NULL) {
    if (cur == exclude) {
      cur = cur->next;
      continue;
    }

    // send a filetable message
    send_table_update(cur->sockfd, file_table);

    cur = cur->next;
  }
}

void *handshake_thread(void *arg) {
  Peer *peer = (Peer *) arg;

  Message msg;

  // Handle packets received from the connection described by sockfd.
  //while (receive_peer_segment(peer->sockfd, &segment)) {
  while (1) {
    memset(&msg, 0, sizeof(msg));

    int n_read = recv_message(peer->sockfd, &msg);
    if (n_read <= 0) {
      break;
    }

    // after reading, recv_message will have updated msg.body to be the correct type
    // for each message

    switch (msg.type) {
      case REGISTER:
        {
          RegisterBody *b = msg.body;

          // update timestamp and port that the peer is listening on
          peer->last_timestamp = time(NULL);
          peer->listen_port = b->listen_port;

          printf("REGISTER message from %s : %d. %d files\n", peer->ip, peer->listen_port, b->n_files);

          FileInfo_FS *f = b->files;
          while (f != NULL) {
            fileinfo_print(f);
            f = f->next;
          }


          pthread_mutex_lock(file_table->lock);

          // merge those files into the file table
          // note that this entire section is critical, since we need to ensure if two peers
          // join, any events received are also sent in the correct order
          FileEvent *events = filetable_merge(file_table, b->files, peer->ip, b->listen_port);

          filetable_print(file_table);

          send_register_ack(peer->sockfd, INTERVAL, PIECE_LENGTH);

          // broadcast updated table to all clients
          broadcast_table(NULL);

          pthread_mutex_unlock(file_table->lock);

          // free all the file events we got
          FileEvent *tmp;
          while (events != NULL) {
            fileevent_print(events);
            tmp = events->next;
            free(events);
            events = tmp;
          }

          // free the fileinfo list
          fileinfo_destroy_all(b->files);
					// free the fileevents list
					//fileevent_destroy_all(((FileUpdateBody*)(msg.body))->events);
          // then free the body itself
          free(b);
        }

        break;

      // If the packet is a heartbeat, update peer entry.
      case KEEP_ALIVE:
        // update this peer's timestamp
        peer->last_timestamp = time(NULL);


        break;

      // If the packet is an update type packet, do file table stuff.
      case FILE_UPDATE:
        {
          // update the peer's timestamp
          peer->last_timestamp = time(NULL);

          FileUpdateBody *b = msg.body;

          printf("===== FILE_UPDATE from %s ====\n", peer->ip);
          FileEvent *e = b->events;
          while (e != NULL) {
            fileevent_print(e);
            e = e->next;
          }
          printf("============================================\n");

          pthread_mutex_lock(file_table->lock);
          filetable_eventMerge(file_table, b->events, peer->ip, peer->listen_port);

          filetable_print(file_table);

          // broadcast updated table to other clients
          broadcast_table(peer);

          pthread_mutex_unlock(file_table->lock);
        	
					// free memory
					fileevent_destroy_all(b->events);
					free(b);
				}
        break;

      default:
        fprintf(stderr, "Got other message type\n");
        break;
    }
  }

  // remove peer from all places it appears in the file table
  pthread_mutex_lock(file_table->lock); 
  filetable_removePeerAll(file_table, peer->ip, peer->listen_port);
  pthread_mutex_unlock(file_table->lock); 

  filetable_print(file_table);

  // exit thread
  pthread_exit(0);
}

void *monitor_thread(void *arg) {
  // Every INTERVAL seconds, remove any peers from the table that are past their
  // timeout length and end their threads.
  while(1) {
    sleep(INTERVAL + 5); // allow a small buffer zone over the interval

    time_t time_now = time(NULL);

    Peer *p = peer_table->head;
    Peer *tmp;

    while (p != NULL) {
      tmp = p->next;
      // Check if the peer has timed out.

      if (difftime(time_now, p->last_timestamp) > INTERVAL) {
        // update file table, removing this peer's ip from all places it appears
        pthread_mutex_lock(file_table->lock); 
        filetable_removePeerAll(file_table, p->ip, p->listen_port);
        pthread_mutex_unlock(file_table->lock); 

        // this is marginally inefficient (we could just update pointers here)
        // but it's worth it for maintaining clarity/permitting peertable design to change
        peertable_remove(peer_table, p);

        // stop that peer's thread/free it
        peer_destroy(p);
      }

      p = tmp;
    }

    // Reset the file table if no peers remain.
    if (peer_table->head == NULL) {
      filetable_destroy(file_table);
      file_table = NULL;
      file_table = filetable_init();
    }
  }

  pthread_exit(0);
}

void end_tracker() {
  pthread_cancel(monitor_tid);
  pthread_join(monitor_tid, NULL);

  // close connection
  if (listen_sock != -1) {
    close(listen_sock);
    listen_sock = -1;
  }

  // clean up peer and file tables
  peertable_destroy(peer_table);
  filetable_destroy(file_table);
}
