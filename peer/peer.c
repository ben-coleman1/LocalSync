/*
 * peer.c: The driver for a peer.
 *
 * See design doc for full details.
 * Peer starts the heartbeat thread, upload thread, watches files, and starts 
 * download threads as needed based on tracker updates.
 */

#include <signal.h>
#include <time.h>
#include "peer.h"
#include "../monitor/monitor.h"
#include "../upload_download/upload_download.h"
#include <ctype.h>
#include <string.h>

// inotify & FSEvent have a bit of a lag, so we wait this amount of time before unblocking
// an event from a file in order to ensure we actually ignore it 
#define WAIT_TIME (const struct timespec[]){{0, 300000000L}}

/***************** Global Variables *****************************************/
// A record of the files contained on this peer and other peers.

int num_streams;

// The maximum length of a piece of file sent between peers.
int piece_len;

// The interval with which to send a keep alive message to the tracker.
int interval = -1;

// The socket with which this peer has connected to the tracker.
int tracker_conn = -1;

// The peer's chosen port, and current ip.
int port_num;
char *my_ip;

// The tracker's hostname address.
char *tracker_host;

// the directory we are syncing
char *dir;

monitor *filemonitor;

// pthread sending keep alives
pthread_t heartbeat_thread_id;
pthread_t monitor_thread_id;
pthread_mutex_t comm_lock = PTHREAD_MUTEX_INITIALIZER;

/*********************** Functions ******************************************/

// Usage: port num to use, directory to sync, tracker hostname
int main(const int argc, char *argv[]) {
  // ignore any SIGPIPEs from the kernel
  signal(SIGPIPE, SIG_IGN);

  if (argc != 4) {
    printf("Usage: peer [tracker host] [watchDir] [streams]\n");
    exit(1);
  }

	// Validate number of streams
	for (int i = 0; i < strlen(argv[3]); i++) {
		if (!isdigit(argv[3][i])) {
			printf("Usage: [streams] must be a positive integer less than 51\n");
			exit(1);
		}
	}
	num_streams = atoi(argv[3]);
	if (num_streams > 50 || num_streams < 1) {
		printf("Usage: [streams] must be a positive integer less than 51\n");
		exit(1);
	}
	if (num_streams > 2) {
		printf("WARNING: This number of streams is intended for large files on");
		printf("small networks.\nDepending on directory size and network size");
		printf(" this can crash the program.\nFleetsync is not responsible for");
		printf(" data loss or crashes. Use at your own risk.\n");
		sleep(5);
	}

	// Validate arguments, creating the directory if it doesn't exist.
  tracker_host = argv[1];
  if (gethostbyname(tracker_host) == NULL) {
    printf("Unknown host.\n");
    printf("Usage: peer [tracker host] [watchDir]\n");
    exit(1);
  }
  dir = argv[2];
  if (mkdir(dir, 0777) != 0) printf("Created directory %s.\n", dir);
  srand(time(NULL));

  // Set the port number randomly.
  port_num = (rand() % 64311) + 1024;

  // when user closes, try to clean up nicely
  signal(SIGINT, stop_peer);

  // create the file monitor
  filemonitor = monitor_init(dir);
  if (filemonitor == NULL) {
    exit(1);
  }

  // start watching file changes
  monitor_start_watching(filemonitor);

  // connect to the tracker
  tracker_conn = initialize_connection(tracker_host);
  if (tracker_conn < 0) {
    printf("Unable to connect.\n");
    exit(1);
  }

  // register with the tracker
  if (register_with_tracker() < 0) {
    fprintf(stderr, "Couldn't register with the tracker.\n");
    exit(1);
  }

  // start the keep-alive/heartbeat thread
  if (pthread_create(&heartbeat_thread_id, NULL, heartbeat_thread, NULL) < 0) {
    perror("Error creating thread");
    return -1;
  }

  // and read those changes, sending them to the server
  if (pthread_create(&monitor_thread_id, NULL, read_monitor_thread, filemonitor) < 0) {
    perror("Error creating thread");
    exit(1);
  }

  // listens file upload handshake port indefinitely
  // start thread to listen for requests for files from peers
  init_upload(dir, port_num);

  my_ip = get_my_ip();
  printf("I am %s\n", my_ip);


  // Handle incoming file table update messages from the tracker
  Message msg;
  while (1) {
    memset(&msg, 0, sizeof(msg));

    int n_read = recv_message(tracker_conn, &msg);
    if (n_read <= 0) {
      printf("Got nothing from tracker. Exit.\n");
      break;
    }

    // after reading, recv_message will have updated msg.body to be the correct type
    // for each message

    switch (msg.type) {
      case TABLE_UPDATE:
        {
          TableUpdateBody *b = msg.body;
          FileTable *ft = b->table;

          // Manually set lock to null
          ft->lock = NULL;

          filetable_print(ft);

          update_from_filetable(ft);

          // clean up from this message
          filetable_destroy(ft);
          free(b);
        }
        break;
      default:
        //printf("got other message type %d \n", msg.type);
        break;
    }
  }
}

