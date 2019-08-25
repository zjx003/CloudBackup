#include <iostream>
#include <vector>
#include <unordered_map>
#include <string>
#include <thread>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <boost/filesystem.hpp>
#incluse <boost/algorithm/string.hpp>
#include <unistd.h>
#include <fcntl.h>
#include <zlib.h>
#include <pthread.h>

#define UNGZIPFILE_PATH "cloudfile/list/"
#define GZIPFILE_PATH "cloudfile/zip/"
#define RECORD_FILE "record_file"
#define HEAT_TIME 10

namespace bf=boost::filesystem;

class CompressStore{
private:
	std::string _file_dir;
	
	std::unordered_map<std::string,std::string> _file_list;
	
	pthread_rwlock_t _rwlock
private:	
	bool GetListRecord(){
		bf::path name(RECORD_FILE);
		if(!bf::exists(name)){
			std::cerr<<"record file is no exists"<<std::endl;
			return false;
		}
		std::ifstream file(RECORD_FILE,std::ios::binary);
		if(!file.is_open()){
			std::cerr<<"record file body open error"<<std::endl;
			return false;
		}
		int64_t fsize=bf::file_size();
		std::string body;
		body.resize(fsize);
		file.read(&body[0],fsize);
		if(!file.good()){
			std::cerr<<"record file body read error"<<std::endl;
			return false;
		}
		file.close();
		std::vector<std::string> list;
		boost::split(list,body,boost::is_any_of("\n");
		for(auto e:list){
			size_t pos=i.find(" ");
			if(pos==std::sting::npos){
				continue;
			}
			std::string key=e.substr(0,pos);
			std::string val=e.substr(pos+1);
			_file_list[key]=val;
		}
		return true;
	}
	
	bool SetListRecord(){
		std::stringstream tmp;
		for(auto e:_file_list){
			tmp<<e.first<<" "<<e.second<<"\n";
		}
		std::ostream file(RECORD_FILE,std::ios::binary|std::ios::trunc);
		if(!file.is_open()){
			std::cerr<<"record file open error"<<std::endl;
			return false;
		}
		file.write(tmp.str().c_str(),tmp.str().size());
		if(!file.good()){
			std::cerr<<"record file write error"<<std::endl;
			return false;
		}
		file.close();
		return true;
	}
	
	bool IsNeedCompress(std::string &file){
		struct stat sta;
		if(stat(file.c_str(),&sts)<0){
			std::cerr<<"get file:["<<file<<"] stat error"<<std::endl;
			return false;
		}
		time_t cur_time=time(nullptr);
		time_t acc_time=sta.st_atime;
		if((cur_time-acc_time)<HEAT_TIME){
			return false;
		}
		return true;
	}
		
	
	bool CompressFile(std::string &string,std::string &gzip){
		int fd=open(file.c_str(),0_RDONLY);
		if(fd<0){
			std::cerr"compress open file:["<<file<<"] error"<<std::endl;
			return false;
		}
		gzFile gf =gzopen(gzip.c_str(),"wb");
		if(gf==nullptr){
			std::cerr<<"compress open gzip:{"<<gzip<<"]error"<<std::endl;
			return false;
		}
		int ret;
		char buf[1024];
		flock(fd,LOCK_SH);
		while((ret=read(fd,buf,1024))>0){
			gzwrite(gf,buf,ret);
		}
		flock(fd,LOCK_UN);
		close(fd);
		gzclose(gf);
		unlink(file.c_str());
		return true;
	}
	
	
	bool UnCompressFile(std::string &gzip,std::string &file){
		int fd(open(file.c_str(),O_CREAT|O_REONLY,0664);
		if(fd<0){
			std::cerr<<"open file"<<file<<"failed"<<std::endl;
			return false;
		}
		gzFile gf=gzopen(gzip.c_str(),"rb");
		if(gf==nullptr){
			std::cerr<<"open gzip"<<gzip<<"failed"<<STD::ENDL;
			close(fd);
			return false;
		}
		int ret;
		char buf[1024]={0};
		flock(fd,LOCK_EX);
		while((ret=gzread(gf,buf,1024))>0)>0){
			int len=write(fd,buf,ret);
			if(len <0){
				std::cerr<<"get gzip data failed"<<std::endl;
				gzclose(gf);
				close(fd);
				flock(fd,LOCK_UN);
				return false;
			}
		}
		gzclose(gf);
		flock(fd,LOCK_UN);
		close(fd);
		unlink(gzip.c_str());
		return true;	
	}
	
	bool GetNomalfile(std::string &name,std::string &body){
		int64_t fsize=bf::file_size(name);
		body.resize(fsize);
		int fd=open(name.c_str(),O_REONLY);
		if(fd<0){
			std::cerr<<"open file "<<name<<" failed"<<std::endl;
			return false;
		}
		flock(fd,LOCK_SH);
		int ret=read(fd,&body[0],fsize);
		flock(fd,LOCK_UN);
		if(ret!=fsize){
			std::cerr<<"get file "<<name<<" body error"<<std::endl;
			close(fd);
			return false;
		}
		return true;
	}
	
	//目录检索，获取目录中文件名
	//判断文件是否需要压缩存储
	bool DirectoryCheck(){
		if(!bf::exists(UNGZIPFILE_PATH)){
			bf::create_directory(UNGZIPFILE_PATH);
		}
		bf::directory_iterator item_begin(UNGZIPFILE_PATH);
		bf::directory_iterator item_end;
		for(;item_begin!=item_end;++item_begin){
			if(bf::is_directory(item_begin->status())){
				continue;
			}
			std::string name=item_begin->path().string();
			if(IsNeedCompress(name)){
				std::string gzip=GZIPFILE_PATH+item_begin->path().filename().string()+".gz";
				CompressFile(name,gzip);
				AddFileRecord(name,gzip);
			}
		}
		return true;
	}
		
public:
	CompressStore(){
		pthread_rwlock_init(&rw_lock,nullptr);
		if(!bf::exists(GZIPFILE_PATH)){
			bf::create_directory(GZIPFILE_PATH);
		}
	}
	~CompressStore(){
		pthread_rwlock_destory(&rw_lock);
	}
	
	bool GetFileList(std::vector<std::string> &list){
		pthread_rwlock_rdlock(&_rwlock);
		for(auto e:_file_list){
			list.pash_back(e.first);
		}
		pthread_rwlock_unlock(&_rwlock);
		return true;
	}
	
	bool AddFileRecord(const std::string file,const std::string &gzip){
		pthread_rwlock_wrlock(&_rwlock);
		_file_list[file]=gzip;
		pthread_rwlock_unlock(&_rwlock);
	}
		
	bool SetFileData(const std::string& file,const std::string &body,const int64_t offset){
		int fd=open(file.c_str(),O_CREAT|O_WRONLY,0664);
		if(fd<0){
			std::cerr<<"open file"<<file<<"error"<<std::endl;
			return false;
		}
		flock(fd,LOCK_EX);
		lseek(fd,offset,SEEK_SET);
		int ret=write(fd,body.c_str(),&body[0].size());
		if(ret<0){
			std::cerr<<"store file"<<file<<" data error"<<std::endl;
			flock(fd,LOCK_UN);
			return false;
		}
		flock(fd,LOCK_UN);
		close(fd);
		AddFileRecord(file)
		return true;
	}
	
	bool GetFileGzip(std::string &file,std::string &gzip){
		pthread_rwlock_rdlock(&_rwlock);
		auto it=_file_list.find(file);
		if(it==_file_list.end()){
			pthread_rwlock_unlock(&_rwlock);
			return false;
		}
		gzip=it->second;
		pthread_rwlock_unlock(&_rwlock);
		return true;
	}
	
	bool GetFileData(std::string& file,std::string &body){
		if(bf::exists(file)){
			GetNomalfile(file,body);
		}else{
			std::string gzip;
			GetFileGzip(file,gzip);
			UnCompressFile(gzip,file);
			GetNomalfile(file,body);
		}
	}
	
	bool LowHeatFileStore(){
		GetListRecord();
		while(1){
			DirectoryCheck();
			SetListRecord();
			sleep(3);
		}	
	}
};