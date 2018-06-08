/*
 * filetable.h
 * 	Module for filetable.c
 * written by team Fleetwood MAC
 * CS60, May 2018
 */

#ifndef FILETABLE_H
#define FILETABLE_H


#include "../monitor/fileinfo.h"
#include "../monitor/fileevent.h"

/*										Structures									*/


// An IP structure
// Identifies a peer by its ip and port
typedef struct IP {
	// Ipaddress useful for up to ipv6
	char ip[40];
	// Port number
	int port;
	// Next element of the list
	struct IP *next;
} IP;

// An entry in the FileTable
typedef struct TableEntry {
	// Standard fileinfo struct
	FileInfo_FS *file;
	// Head of a list of peers
	IP *iphead;
	// Pointer to build the linked list
	int numpeers;
	struct TableEntry *next;
} TableEntry;

// The FileTable
typedef struct FileTable {
	// Total number of files
	int numfiles;

	// Head of linked list
	TableEntry *head;
	// Last modified?

	pthread_mutex_t *lock;
} FileTable;

/*              Function declarations             */

/*
 * Makes a copy of a table entry and returns it.
 */
TableEntry *tableentry_clone(TableEntry *entry);

/*
 * Prints an entry of the file table.
 */
void filetable_entryprint(TableEntry *entry);

/*
 * Destroys and frees a table entry struct.
 */
void tableentry_destroy(TableEntry *entry);

/*
 * filetable_init
 *  Initialize the filetable with head = NULL
 * ret: initialized table that must be free'd
 */
FileTable *filetable_init();

/*
 * filetable_destroy
 *  Free the entire filetable
 */
void filetable_destroy(FileTable *ft);

/*
 * filetable_insert
 *  Adds a file to the filetable in sorted alphanumeric order
 *  Assumes filename and creationip are null terminated
 * ret: 0 on success, -1 on null arg or filename exists in the table, -2 on arg length
 */
int filetable_insert(FileTable *ft, FileInfo_FS *file, char *creationip, int creationport);

/*
 * filetable_deleteFile
 *  Delete a file from the filetable
 * ret: 0 on success, -1 on failure
 */
int filetable_remove(FileTable *ft, char *filename);

/*
 * filetable_updateMod
 *  Updates the file's modification time and size
 *  Resets the ip list of the file to include only the peer who modified it
 *  Assumes the ip is already added in the file
 * Ret: 0 on success, -1 on failure
 */
int filetable_updateMod(FileTable *ft, FileInfo_FS *file, char *ip, int port);

/*
 * filetable_addPeer
 *  Add a peer to the list of peers who have the current version of a file
 * Ret: 0 on success, -1 on failure
 */
int filetable_addPeer(FileTable *ft, char *filename, char *ip, int port, int size);

/*
 * filetable_removePeerAll
 *  Remove peer from all files it is currently listed on
 * Ret: 0 on success, -1 on failure
 */
int filetable_removePeerAll(FileTable *ft, char *ip, int port);

/*
 * filetable_entryContainsPeer
 * Check if peer is in the listed table entry 
 * Ret: 1 if yes, 0 otherwise
 */
int filetable_entryContainsPeer(TableEntry *entry, char *ip, int port);

/*
 * filetable_getEntry
 * Get the file with name filename
 * Ret the table entry
 */
TableEntry *filetable_getEntry(FileTable *ft, char *filename);

/*
 * filetable_getPeers
 *  Finds and returns the peers with the newest version of the file
 * Ret: Linked-list of peers on success, NULL on failure
 *  Do not free the returned linked-list
 */
IP *filetable_getPeers(FileTable *ft, char *filename);

/*
 * filetable_merge
 * 	Performs union of provided files and files in the table
 * 	Creates a linked list of FileEvents by which
 * 	files is added to filetable ft
 */
FileEvent *filetable_merge(FileTable *ft, FileInfo_FS *files, char *ip, int port);

/*
 * filetable_fileDiff
 * 	Finds the file differences between ft1 and ft2
 * Ret: The file events needed to update ft1 as (ft1 U ft2)
 */
FileEvent *filetable_fileDiff(FileTable *ft1, FileInfo_FS *files);

/*
 * filetable_eventmerge
 * 	Performs updates to filetable ft, as directed by events e
 * 	e is a linked list of file events
 *
 * Note: Does nothing if ft is NULL
 */
void filetable_eventMerge(FileTable *ft, FileEvent *e, char *ip, int port);

/*
 * filetable_getNumPeers
 * Finds and returns the number of peers with the newest version of the file
 * Ret: the number of peers
 */
int filetable_getNumPeers(FileTable *ft, char *filename);

/*
 * filetable_print
 *  Prints out the filetable
 */
void filetable_print(FileTable *ft);

/*
 * filetable_entryprint
 * 	Prints out a TableEntry element
 */
void filetable_entryprint(TableEntry *entry);

/*
 * peer_print
 * 	Prints out a linked list of peers
 */
void filepeer_print(IP *peers);

/*
 * Receives a filetable struct and writes it to a local filetable struct.
 */
FileTable *filetable_receive(int fd);

/*
 * Sends a filetable struct.
 */
int filetable_send(int fd, FileTable *table);

#endif //FILETABLE_H
