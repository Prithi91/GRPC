#define BUFSIZE 5000

#include <regex>
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
#include <sys/inotify.h>
#include <grpcpp/grpcpp.h>

#include "dfslib-shared-p1.h"
#include "dfslib-clientnode-p1.h"
#include "proto-src/dfs-service.grpc.pb.h"

using namespace std;

using grpc::Status;
using grpc::Channel;
using grpc::StatusCode;
using grpc::ClientWriter;
using grpc::ClientReader;
using grpc::ClientContext;
using dfs_service::File;
using dfs_service::request;
using dfs_service::Empty;

//
// STUDENT INSTRUCTION:
//
// You may want to add aliases to your namespaced service methods here.
// All of the methods will be under the `dfs_service` namespace.
//
// For example, if you have a method named MyMethod, add
// the following:
//
//      using dfs_service::MyMethod
//


DFSClientNodeP1::DFSClientNodeP1() : DFSClientNode() {}

DFSClientNodeP1::~DFSClientNodeP1() noexcept {}

StatusCode DFSClientNodeP1::Store(const std::string &filename) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to store a file here. This method should
    // connect to your gRPC service implementation method
    // that can accept and store a file.
    //
    // When working with files in gRPC you'll need to stream
    // the file contents, so consider the use of gRPC's ClientWriter.
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
	size_t fsize, bytes_read, tobeRead;
	string filepath = this->WrapPath(filename);
	cout << "File path" << filepath << endl;
	ifstream fin;
	fin.open(filepath,ios::in|ios::ate|ios::binary);
	file.set_file_name(filename);
	fsize = fin.tellg();
	cout <<"file size"<< fsize;
	fin.seekg(0, ios::beg);
	bytes_read=0;
	tobeRead = fsize;
	file.set_file_size(fsize);
	auto deadline = std::chrono::system_clock::now() + std::chrono::milliseconds(this->deadline_timeout);
	context.set_deadline(deadline);
	unique_ptr<ClientWriter<File> > writer(this->service_stub->StoreFile(&context, &empty));
	do{
		if(tobeRead <= BUFSIZE){
			char buffer[tobeRead];
			memset(buffer,0, sizeof(buffer));
			fin.read(buffer, tobeRead);
			bytes_read+= tobeRead;
			//cout << "read content:" << buffer << endl;
			file.set_file_chunk(buffer,tobeRead);
		}
		else{
			char buffer[BUFSIZE];
			memset(buffer,0, sizeof(buffer));
			fin.read(buffer, BUFSIZE);
			bytes_read+= BUFSIZE;
			//cout << "read content:" << buffer << endl;
			file.set_file_chunk(buffer,BUFSIZE);
		}
		//cout << "Set file content:" << file.file_chunk() << endl;
		writer->Write(file);
		tobeRead = fsize - bytes_read;
	}while(tobeRead>0);
	fin.close();
	writer->WritesDone();
	Status status = writer->Finish();
	cout<< status.error_code()<<endl;
	return status.error_code();
}


StatusCode DFSClientNodeP1::Fetch(const std::string &filename) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to fetch a file here. This method should
    // connect to your gRPC service implementation method
    // that can accept a file request and return the contents
    // of a file from the service.
    //
    // As with the store function, you'll need to stream the
    // contents, so consider the use of gRPC's ClientReader.
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
	request file_to_fetch;
	file_to_fetch.set_file_name(filename);
	File file;
	ofstream fout;
	//short flag=0;
	string filepath = this->WrapPath(filename);
	fout.open(filepath, ios::out);
	auto deadline = std::chrono::system_clock::now() + std::chrono::milliseconds(this->deadline_timeout);
	context.set_deadline(deadline);
	unique_ptr<ClientReader<File> > reader(this->service_stub->FetchFile(&context,file_to_fetch));
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

StatusCode DFSClientNodeP1::List(std::map<std::string,int>* file_map, bool display) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to list all files here. This method
    // should connect to your service's list method and return
    // a list of files using the message type you created.
    //
    // The file_map parameter is a simple map of files. You should fill
    // the file_map with the list of files you receive with keys as the
    // file name and values as the modified time (mtime) of the file
    // received from the server.
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
		file_map->insert(pair<std::string,int>(file.file_name(),file.file_mtime()));	
		if(display){
			cout << file.file_name() << "\t" << file.file_mtime() <<endl;
		}
	}
	Status status = reader->Finish();
	cout<< status.error_code()<<endl;
	return status.error_code();
}

StatusCode DFSClientNodeP1::Stat(const std::string &filename, void* file_status) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to get the status of a file here. This method should
    // retrieve the status of a file on the server. Note that you won't be
    // tested on this method, but you will most likely find that you need
    // a way to get the status of a file in order to synchronize later.
    //
    // The status might include data such as name, size, mtime, crc, etc.
    //
    // The file_status is left as a void* so that you can use it to pass
    // a message type that you defined. For instance, may want to use that message
    // type after calling Stat from another method.
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
	File file;
	request req;
	req.set_file_name(filename);
	auto deadline = std::chrono::system_clock::now() + std::chrono::milliseconds(this->deadline_timeout);
	context.set_deadline(deadline);
	Status status = this->service_stub->GetAtt(&context, req, &file);
	cout << file.file_name() << "\t" << file.file_size() << "\t" << file.file_mtime() <<endl;
	cout<< status.error_code()<<endl;
	return status.error_code();
}

//
// STUDENT INSTRUCTION:
//
// Add your additional code here, including
// implementations of your client methods
//

