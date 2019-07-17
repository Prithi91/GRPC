#define BUFSIZE 5000

#include <map>
#include <mutex>
#include <shared_mutex>
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

#include "dfslib-shared-p2.h"
#include "dfslib-servernode-p2.h"
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
using dfs_service::Request;
using dfs_service::Empty;


extern dfs_log_level_e DFS_LOG_LEVEL;

//
// STUDENT INSTRUCTION:
//
// As with Part 1, the DFSServiceImpl is the implementation service for the rpc methods
// and message types you defined in your `dfs-service.proto` file.
//
// You may start with your Part 1 implementations of each service method.
//
// Elements to consider for Part 2:
//
// - How will you implement the write lock at the server level?
// - How will you keep track of which client has a write lock for a file?
//      - Note that we've provided a preset client_id in DFSClientNode that generates
//        a client id for you. You can pass that to the server to identify the current client.
// - How will you release the write lock?
// - How will you handle a store request for a client that doesn't have a write lock?
// - When matching files to determine similary, you can use the `file_checksum` method we've provided.
//      - Both the client and server have a premade `crc_table` variable to speed things up.
//      - Use the `file_checksum` method to compare two files, similar to the following:
//
//          std::uint32_t server_crc = dfs_file_checksum(filepath, &this->crc_table);
//
//      - Hint: as the crc checksum is a simple integer, you can pass it around inside your message types.
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

    // CRC Table kept in memory for faster calculations
    CRC::Table<std::uint32_t, 32> crc_table;
    //Map of locks
    std::map<std::string,std::string>* lock_map;
    std::mutex map_mutex;

public:

    DFSServiceImpl(const std::string& mount_path): mount_path(mount_path), crc_table(CRC::CRC_32()) {

    }

    ~DFSServiceImpl() {}

    //
    // STUDENT INSTRUCTION:
    //
    // Add your additional code here, including
    // the implementations of your rpc protocol methods.
    //
    void addMapPair(string filename, string clientId){
	this->lock_map -> insert(pair<std::string,std::string>(filename,clientId));	
    }

    std::string getLockStat(string filename){
	std::map<std::string,std::string>::iterator itr;
	if(!this->lock_map->empty()){
		itr = this->lock_map->find(filename);
		if( itr != lock_map->end()) return itr->second;
		else return NULL;
	}
	return NULL;
    }

    void setMapPair(string filename, string clientId){
	std::map<std::string,std::string>::iterator itr;
	if(!this->lock_map->empty()){
		itr = this->lock_map->find(filename);
		if( itr != lock_map->end()) itr->second= clientId;
	}
    }
    
    Status AcquireLock(ServerContext* context, const ::dfs_service::Request* request, ::dfs_service::Empty* response) override {
	Status status_ok;
	this->map_mutex.lock();
	string lockclient = this->getLockStat(request->file_name());
	string clientid = request->client_id();
	if(lockclient.empty()){
		this->addMapPair(request->file_name(), clientid);
		this->map_mutex.unlock();
		return status_ok;
	}
	else if(strcmp(lockclient.c_str(),"Free")==0){
		this->setMapPair(request->file_name(), clientid);
		this->map_mutex.unlock();
		return status_ok;
	}
	else if(strcmp(lockclient.c_str(),clientid.c_str())==0){
		this->map_mutex.unlock();
		return status_ok;
	}
	else{
		this->map_mutex.unlock();
		Status failed(StatusCode::RESOURCE_EXHAUSTED,"Another client has the file lock");
		return failed;
	}
    }

    Status StoreFile(ServerContext* context, ServerReader< ::dfs_service::Request>* reader, ::dfs_service::Empty* response) override {
	//const Request req_topass;
	Request req;
	//req_topass =&req;
	Status status_ok;
	int i=0;
	fstream fout;
	string filepath;
	while(reader->Read(&req)){
		if(i==0){
			//string filename = req.file_name();
			//string clientid = req.client_id();
			//req_topass.set_file_name(filename);
			//req_topass.set_client_id(clientid);
			filepath = this->WrapPath(req.file_name());
			Status lock_status = this->AcquireLock(context, &req, response);
			if(lock_status.error_code()==0){
				fout.open(filepath,ios::out|ios::binary);
			}
			else{
				return lock_status;
			}
		}
		fout << req.file_chunk();
		i++;
	}
	fout.close();
	this->map_mutex.lock();
	this->setMapPair(req.file_name(), "Free");
	this->map_mutex.unlock();
	return status_ok;
    }

    Status FetchFile(ServerContext* context, const ::dfs_service::Request* request, ServerWriter< ::dfs_service::File>* writer) override{
	cout<<"Fetch called"<<request->file_name();
	//char buffer[BUFSIZE];	
	size_t fsize, bytes_read, tobeRead;
	File file;
	Empty empty;
	Status status_ok;
	string filename = request->file_name();
	string filepath = this->WrapPath(filename);
	cout << "File path" << filepath << endl;
	Status lock_status = this->AcquireLock(context, request, &empty);
	if(lock_status.error_code()!=0) return lock_status;
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

    Status GetAtt(ServerContext* context, const ::dfs_service::Request* request, ::dfs_service::File* response) override{
	cout<<"Stat called"<<request->file_name();
	//File file;
	string filename = request->file_name();
	struct tm *time;
	struct stat filestat;
	size_t fsize;
	Status status_ok;
	time_t modtime;
	uint32_t server_csum;
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
	server_csum = dfs_file_checksum(filepath, &this->crc_table);
	response->set_file_csum(server_csum);
	return status_ok;
    }
};

//
// STUDENT INSTRUCTION:
//
// The following three methods are part of the basic DFSServerNode
// structure. You may add additional methods or change these slightly
// to add additional startup/shutdown routines inside, but be aware that
// the basic structure should stay the same as the testing environment
// will be expected this structure.
//
/**
 * The main server node constructor
 *
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
/**
 * Start the DFSServerNode server
 */
void DFSServerNode::Start() {
    ServerBuilder builder;
    DFSServiceImpl service(this->mount_path);
    builder.AddListeningPort(this->server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    this->server = builder.BuildAndStart();


    dfs_log(LL_SYSINFO) << "DFSServerNode server listening on " << this->server_address;
    this->server->Wait();
}

//
// STUDENT INSTRUCTION:
//
// Add your additional definitions here
//