// should be called when the file has successfully completed its download
void download_complete_callback(TableEntry *fileentry) {
  if (fileentry == NULL) {
    return;
  }

  // create a file event to tell the server we now have the latest copy
  FileEvent *complete_evt = fileevent_init();
  if (complete_evt == NULL) {
    return;
  }

  complete_evt->action = DOWNLOAD_COMPLETE;
  complete_evt->file = fileentry->file;

  // tell the server we're done
  pthread_mutex_lock(&comm_lock);
  send_file_update(tracker_conn, complete_evt);
  pthread_mutex_unlock(&comm_lock);

  // resume accepting changes from the file
  nanosleep(WAIT_TIME, NULL); // wait to allow inotify/fsevent to flush first
  monitor_resume_modify(filemonitor, fileentry->file->filepath);

  // and we can now destroy the table entry
  tableentry_destroy(fileentry);
	complete_evt->file = NULL;
	fileevent_destroy(complete_evt);
}

char* get_directory_path (char* fullpath) {
  char* e = strrchr(fullpath, '/');
  if(!e){
    return NULL;
  }
  int index = (int)(e - fullpath);
  char* s = (char*) calloc(1, sizeof(char)*(index+1));
  strncpy(s, fullpath, index);
  s[index] = '\0';
  printf("s is %s\n", s);
  return s;
}

// called when there's an update to the specified file
void download_file(TableEntry *fileentry) {
  if (fileentry == NULL) {
    return;
  }

  // don't download anything that's already being downloaded
  nanosleep(WAIT_TIME, NULL); // but only after waiting for as long as it would take to remove
  if (fileset_contains(filemonitor->ignore_modify, fileentry->file->filepath)) {
    return;
  }

  // first make a clone of the table entry
  TableEntry *entry_clone = tableentry_clone(fileentry);

  printf("DOWNLOAD: ");
  filetable_entryprint(entry_clone);

  // mark the file as ignored to avoid generating an event
  monitor_ignore_modify(filemonitor, fileentry->file->filepath);

  // first, handle if this event is for a directory
  if (fileentry->file->is_dir) {
    char *fpath = get_full_filepath(dir, fileentry->file->filepath);

    // only send the callback when we actually create the directory
    if (mkdir(fpath, 0777) == 0) {
      download_complete_callback(entry_clone);
    }

    free(fpath);
  } else {
    // create the file as a placeholder until full download works
    char *path = get_full_filepath(dir, fileentry->file->filepath);
    FILE *fp = fopen(path, "w");
    free(path);

    if (fp == NULL) {
      path = get_full_filepath(dir, fileentry->file->filepath);
      char *dire = get_directory_path(path);
      if (dire != NULL) {
        mkdir(dire, 0777);
        fp = fopen(fileentry->file->filepath, "w");
        if (fp != NULL) {
          //download_complete_callback(entry_clone);
          return;
        }
      }
      printf("error opening file %s\n", path);
      return;
    }

    fclose(fp);
    init_download(dir, entry_clone, piece_len, num_streams);
  }
}

int delete_recursive(const char *fpath, const struct stat *sb,
            int tflag){
  return 0; 
}

int remove_dir(char *filepath) {
  char *full_path = get_full_filepath(dir, filepath);

  // This is a directory, for each file or subdirectory, recursively call this.
  ftw(full_path, delete_recursive, 0);

  // Delete the directory.
  if (rmdir(full_path) != 0) printf("Error removing directory.\n");
  return 0;
}

int is_dir(char *path) {
  char *fullpath = get_full_filepath(dir, path);
  DIR* ckdir = opendir(fullpath);
  free(fullpath);
  if (ckdir) {
    closedir(ckdir);
    return 0;
  }
  return 1;
}

// called when there's a table entry we need to delete
void delete_file(char *filepath) {
  if (filepath == NULL) {
    return;
  }
  
  // mark the file as ignored to avoid generating an event
  monitor_ignore_delete(filemonitor, filepath);

  // handle if it is a directory
  if (is_dir(filepath) == 0) {
    remove_dir(filepath);
  } else {
    // otherwise delete the file from disk
    char *path = get_full_filepath(dir, filepath);
    unlink(path);
    free(path);
  }

  // remove file from the ignore list
  nanosleep(WAIT_TIME, NULL); // wait to allow inotify/fsevent to flush first
  monitor_resume_delete(filemonitor, filepath);
}

