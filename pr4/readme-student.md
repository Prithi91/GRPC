
# Project README file

This is **YOUR** Readme file.

## Project Description

Please see [Readme.md](Readme.md) for the rubric we use for evaluating your submission.

We will manually review your file looking for:

- A summary description of your project design.  If you wish to use grapics, please simply use a URL to point to a JPG or PNG file that we can review

- Any additional observations that you have about what you've done. Examples:
	- __What created problems for you?__
	- __What tests would you have added to the test suite?__
	- __If you were going to do this project again, how would you improve it?__
	- __If you didn't think something was clear in the documentation, what would you write instead?__

## Known Bugs/Issues/Limitations

__Please tell us what you know doesn't work in this submission__

## References

__Please include references to any external materials that you used in your project__

## Project Design - GRPC Service
# The GRPC service uses the following message types and methods for File transfer services.
	-A "File" message type for storing a file's meta data like File size, modified time, contents of the file in chunks.
	-A "Request" message type for storing a request's meta data like requested filename.
	-A "StoreFile" method that takes as input a stream of "File" message type, each instance of "File" having the subsequent chunks of the file to be stored in the server.
	-A "FetchFile" method that takes as input a "request" message type and returns a stream of "File" message type, each instance of "File" having the subsequent chunks of the file requested from the server.
	-A "ListFiles" method that returns a stream of "File" message type, representing the various files that are available in the server.
	-A "GetAtt" method that takes as input a "request" message type and returns a "File" message type with the meta data of the requested file.
# The DFS service uses the following message types and methods for File transfer services.
	-A "File" message type for storing a file's meta data like File size, modified time, checksum, contents of the file in chunks.
	-A "Request" message type for storing a request's meta data like requested filename, clientId.
	-A "StoreFile" method that takes as input a stream of "Request" message type, each instance of "Request" having the subsequent chunks of the file to be stored in the server and the client context.
	-A "FetchFile" method that takes as input a "request" message type and returns a stream of "File" message type, each instance of "File" having the subsequent chunks of the file requested from the server.
	-A "ListFiles" method that returns a stream of "File" message type, representing the various files that are available in the server.
	-A "GetAtt" method that takes as input a "request" message type and returns a "File" message type with the meta data of the requested file.
	-A "AcquireLock" method that takes as input a "request" message type containing the client context and the filename for which the lock is to be acquired and returns a status of OK if the client has succesfully acquired the lock or a status of RESOURCE_EXHAUSTED if another client has the lock at that time.
	-The service implementation makes use of a global map for mapping client contexts to files. Every time a client wants to acquire a file lock, the mutex associated with the file is locked and the map is updated with the filename as key and the clientID as value. Once the client is done with the file(i.e. finished fetching or storing), the value for the respective filename is set to "Free" so that a new client requesting the lock can acquire it. 
	-Coordination between the watcher thread and sync thread is achieved by the use of a mutex(curRunning_mutex). Whenever sync method or watcher callback is called, the corresponding thread locks the mutex and proceeds. The mutex is unlocked once the method finishes(Sync done or file event handled). So when one of the threads is active/running, the other thread tries to acquire the lock and blocks until it is released by the running thread.

## Testing
	-Individual grpc methods like fetch, store,list and stat were tested by generating requests for the below cases:
	# Files existing in the server
	# File not present in the server
	# Files of various types(jpg, pdf, txt., etc)
	# Files of varying sizes
	# DEADLINE_EXCEEDED status was tested by explicitly making the server sleep for a time longer than the timeout value specified in the client context.
	-Sync and watchers were tested with the below cases 
	# Client mount path as Empty folder(All files from server should be fetched)
	# Server mount path as Empty folder(All files from client should be uploaded)
	# File creation Event on server(File should be fetched from server)
	# File creation Event on client(File should be uploaded to server)
	# File updation Event on server(Changes should be fetched from server)
	# File updation Event on client(Changes should be uploaded to server)
	# Creation/Updation Events when sync is running(Watcher should block until sync finishes)
	# Multiple clients requesting same files - simulated by running client from different terminals with different mount paths(Files should be downloaded to all clients properly)

## References

