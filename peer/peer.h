/*
 * peer.h: declarations of functions used in peer.c to connect 
 * peer with tracker. 
 */

#ifndef PEER_H
#define PEER_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <netdb.h>
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include <ftw.h>
#include <fts.h>
#include <pthread.h>
#include "../messaging/segment.h"
#include "../filetable/filetable.h"
#include "../upload_download/upload_download.h"


void download_complete_callback(TableEntry *fileentry);

/*
 * connect to the tracker
 */
int initialize_connection(char *hostname);

/*
 * Initialize the heartbeat thread.
 */
int heartbeat_init(pthread_t *t_id);

/*
 * sends a REGISTER message and waits for REGISTER_ACK from the tracker
 * before returning. Sets piece length and keep alive interval.
 */
int register_with_tracker();

/*
 * thread to send a KEEP_ALIVE to tracker at the proper interval
 */
void *heartbeat_thread(void *arg);

/*
 * thread to read events from the file monitor and send them to the tracker
 */
void *read_monitor_thread(void *arg);

/*
 * signal handler to clean up/free memory used by this peer
 */
void stop_peer();

/*
 * updates local files to match the provided filetable
 */
void update_from_filetable(FileTable *table);

/*
 * Deletes the file at filepath.
 */
void delete_file(char *filepath);

/*
 * A callback for ftw that calls the delete_file method on a passed filepath.
 */
int delete_recursive(const char *fpath, const struct stat *sb,
            int tflag);

/*
 * Deletes specified directory
 */
int remove_dir(char *filepath);

/*
 * Checks if path is a directory
 */
int is_dir(char *path);

#endif 