void update_from_filetable(FileTable *ft) {
  if (ft == NULL) {
    return;
  }

  // get current status of files
  FileInfo_FS *files = monitor_get_current_files(filemonitor);

  FileInfo_FS *cur = files;
  while (cur != NULL) {
    // then, for each file, look it up in the filetable
    TableEntry *entry = filetable_getEntry(ft, cur->filepath);

    // if we didn't find it, that means we need to delete it locally
    if (entry == NULL) {
      // only delete if it's not a dotfile
      if (cur->filepath[0] != '.') {
        printf("Deleting %s\n", cur->filepath);
        delete_file(cur->filepath);
      }
    } else {
      // we only download when 1. the tracker thinks we don't have the latest
      // and 2. the modification times are off or the size is off
      int hasLatest = filetable_entryContainsPeer(entry, my_ip, port_num);
      if (!hasLatest && (cur->last_modified < entry->file->last_modified || cur->size != entry->file->size)) {
        // if our version is out of date or the wrong size
        // then start downloading the latest copy
        download_file(entry);
      } else {
        //printf("%s is up to date\n", cur->filepath);
      }
    }

    cur = cur->next;
  }


  // now we need to determine which files are new and need to be downloaded
  // done by inefficiently looking through both lists again.
  // This could be made more efficient if we ensured the lists were always sorted.
  TableEntry *entry = ft->head;

  while (entry != NULL) {
    FileInfo_FS *cur = files;

    // flag for whether to download this new file
    bool fileExists = false;

    while (cur != NULL) {
      // if the file in the table is here, don't need to download
      if (strcmp(cur->filepath, entry->file->filepath) == 0) {
        fileExists = true;
        break;
      }

      cur = cur->next;
    }

    if (!fileExists) {
      download_file(entry);
    }

    entry = entry->next;
  }

  fileinfo_destroy_all(files);
}

void *read_monitor_thread(void *arg) {
  monitor *m = (monitor *) arg;
  while (true) {
    FileEvent *e = monitor_get_events(m);
    FileEvent *tmp;

    pthread_mutex_lock(&comm_lock);
    send_file_update(tracker_conn, e);
    pthread_mutex_unlock(&comm_lock);

    printf("SEND EVENTS:\n");
    // clean up
    while (e != NULL) {
      // TODO update ignored files and current downloads as needed
      // if we deleted a file, stop current download if it exists (stopping download should also unignore)

      tmp = e->next;
      fileevent_print(e);
      fileevent_destroy(e);
      e = tmp;
    }
  }
}

// sends initial REGISTER packet to the tracker and waits for acknowledgement
int register_with_tracker() {
  // get all the files we currently have
  FileInfo_FS *files = monitor_get_current_files(filemonitor);

  // send files and registration info to the server
  send_register(tracker_conn, port_num, files);

  // then wait for initial acknowledgement with the filetable
  Message msg;
  if (recv_message(tracker_conn, &msg) == -1) {
    fprintf(stderr, "Couldn't receive register ack\n");
    exit(1);
  }

  // should always be a REGISTER_ACK; assert so
  if (msg.type != REGISTER_ACK) {
    fprintf(stderr, "Didn't receive register ack as first message after reg.\n");
    exit(1);
  }

  // cast body into the right type
  RegisterAckBody *b = msg.body;

  // then set local vars to meet with tracker's specs
  interval = b->interval;
  piece_len = b->piece_len;
  printf("Successfully registered. Piecelen: %d, interval: %d \n", piece_len, interval);

  // clean up from registration
  fileinfo_destroy_all(files);
  free(b);

  return 1;
}

// thread to loop forever, sending keep alive messages on every interval
void *heartbeat_thread(void *arg) {
  while (1) {
    sleep(interval);

    if (send_keep_alive(tracker_conn) == -1) {
      fprintf(stderr, "Couldn't send heartbeat.\n");
      exit(1);
    }
  }

  pthread_exit(0);
}

// Initialize a connection with the tracker on the HANDSHAKE_PORT
int initialize_connection(char *host_ip) {
   int comm_sock = socket(AF_INET, SOCK_STREAM, 0);
   if (comm_sock < 0) {
     perror("Problem creating socket");
     exit(2);
   }

  // Set up the socket.
  struct sockaddr_in server;
  server.sin_family = AF_INET;
  server.sin_port = htons(HANDSHAKE_PORT);

  struct hostent *hostp = gethostbyname(host_ip);

  // Check that the host is valid.
  if (hostp == NULL) {
    perror("Unknown host");
    exit(3);
  }
  memcpy(&server.sin_addr, hostp->h_addr_list[0], hostp->h_length);


  printf("Connecting to %s on port %d...\n", inet_ntoa(server.sin_addr), HANDSHAKE_PORT);

  // Try to connect to the socket.
  if (connect(comm_sock, (struct sockaddr *) &server, sizeof(server)) < 0) {
    perror("Error connecting socket");
    exit(4);
  }

  printf("Connected to the server.\n");
  return comm_sock;
}

void stop_peer() {
  printf("Stopping peer...\n");

  // stop and clean up file monitor
  if (filemonitor) {
    // first stop the thread that's reading from the monitor
    pthread_cancel(monitor_thread_id);
    pthread_join(monitor_thread_id, NULL);

    // then stop and destroy the file monitor itself
    monitor_stop_watching(filemonitor);
    monitor_destroy(filemonitor);
    filemonitor = NULL;
  }

  // stop and cleanly exit the heartbeat thread
  pthread_cancel(heartbeat_thread_id);
  pthread_join(heartbeat_thread_id, NULL);

  upload_destroy();

  // if it all went smoothly, exit clean
  exit(0);
}
