#pragma once
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include "httplib.h"
#include <unordered_map>
#include <boost\filesystem.hpp>
#include <boost\algorithm\string.hpp>
#define CLIENT_BACKUP_DIR "backup"
#define CLIENT_BACKUP_INFO_FILE "backup.list"
#define RANGE_MAX_SIZE (10<<20)
#define BACKUP_URI "/list/"
#define SERVER_IP "192.168.203.136"
#define SERVER_PORT 9000
namespace bf = boost::filesystem;

class ThrBackup{
private:
	std::string _file;
	int64_t _range_start;
	int64_t _range_len;
public:
	bool _res;
	ThrBackup(const std::string &file,int64_t start,int64_t len)
		:_res(true),
		_file(file),
		_range_start(start),
		_range_len(len)
	{}
	void Start()
	{
		std::ifstream path(_file, std::ios::binary);
		if (!path.is_open()){
			std::cerr << "range backup file" << _file << "failed" << std::endl;
			_res = false;
			return;
		}
		path.seekg(_range_start, std::ios::beg);
		std::string body;
		body.resize(_range_len);
		path.read(&body[0], _range_len);
		if (!path.good()){
			std::cerr << "read file" << _file << "range data failed" << std::endl;
			_res = false;
			return;
		}
		path.close();
		bf::path name(_file);
		std::string url = BACKUP_URI + name.filename().string();
		httplib::Client cli(SERVER_IP, SERVER_PORT);
		httplib::Headers hdr;
		hdr.insert(std::make_pair("Content-Length", std::to_string(_range_len)));
		std::stringstream tmp;
		tmp << "bytes=" << _range_start << "-" << (_range_start + _range_len - 1);
		hdr.insert(std::make_pair("Range", tmp.str().c_str()));
		auto rsp = cli.Put(url.c_str(), hdr, body, "text/plain");
		if (rsp&&rsp->status != 200){
			_res = false;
		}
		std::stringstream ss;
		ss << "Backup file" << _file << "range:[" << _range_start << "-" << _range_len << "]backup success\n";
		std::cout << ss.str();
		return;
	}
};

class CloudClient{
public:
	CloudClient()
	{
		bf::path path(CLIENT_BACKUP_DIR);
		if (!bf::exists(path)){
			bf::create_directory(path);
		}
	}
	bool Start()
	{
		GetBackupInfo();
		while (1){
			BackupDirListen(CLIENT_BACKUP_DIR);
			SetBackupInfo();
			Sleep(3*1000);
		}
		return true;
	}
private:
	//获取文件备份信息
	//filename1 etag\n
	bool GetBackupInfo()
	{
		bf::path path(CLIENT_BACKUP_INFO_FILE);
		if (!bf::exists(path)){
			std::cerr << "list file" << path.string() << "is not exist" << std::endl;
			return false;
		}
		int64_t fsize = bf::file_size(path);
		if (fsize == 0){
			std::cerr << "have no backup file" << std::endl;
			return false;
		}
		std::string body;
		body.resize(fsize);
		std::ifstream file(CLIENT_BACKUP_INFO_FILE,std::ios::binary);
		if (!file.is_open()){
			std::cerr << "list file open error" << std::endl;
			return false;
		}
		file.read(&body[0], fsize);
		if (!file.good()){
			std::cerr << "read list file body error" << std::endl;
			return false;
		}
		file.close();
		std::vector<std::string> list;
		boost::split(list, body, boost::is_any_of("\n"));//切割
		for (auto i : list){
			size_t pos = i.find(" ");
			if (pos == std::string::npos){
				continue;
			}
			std::string key = i.substr(0, pos);
			std::string val = i.substr(pos + 1);
			_backup_list[key] = val;
		}
		return true;
	}
	//保存备份信息，刷新存储
	bool SetBackupInfo()
	{
		std::string body;
		for (auto& e : _backup_list){
			body += e.first + " " + e.second + "\n";
		}
		std::ofstream file(CLIENT_BACKUP_INFO_FILE, std::ios::binary);
		if (!file.is_open()){
			std::cerr << "open list file error" << std::endl;
			return false;
		}
		file.write(&body[0], body.size());
		if (!file.good()){
			std::cerr << "set backup info error" << std::endl;
			return false;
		}
		file.close();
		return true;
	}
	//目录监控
	bool BackupDirListen(const std::string &path)
	{
		bf::directory_iterator item_begin(path);
		bf::directory_iterator item_end;
		for (; item_begin != item_end; ++item_begin){
			if (bf::is_directory(item_begin->status())){
				BackupDirListen(item_begin->path().string());
				continue;
			}
			if (FileisNeedBackup(item_begin->path().string()) == false){
				continue;
			}
			std::cerr << "file:[" << item_begin->path().string() << "] need backup" << std::endl;
			if (PutFileData(item_begin->path().string()) == false){
				continue;
			}
			AddBakeupInfo(item_begin->path().string());
		}
		return true;
	}
	
