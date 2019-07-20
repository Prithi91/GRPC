#define BUFSIZE 5000

#include <regex>
#include <mutex>
#include <vector>
#include <string>
#include <thread>
#include <cstdio>
#include <chrono>
#include <errno.h>
#include <csignal>
#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <getopt.h>
#include <unistd.h>
#include <limits.h>
#include <dirent.h>
#include <sys/inotify.h>
#include <grpcpp/grpcpp.h>
#include <utime.h>

#include "src/dfs-utils.h"
#include "src/dfs-clientnode-p2.h"
#include "dfslib-shared-p2.h"
#include "dfslib-clientnode-p2.h"
#include "proto-src/dfs-service.grpc.pb.h"

using namespace std;

using grpc::Status;
using grpc::Channel;
using grpc::StatusCode;
using grpc::ClientWriter;
using grpc::ClientReader;
using grpc::ClientContext;
using dfs_service::File;
using dfs_service::Request;
using dfs_service::Empty;


extern dfs_log_level_e DFS_LOG_LEVEL;

DFSClientNodeP2::DFSClientNodeP2() : DFSClientNode() {}
DFSClientNodeP2::~DFSClientNodeP2() {}

grpc::StatusCode DFSClientNodeP2::RequestWriteAccess(const std::string &filename) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to obtain a write lock here when trying to store a file.
    // This method should request a write lock for the given file at the server,
    // so that the current client becomes the sole creator/writer. If the server
    // responds with a RESOURCE_EXHAUSTED response, the client should cancel
    // the current file storage
    //
    // The StatusCode response should be:
    //
    // OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline timeout occurs
    // StatusCode::RESOURCE_EXHAUSTED - if a write lock cannot be obtained
    // StatusCode::CANCELLED otherwise
    //
    //
	ClientContext context;
	Empty empty;
	Request request;
	request.set_file_name(filename);
	request.set_client_id(this->ClientId());
	//cout << "File path" << filepath << endl;
	auto deadline = std::chrono::system_clock::now() + std::chrono::milliseconds(this->deadline_timeout);
	context.set_deadline(deadline);
	Status status = this->service_stub->AcquireLock(&context, request, &empty);
	return status.error_code();
}

grpc::StatusCode DFSClientNodeP2::Store(const std::string &filename) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to store a file here. Refer to the Part 1
    // student instruction for details on the basics.
    //
    // You can start with your Part 1 implementation. However, you will
    // need to adjust this method to recognize when a file trying to be
    // stored is the same on the server (i.e. the ALREADY_EXISTS gRPC response).
    //
    // You will also need to add a request for a write lock before attempting to store.
    //
    // If the write lock request fails, you should return a status of RESOURCE_EXHAUSTED
    // and cancel the current operation.
    //
    // The StatusCode response should be:
    //
    // StatusCode::OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline timeout occurs
    // StatusCode::ALREADY_EXISTS - if the local cached file has not changed from the server version
    // StatusCode::RESOURCE_EXHAUSTED - if a write lock cannot be obtained
    // StatusCode::CANCELLED otherwise
    //
    //
	Request req;
	File file;
	ClientContext context;
	req.set_file_name(filename);
	string filepath = this->WrapPath(filename);
	req.set_client_id(this->ClientId());
	Status statstatus = this->service_stub->GetAtt(&context, req, &file);
	uint32_t client_csum;
	client_csum = dfs_file_checksum(filepath, &this->crc_table);
	if(file.file_csum() == client_csum){
		Status failed(StatusCode::ALREADY_EXISTS,"File has not changed in the client");
		return failed.error_code();
	}
	StatusCode lockStatus = this->RequestWriteAccess(filename);
	if(lockStatus == StatusCode::OK){
		Empty empty;
		size_t fsize, bytes_read, tobeRead;
		cout << "File path" << filepath << endl;
		ifstream fin;
		fin.open(filepath,ios::in|ios::ate|ios::binary);
		fsize = fin.tellg();
		cout <<"file size"<< fsize;
		fin.seekg(0, ios::beg);
		bytes_read=0;
		tobeRead = fsize;
		req.set_file_size(fsize);
		ClientContext newcontext;
		auto deadline = std::chrono::system_clock::now() + std::chrono::milliseconds(this->deadline_timeout);
		newcontext.set_deadline(deadline);
		unique_ptr<ClientWriter<Request> > writer(this->service_stub->StoreFile(&newcontext, &empty));
		do{
			if(tobeRead <= BUFSIZE){
				char buffer[tobeRead];
				memset(buffer,0, sizeof(buffer));
				fin.read(buffer, tobeRead);
				bytes_read+= tobeRead;
				//cout << "read content:" << buffer << endl;
				req.set_file_chunk(buffer,tobeRead);
			}
			else{
				char buffer[BUFSIZE];
				memset(buffer,0, sizeof(buffer));
				fin.read(buffer, BUFSIZE);
				bytes_read+= BUFSIZE;
				//cout << "read content:" << buffer << endl;
				req.set_file_chunk(buffer,BUFSIZE);
			}
			//cout << "Set file content:" << file.file_chunk() << endl;
			writer->Write(req);
			tobeRead = fsize - bytes_read;
		}while(tobeRead>0);
		fin.close();
		writer->WritesDone();
		Status status = writer->Finish();
		cout<< status.error_code()<<endl;
		return status.error_code();
	}//end if status ok
	else{
		cout<< lockStatus<<endl;
		return lockStatus;
	}
}


