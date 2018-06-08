/*
 *
 * functionality for passing fles down to requesting client.
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
#include <signal.h>
#include "upload_download.h"
#include "../filetable/filetable.h"
#include "../monitor/fileinfo.h"
#include <signal.h>


/******************* global vars ********************/
pthread_mutex_t *fileMutex; // Global for cancelability/freeing


/******************* function primitives *******************/
void *send_file(void *socket);
void *upload(void *nothin);

/******************* macros *******************/
#define LIST_PORT 8789 // port on which peers will be listening for pull reqs
#define LISTEN_BACKLOG 40 // number of peers we will service

pthread_t upload_thread;
char *baseDir;



/******************* init_upload() *******************/
/*
 * wrapper funtion for calling upload thread method
 */
void init_upload(char *dir, int listPort)
{
  baseDir = dir;

  int *port = calloc(1, sizeof(int));
  *port = listPort;

  // create and call the upload thread with peer info
  pthread_create(&upload_thread, NULL, &upload, (void *) port);
}




/******************* upload() *******************/
/*
 * thread method for handling download request
 * arg is the TableEntry for the file that we're requesting
 */
void *upload(void *port)
{

	// ignore any SIGPIPEs from the kernel
	signal(SIGPIPE, SIG_IGN);

	int p = *(int*)port;
	free(port);

  // init the mutex
  fileMutex = calloc(1, sizeof(pthread_mutex_t));
  pthread_mutex_init(fileMutex, NULL);


  int list_sock;
  struct sockaddr_in server; // server address
  struct sockaddr_in client; // client address
  int client_len = sizeof(client); // size of the client struct

  // create socket
  list_sock = socket(AF_INET, SOCK_STREAM, 0);
  if (list_sock < 0) {
    fprintf(stderr, "Error opening socket\n");
    exit(1);
  }

  // initialize the socket w/ my address
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = htonl(INADDR_ANY);
  server.sin_port = htons(p);

  // bind the socket to the address info
  if (bind(list_sock, (struct sockaddr*) &server, sizeof(server))) {
    fprintf(stderr,"Error binding socket name\n");
    exit(1);
  }
  printf("Listening at port %d\n", ntohs(server.sin_port));

  // listen for incoming clients
  listen(list_sock, LISTEN_BACKLOG);

  while (true) {

    int comm_sock;

    // accept client
    comm_sock = accept(list_sock, (struct sockaddr*) &client, (socklen_t*) &client_len);

    // check for unsucessful connection
    if (comm_sock == -1) {
      fprintf(stderr,"Error accepting client");
      continue;
    }

    //printf("Accepted upload request.\n");

    // allocate a handling info struct
    UpHandlingInfo *handlingInfo = calloc(1, sizeof(UpHandlingInfo));
    handlingInfo->sock = comm_sock;
    handlingInfo->fileMutex = fileMutex;

    // create a thread to upload the file
    pthread_t send_thread;
    if (pthread_create(&send_thread, NULL, send_file, (void *) handlingInfo) < 0) {
      fprintf(stderr, "Error creating thread\n");
    	exit(1);
		}
		pthread_detach(send_thread);

  }

  // close the listening socket
  close(list_sock);

	// exit
  exit(0);
}



/******************* send_file() *******************/
/*
 * parcels requested file segments, then sends them out to
 * client
 */
void *send_file(void *handlingInf)
{
  // snag and cast the struct
  UpHandlingInfo *handlingInfo = (UpHandlingInfo *) handlingInf;

  // get the socket
  int sock = handlingInfo->sock;

  SequenceInfo *seqInf = calloc(1,sizeof(SequenceInfo));

	while (true) {
		// read the sequence info
  	if (read(sock, seqInf, sizeof(SequenceInfo)) <= 0) {
			fprintf(stderr, "Download stopped by peer\n");
			break;
		}

  	// read the fileinfo
  	FileInfo_FS *fileInfo = fileinfo_receive(sock, 1);
  	//fileinfo_print(fileInfo);

  	// get initSeg
  	int initSeg = seqInf->initSeg;

		// breakout condition (termination message from downloaded)
		if (initSeg == -1) {
			fileinfo_destroy_all(fileInfo);
			break;
		}
  	// get length
  	int length = seqInf->length;

  	// get the filename
  	char *fileName = fileInfo->filepath;

  	// allocate a buffer to hold the sequence of chars
  	char *buf = calloc(length, sizeof(char));
  	if (buf == NULL) {
  		fprintf(stderr, "couldn't malloc buffer\n");
    	exit(1);
  	}

  	// lock the file
  	pthread_mutex_lock(handlingInfo->fileMutex);

  	// open the file
  	char *fullpath = get_full_filepath(baseDir, fileName);
  	FILE *fd = fopen(fullpath, "rb");

  	// Free path
  	free(fullpath);

  	if (fd == NULL) {
    	fprintf(stderr, "couldn't open file to read from\n");
    	pthread_exit(NULL);
 		}

  	if (fseek(fd, (long int) initSeg, SEEK_SET) != 0) {
    	fprintf(stderr, "fseek failed\n");
  	}

  	// for every byte, starting at initSeg
  	for(int i = 0; i<length; i++) {
    	// read the byte into the right spot in the buffer
    	if (fread(buf+i, 1, 1, fd) <= 0) {
      	printf("fread failed\n");

      	if (ferror(fd)) {
					fprintf(stderr, "ferror\n");
      	}

     		break;
    	}
  	}

  	// close the file
  	fclose(fd);

		// destroy fileinfo
  	fileinfo_destroy_all(fileInfo);

  	// unlock the file
  	pthread_mutex_unlock(handlingInfo->fileMutex);

  	// send the sequence to the peer
  	if (write(sock, buf, length) < 0) {
    	fprintf(stderr, "Download stopped by peer\n");
  		free(buf);
			break;
		}

		// free the buffer
		free(buf);

	}

	// free seqInf
	free (seqInf);

  // close the sock
  close(sock);

  // free passed arg
  free(handlingInf);

  // get outta here
  pthread_exit(NULL);
}



/******************* upload_destroy() *******************/
/*
 * kills upload process and frees associated memory
 */
void upload_destroy() {
  pthread_cancel(upload_thread);
  pthread_join(upload_thread, NULL);

	// free fileMutex
	pthread_mutex_destroy(fileMutex);
	free(fileMutex);
}
