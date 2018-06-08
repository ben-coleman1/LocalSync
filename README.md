# LocalSync
## Fleetwood MAC ft. NAT King Cole

LocalSync implements a decentralized system of sharing files across devices,
using a RaspberryPi as a tracker node and individual devices as peer. The
implementation is as follows:

### Building & Running
`make` from the project root will build everything needed.

From the tracker directory, running `./tracker` will start a tracker.

From the peer directory, running `./peer [tracker hostname] [watch dir]` will 
start a peer process, connecting to the specified tracker and watching the 
given directory.

### Tracker
The tracker node maintains information about what peers and what files are
currently in the network. It provides information to peers about which peers
possess which files, and updates peer when it receives notice of file changes.
The tracker maintains a list of active peers and removes peers who become
inactive for a specified amount of time.

The tracker can run on a RaspberryPi and must be initialized 
before peers can connect.

### Peer
A device can become a peer node by connecting to the tracker on a pre-specified
port. Once connected, a peer will sync its current file directory with the
tracker to determine what files must be obtained from other peers to bring it
up to date. A peer also sends a "keep alive" message to the tracker periodically
and responds to other peers' requests for files.

A peer can be run on any device by using the peer.c files in the
peer directory.

### More Information
See Design Report at
https://docs.google.com/document/d/1ouJWGdzWkjcqUoYXNijWnEzzgpHguuIY7lebdpc4KWo/edit
for information on file sharing, file tables, and implementation details.