grpc::StatusCode DFSClientNodeP2::Fetch(const std::string &filename) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to fetch a file here. Refer to the Part 1
    // student instruction for details on the basics.
    //
    // You can start with your Part 1 implementation. However, you will
    // need to adjust this method to recognize when a file trying to be
    // fetched is the same on the client (i.e. the files do not differ
    // between the client and server and a fetch would be unnecessary.
    //
    // The StatusCode response should be:
    //
    // OK - if all went well
    // DEADLINE_EXCEEDED - if the deadline timeout occurs
    // NOT_FOUND - if the file cannot be found on the server
    // ALREADY_EXISTS - if the local cached file has not changed from the server version
    // CANCELLED otherwise
    //
    // Hint: You may want to match the mtime on local files to the server's mtime
    //
	Request req;
	File file;
	ClientContext context;
	req.set_file_name(filename);
	string filepath = this->WrapPath(filename);
	cout<<"cur client id"<<this->ClientId();
	req.set_client_id(this->ClientId());
	Status statstatus = this->service_stub->GetAtt(&context, req, &file);
	uint32_t client_csum;
	client_csum = dfs_file_checksum(filepath, &this->crc_table);
	if(file.file_csum() == client_csum){
		Status failed(StatusCode::ALREADY_EXISTS,"File has not changed in the server");
		return failed.error_code();
	}
	ofstream fout;
	//short flag=0;
	fout.open(filepath, ios::out);
	ClientContext new_context;
	auto deadline = std::chrono::system_clock::now() + std::chrono::milliseconds(this->deadline_timeout);
	new_context.set_deadline(deadline);
	cout<<"Fetching"<< filename<<endl;
	unique_ptr<ClientReader<File> > reader(this->service_stub->FetchFile(&new_context,req));
	while(reader->Read(&file)){
		//flag=1;
		fout << file.file_chunk();
		//cout << "content:" << file.file_chunk();	
	}
	fout.close();
	Status status = reader->Finish();
	cout<< status.error_code()<<endl;
	return status.error_code();
}

grpc::StatusCode DFSClientNodeP2::List(std::map<std::string,int>* file_map, bool display) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to list files here. Refer to the Part 1
    // student instruction for details on the basics.
    //
    // You can start with your Part 1 implementation and add any additional
    // listing details that would be useful to your solution to the list response.
    //
    // The StatusCode response should be:
    //
    // StatusCode::OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline timeout occurs
    // StatusCode::CANCELLED otherwise
    //
    //
	ClientContext context;
	File file;
	Empty empty;
	auto deadline = std::chrono::system_clock::now() + std::chrono::milliseconds(this->deadline_timeout);
	context.set_deadline(deadline);
	unique_ptr<ClientReader<File> > reader(this->service_stub->ListFiles(&context,empty));
	while(reader->Read(&file)){
		file_map->insert(pair<std::string,int>(file.file_name(),file.file_csum()));	
		if(display){
			//cout << file.file_name() << "\t" << file.file_mtime() <<endl;
			//cout << file.file_name() << "\t" << file.file_csum() <<endl;
		}
	}
	Status status = reader->Finish();
	cout<< status.error_code()<<endl;
	return status.error_code();
}

