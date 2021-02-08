# GRPC

This project implements a simple distributed file system (DFS). Several file transfer protocols have been developed using gRPC and Protocol Buffers.A weakly consistent synchronization system has been incorporated to manage cache consistency between multiple clients and a single server. The system handles both binary and text-based files.

The source code uses a combination of C++14, gRPC, and Protocol Buffers.

# Design

## The GRPC service uses the following message types and methods for File transfer services.
<ul>
	<li>A "File" message type for storing a file's meta data like File size, modified time, contents of the file in chunks.<\li>
	<li>A "Request" message type for storing a request's meta data like requested filename.<\li>
	<li>A "StoreFile" method that takes as input a stream of "File" message type, each instance of "File" having the subsequent chunks of the file to be stored in the server.<\li>
	<li>A "FetchFile" method that takes as input a "request" message type and returns a stream of "File" message type, each instance of "File" having the subsequent chunks of the file requested from the server.<\li>
	<li>A "ListFiles" method that returns a stream of "File" message type, representing the various files that are available in the server.<\li>
	<li>A "GetAtt" method that takes as input a "request" message type and returns a "File" message type with the meta data of the requested file.
<br/>
		
## The DFS service uses the following message types and methods for File transfer services.
<ul>
	<li>A "File" message type for storing a file's meta data like File size, modified time, checksum, contents of the file in chunks.<\li>
	<li>A "Request" message type for storing a request's meta data like requested filename, clientId.<\li>
	<li>A"StoreFile" method that takes as input a stream of "Request" message type, each instance of "Request" having the subsequent chunks of the file to be stored in the server and the client context.<\li>
	<li>A "FetchFile" method that takes as input a "request" message type and returns a stream of "File" message type, each instance of "File" having the subsequent chunks of the file requested from the server.<\li>
	<li>A "ListFiles" method that returns a stream of "File" message type, representing the various files that are available in the server.<\li>
	<li>A "GetAtt" method that takes as input a "request" message type and returns a "File" message type with the meta data of the requested file.<\li>
	<li>A "AcquireLock" method that takes as input a "request" message type containing the client context and the filename for which the lock is to be acquired and returns a status of OK if the client has succesfully acquired the lock or a status of RESOURCE_EXHAUSTED if another client has the lock at that time.<\li>
	<li>The service implementation makes use of a global map for mapping client contexts to files. Every time a client wants to acquire a file lock, the mutex associated with the file is locked and the map is updated with the filename as key and the clientID as value. Once the client is done with the file(i.e. finished fetching or storing), the value for the respective filename is set to "Free" so that a new client requesting the lock can acquire it. <\li>
	<li>Coordination between the watcher thread and sync thread is achieved by the use of a mutex(curRunning_mutex). Whenever sync method or watcher callback is called, the corresponding thread locks the mutex and proceeds. The mutex is unlocked once the method finishes(Sync done or file event handled). So when one of the threads is active/running, the other thread tries to acquire the lock and blocks until it is released by the running thread.



