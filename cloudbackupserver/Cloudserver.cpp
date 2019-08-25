#include "httplib.h"
#include "CompressStore.hpp"
#include "sstream"
#include <fstream>
#include <unistd.h>
#include <fcntl.h>
#include <boost/filesystem.hpp>
#define SERVER_BASE_DIR "cloudfile"
#define SERVER_ADDR "0.0.0.0"
#define SERVER_PORT 9000
#define SERVER_BACKUP_DIR SERVER_BASE_DIR"/list"
using namespace httplib;
namespace bf = boost::filesystem;

CompressStore cstor;

class CloudServer{
public:
    CloudServer(const char* cert,const char* key):srv(cert,key){
		bf::path path(SERVER_BASE_DIR);
		if(!bf::exists(path)){
			bf::create_directory(path);
		}
		bf::path list_path(SERVER_BACKUP_DIR);
		if(!bf::exists(list_path)){
			bf::create_directory(list_path);
		}    
    
	}
    bool Start(){
		srv.set_base_dir(SERVER_BASE_DIR);
		srv.Get("/(list(/){0,1}){0,1}",GetFileList);
		srv.Get("/list/(.*)",GetFileData);
		srv.Put("/list/(.*)",PutFileData);
		srv.listen(SERVER_ADDR,SERVER_PORT);
		return true;
    }
private:
    SSLServer srv;
private:
    static void GetFileList(const Request &req,Response &rsp){
		std::vector<std::string> list;
		cstor.GetFileList(list);
		std::string body;
		body="<html><body><ol><hr />";
		for(auto e:list){
			bf::path path(e);
			std::string file=path.filename().string(); 
			std::string uri ="/list/"+file;
			body+="<h4><li>";
			body+="<a href='";
			body+=uri;
			body+="'>";
			body+=file;
			body+="</a>";
			body+="</li></h4>";
			//"<h4><li><a href='/list/filename'>filename</a></li></h4>"
		}
		body+="</ hr></ol></body></html>";
		rsp.set_content(&body[0],"text/html");
		
		return;
    }
	
    static void GetFileData(const Request &req,Response &rsp){
		std::string realpath=SERVER_BASE_DIR+req.path;
		std::string body;
		cstor.GetFileData(realpath,body);
		rep.set_content(body,"text/plain");
	}
	
    static bool RangeParse(std::string &range,int64_t &start){
		//Range:<unit>=<Range-Start>-    //   到结尾
		//Range:<unit>=<range-start>-<range-end>
		//bytw=start-end
		size_t pos1=range.find("=");
		size_t pos2=range.find("-");
		if(pos1==std::string::npos||pos2==std::string::npos){
			std::cerr<<"range:["<<range<<"] format error"<<std::endl;
			return false;
		}
		std::stringstream rs;
		rs<<range.substr(pos1+1,pos2-pos1-1);
		rs>>start;
		return true;
    }  
	
    static void PutFileData(const Request &req,Response &rsp){
		std::cout<<"backup file"<<req.path<<std::endl;
		if(!req.has_header("Range")){//判断是否有某个头信息
			rsp.status=400;
			return;
		}
		std::string range = req.get_header_value("Range");
		int64_t range_start;
		if(RangeParse(range,range_start)==false){
			rsp.status=400;
			return;
		}
		std::cerr<<"backup file:["<<req.path<<"] range:["<<range<<"]"<<std::endl;
		std::string realpath=SERVER_BASE_DIR+req.path;
		cstor.SetFileData(readpath,req.body,range_start)
		return;
    }
};

void thr_start(){
	cstor.LowHeatFileStore();
}
int main(){
	std::thread thr(thr_start);
	thr.detach();
    CloudServer srv("./cert.pem","./key.pem");
    srv.Start();
    return 0;
}