void DFSClientNodeP2::ListClientFiles(std::map<std::string,int>* file_map,const std::string mount_path) {
	DIR *dir;
	struct dirent *ent;	
	const char *filedir = mount_path.c_str();
	//struct tm *time;
	//struct stat filestat;
	Status status_ok;
	//time_t modtime;
	//uint32_t server_csum;
	if((dir = opendir (filedir)) != NULL){
		while((ent = readdir(dir)) != NULL){
			File file;
			file.set_file_name(ent->d_name);
			string filepath = mount_path + ent->d_name;
			//stat(filepath.c_str(), &filestat);
			//time = gmtime(&(filestat.st_mtime));
			//modtime = mktime(time);
			//file.set_file_mtime(modtime);
			//cout<< file.file_name() << "\t" << file.file_mtime()<< endl;
			//server_csum = dfs_file_checksum(filepath, &this->crc_table);
			//file.set_file_csum(server_csum);
			//writer->Write(file);	
			file_map->insert(pair<std::string,int>(file.file_name(),1));
		}
		closedir(dir);
	}
	else{
		cout<<"Directory open failed"<<endl;
		//Status exists(StatusCode::CANCELLED,"Directory cannot be opened");
	}
}

grpc::StatusCode DFSClientNodeP2::Stat(const std::string &filename, void* file_status) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to get the status of a file here. Refer to the Part 1
    // student instruction for details on the basics.
    //
    // You can start with your Part 1 implementation and add any additional
    // status details that would be useful to your solution.
    //
    // The StatusCode response should be:
    //
    // StatusCode::OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline timeout occurs
    // StatusCode::NOT_FOUND - if the file cannot be found on the server
    // StatusCode::CANCELLED otherwise
    //
    //
	ClientContext context;
	//File file;
	File* file_ptr = (File*)file_status;
	Request req;
	req.set_file_name(filename);
	auto deadline = std::chrono::system_clock::now() + std::chrono::milliseconds(this->deadline_timeout);
	context.set_deadline(deadline);
	Status status = this->service_stub->GetAtt(&context, req, file_ptr);
	cout << file_ptr->file_name() << "\t" << file_ptr->file_size() << "\t" << file_ptr->file_mtime() << "\t" << file_ptr->file_csum() <<endl;
	//cout<< status.error_code()<<endl;
	//file_ptr = &file;
	//file_status = file_ptr;
	//file_status = static_cast<File *>(&file);
	//file_status = &file;
	//File *file_ptr = static_cast<File *>(file_status);
	//cout << "csum from stat" << file_status->file_csum();
	return status.error_code();
}