	bool AddBakeupInfo(const::std::string &file)
	{
		//etag="mtime-fsize"
		std::string etag;
		if (GetFileEtag(file, etag) == false){
			return false;
		}
		_backup_list[file] = etag;
		return true;
	}
	bool GetFileEtag(const std::string &file, std::string &etag)
	{
		bf::path path(file);
		if (!bf::exists(path)){
			std::cerr << "get file" << file << "etag error" << std::endl;
			return false;
		}
		int64_t fsize = bf::file_size(path);
		int64_t mtime = bf::last_write_time(path);
		std::stringstream tmp;
		tmp << std::hex << fsize << "-" << std::hex << mtime;
		etag = tmp.str();
		return true;
	}

	bool FileisNeedBackup(const std::string &file)
	{
		std::string etag;
		if (GetFileEtag(file, etag) == false){
			return false;
		}
		auto it = _backup_list.find(file);
		if (it != _backup_list.end() && it->second == etag){
			return false;
		}
		return true;
	}

	bool PutFileData(const std::string &file)
	{
		//按大小对文件进行分块传输，通过获取分块传输是否成功判断文件是否上传成功，选择多线程传输
		//获取文件大小
		int64_t fsize = bf::file_size(file);
		if (fsize <= 0){
			std::cerr << "file" << file << "unnecessary backup" << std::endl;
		}
		//计算需要分多少块，每块的大小及起始位置
		//循环创建线程，在线程中上传文件数据
		int count = (int)(fsize / RANGE_MAX_SIZE);
		std::vector<ThrBackup> thr_res;
		std::vector<std::thread> thr_list;
		std::cerr << "file:[" << file << "]fsize:[" << fsize << "]count:[" << count + 1 << "]" << std::endl;
		for (int i = 0; i <= count; i++){
			int64_t range_start = i*RANGE_MAX_SIZE;
			int64_t range_end = (i + 1)*RANGE_MAX_SIZE - 1;
			if (i == count){
				range_end = fsize - 1;;
			}
			int64_t range_len = range_end - range_start + 1;
			ThrBackup backup_info(file, range_start, range_len);
			thr_res.push_back(backup_info);
		}
		for (int i = 0; i < count; i++){
			thr_list.push_back(std::thread(thr_start, &thr_res[i]));
		}
		//等待所有线程退出，判断文件上传结果
		bool ret = true;
		for (int i = 0; i <= count; i++)
		{
			thr_list[i].join();
			if (thr_res[i]._res == true){
				continue;
			}
			ret = false;
		}
		//上传成功，添加文件备份记录
		if (ret == false){
			return false;
		}
		std::cerr << "file:[" << file << "] backup success" << std::endl;
		return true;
	}

	static void thr_start(ThrBackup *backup_info)
	{
		backup_info->Start();
		return;
	}

private:
	std::unordered_map<std::string, std::string> _backup_list;
};