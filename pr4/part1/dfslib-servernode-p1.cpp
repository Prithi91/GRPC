#define BUFSIZE 5000

#include <map>
#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#include <errno.h>
#include <iostream>
#include <fstream>
#include <getopt.h>
#include <dirent.h>
#include <sys/stat.h>
#include <grpcpp/grpcpp.h>
#include <time.h>

#include "src/dfs-utils.h"
#include "dfslib-shared-p1.h"
#include "dfslib-servernode-p1.h"
#include "proto-src/dfs-service.grpc.pb.h"

using namespace std;
using grpc::Status;
using grpc::Server;
using grpc::StatusCode;
using grpc::ServerReader;
using grpc::ServerWriter;
using grpc::ServerContext;
using grpc::ServerBuilder;

using dfs_service::DFSService;
using dfs_service::File;


//
// STUDENT INSTRUCTION:
//
// DFSServiceImpl is the implementation service for the rpc methods
// and message types you defined in the `dfs-service.proto` file.
//
// You should add your definition overrides here for the specific
// methods that you created for your GRPC service protocol. The
// gRPC tutorial described in the readme is a good place to get started
// when trying to understand how to implement this class.
//
// The method signatures generated can be found in `proto-src/dfs-service.grpc.pb.h` file.
//
// Look for the following section:
//
//      class Service : public ::grpc::Service {
//
// The methods returning grpc::Status are the methods you'll want to override.
//
// In C++, you'll want to use the `override` directive as well. For example,
// if you have a service method named MyMethod that takes a MyMessageType
// and a ServerWriter, you'll want to override it similar to the following:
//
//      Status MyMethod(ServerContext* context,
//                      const MyMessageType* request,
//                      ServerWriter<MySegmentType> *writer) override {
//
//          /** code implementation here **/
//      }
//
class DFSServiceImpl final : public DFSService::Service {

private:

    /** The mount path for the server **/
    std::string mount_path;

    /**
     * Prepend the mount path to the filename.
     *
     * @param filepath
     * @return
     */
    const std::string WrapPath(const std::string &filepath) {
        return this->mount_path + filepath;
    }


public:

    //static std::map<std::string,int> filestat_map;
    DFSServiceImpl(const std::string &mount_path): mount_path(mount_path) {
    }

    ~DFSServiceImpl() {}

    //
    // STUDENT INSTRUCTION:
    //
    // Add your additional code here, including
    // implementations of your protocol service methods
    //

    
    Status FetchFile(ServerContext* context, const ::dfs_service::request* request, ServerWriter< ::dfs_service::File>* writer) override{
	cout<<"Fetch called"<<request->file_name();
	//char buffer[BUFSIZE];	
	size_t fsize, bytes_read, tobeRead;
	File file;
	Status status_ok;
	string filename = request->file_name();
	string filepath = this->WrapPath(filename);
	cout << "File path" << filepath << endl;
	ifstream fin;
	//std::this_thread::sleep_for(std::chrono::milliseconds(10000));
	fin.open(filepath,ios::in|ios::ate|ios::binary);
	if(!fin){
		cout<<"File not found"<<endl;
		Status notfound(StatusCode::NOT_FOUND,"File not found");
		return notfound;
	}
	file.set_file_name(filename);
	fsize = fin.tellg();
	cout <<"file size"<< fsize;
	fin.seekg(0, ios::beg);
	bytes_read=0;
	tobeRead = fsize;
	file.set_file_size(fsize);
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
	return status_ok;
    }
	
    Status StoreFile(ServerContext* context, ServerReader< ::dfs_service::File>* reader, ::dfs_service::Empty* response) override{
	File file;
	Status status_ok;
	int i=0;
	fstream fout;
	string filepath;
	while(reader->Read(&file)){
		if(i==0){
			filepath = this->WrapPath(file.file_name());
			struct stat buffer;
			if(stat(filepath.c_str(), &buffer)==0){
				cout<<"File already exists"<<endl;
				Status exists(StatusCode::ALREADY_EXISTS,"File exists already");
				return exists;
			}
			else{
				fout.open(filepath,ios::out|ios::binary);
			}
		}
		fout << file.file_chunk();
		i++;
	}
	fout.close();
	return status_ok;
    }
   
    Status ListFiles(ServerContext* context, const ::dfs_service::Empty* request, ServerWriter< ::dfs_service::File>* writer) override{
	DIR *dir;
	struct dirent *ent;	
	const char *filedir = this->mount_path.c_str();
	struct tm *time;
	struct stat filestat;
	Status status_ok;
	time_t modtime;
	if((dir = opendir (filedir)) != NULL){
		while((ent = readdir(dir)) != NULL){
			File file;
			file.set_file_name(ent->d_name);
			string filepath = this->WrapPath(ent->d_name);
			stat(filepath.c_str(), &filestat);
			time = gmtime(&(filestat.st_mtime));
			modtime = mktime(time);
			file.set_file_mtime(modtime);
			cout<< file.file_name() << "\t" << file.file_mtime()<< endl;
			writer->Write(file);	
		}
		closedir(dir);
	}
	else{
		cout<<"Directory open failed"<<endl;
		Status exists(StatusCode::CANCELLED,"Directory cannot be opened");
		return exists;
	}
	return status_ok;
    }

    Status GetAtt(ServerContext* context, const ::dfs_service::request* request, ::dfs_service::File* response) override{
	cout<<"Stat called"<<request->file_name();
	//File file;
	string filename = request->file_name();
	struct tm *time;
	struct stat filestat;
	size_t fsize;
	Status status_ok;
	time_t modtime;
	string filepath = this->WrapPath(request->file_name());
	ifstream fin;
	fin.open(filepath,ios::in|ios::ate|ios::binary);
	if(!fin){
		cout<<"File not found"<<endl;
		Status notfound(StatusCode::NOT_FOUND,"File not found");
		return notfound;
	}
	response->set_file_name(filename);
	fsize = fin.tellg();
	response->set_file_size(fsize);
	stat(filepath.c_str(), &filestat);
	time = gmtime(&(filestat.st_mtime));
	modtime = mktime(time);
	response->set_file_mtime(modtime);
	return status_ok;
    }

};

//
// STUDENT INSTRUCTION:
//
// The following three methods are part of the basic DFSServerNode
// structure. You may add additional methods or change these slightly,
// but be aware that the testing environment is expecting these three
// methods as-is.
//
/**
 * The main server node constructor
 *
 * @param server_address
 * @param mount_path
 */
DFSServerNode::DFSServerNode(const std::string &server_address,
        const std::string &mount_path,
        std::function<void()> callback) :
    server_address(server_address), mount_path(mount_path), grader_callback(callback) {}

/**
 * Server shutdown
 */
DFSServerNode::~DFSServerNode() noexcept {
    dfs_log(LL_SYSINFO) << "DFSServerNode shutting down";
    this->server->Shutdown();
}

/** Server start **/
void DFSServerNode::Start() {
    DFSServiceImpl service(this->mount_path);
    ServerBuilder builder;
    builder.AddListeningPort(this->server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    this->server = builder.BuildAndStart();
    dfs_log(LL_SYSINFO) << "DFSServerNode server listening on " << this->server_address;
    this->server->Wait();
}

//
// STUDENT INSTRUCTION:
//
// Add your additional DFSServerNode definitions here
//