void DFSClientNodeP2::Sync() {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your synchronization code here. Note that synchronization
    // of the file system occurs every 3000 milliseconds on a timer by default.
    // This structure has already been handled for you in the client executable.
    //
    // When the Sync method is called, you should synchronize the
    // files between the server and the client based on the goals
    // described in the readme.
    //
    // In addition to synchronizing the files, you'll also need to ensure
    // that the sync thread and the file watcher thread are cooperating. These
    // two threads could easily get into a race condition where both are trying
    // to write or fetch over top of each other. So, you'll need to determine
    // what type of locking/guarding is necessary to ensure the threads are
    // properly coordinated.
    //
	cout << "Syncing..." << endl;
	std::map<std::string,int> file_map;
	std::map<std::string,int> client_file_map;
	this->List(&file_map, true);
   	this->curRunning_mutex.lock();
	cout<<"sync locked cur running";
	cout<< "server files:"<<endl;
	for(std::map<std::string,int>::iterator iter = file_map.begin(); iter!= file_map.end();iter++){
		std::string filename = iter->first;
		cout<<filename<<endl;
		string filepath = this->WrapPath(filename);
		if (!std::regex_match(filename, std::regex(".*\\.(jpg|png|gif|txt|xlsx|docx|md|psd)$"))) continue;
		File serverstat;
		this->Stat(filename,&serverstat);
		//File *file_ptr = static_cast<File *>(serverstat);
		uint32_t server_csum, client_csum;
		server_csum = serverstat.file_csum();
		client_csum = dfs_file_checksum(filepath, &this->crc_table);
		if(server_csum == client_csum){
			cout<<"server csum"<<server_csum<<endl;
			continue;
		}
		else{
			time_t server_mtime = serverstat.file_mtime();
			struct tm *time;
			struct stat filestat;
			time_t client_mtime;
			ifstream fin(filepath);
			stat(filepath.c_str(), &filestat);
			if(!fin){
				StatusCode status = this->Fetch(filename);
				cout << "Sync File " << filename <<" from server returned status" << status << endl;
				continue;
			}
			time = gmtime(&(filestat.st_mtime));
			client_mtime = mktime(time);
			if(server_mtime < client_mtime){
				StatusCode status = this->Store(filename);
				cout << "Sync File " << filename <<" to server returned status" << status;
			}
			else if(server_mtime > client_mtime){
				StatusCode status = this->Fetch(filename);
				cout << "Sync File " << filename <<" from server returned status" << status;
			}
		}
	}
	this->ListClientFiles(&client_file_map, this->mount_path.c_str());
	cout<<"client files"<<endl;
	for(std::map<std::string,int>::iterator iter = client_file_map.begin(); iter!= client_file_map.end();iter++){
		std::string filename = iter->first;
		cout<<filename<<endl;
		string filepath = this->WrapPath(filename);
		if (!std::regex_match(filename, std::regex(".*\\.(jpg|png|gif|txt|xlsx|docx|md|psd)$"))) continue;
		File serverstat;
		this->Stat(filename,&serverstat);
		//File *file_ptr = static_cast<File *>(serverstat);
		uint32_t server_csum, client_csum;
		server_csum = serverstat.file_csum();
		client_csum = dfs_file_checksum(filepath, &this->crc_table);
		if(server_csum == client_csum) continue;
		else{
			time_t server_mtime = serverstat.file_mtime();
			ifstream fin;
			fin.open(filepath);
			if(!fin){
				StatusCode status = this->Fetch(filename);
				cout << "Sync File " << filename <<" from server returned status" << status;
				continue;
			}
			struct tm *time;
			struct stat filestat;
			time_t client_mtime;
			stat(filepath.c_str(), &filestat);
			time = gmtime(&(filestat.st_mtime));
			client_mtime = mktime(time);
			if(server_mtime < client_mtime){
				StatusCode status = this->Store(filename);
				cout << "Sync File " << filename <<" to server returned status" << status;
			}
			else if(server_mtime > client_mtime){
				StatusCode status = this->Fetch(filename);
				cout << "Sync File " << filename <<" from server returned status" << status;
			}
		}
	}
	cout<<"sync unlocking cur running";
    	this->curRunning_mutex.unlock();
}

void DFSClientNodeP2::InotifyWatcherCallback(std::function<void()> callback) {

    //
    // STUDENT INSTRUCTION:
    //
    // This method gets called each time inotify signals a change
    // to a file on the file system. That is every time a file is
    // modified or created.
    //
    // You may want to consider how this section will affect
    // concurrent actions between the inotify watcher and the
    // server cache checks in the SyncTimerCallback to prevent a race condition.
    //
    // The callback method shown must be called here, but you may surround it with
    // whatever structures you feel are necessary to ensure proper coordination
    // between the sync and watcher threads.
    //
    // Hint: how can you prevent race conditions between this thread and
    // the synchronization timer when a file event has been signaled?
    //
    cout<<"Event recorded"<<endl;
    this->curRunning_mutex.lock();
    cout<<"Watcher thread acquired lock"<<endl;
    callback();
    this->curRunning_mutex.unlock();

}

//
// STUDENT INSTRUCTION:
//
// Add any additional code you need to here
//


