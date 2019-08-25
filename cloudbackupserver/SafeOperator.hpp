
class FileSafeOperator{
private:
	std::unordered_map<std::string,std::string>
public:

	bool GetFileList(std::vector<std::string> &list);
	
	bool GetlistRecord();
	
	bool SetlistRecord();
	
	bool GetFileData(std::string& name,std::string &body);
	
	bool SetFileData(std::string& name,std::string &body,int64_t offset);
	
	
	
	bool CompressFile(std::string &sname,std::string &dname);
	
	bool UnCompressFile(std::string &sname,std::string &dname);
	
	bool GetNormalData(std::string &name);
	
	bool InsertBackInfo(std::string &name,std::string &gzip);
	
	bool Start(){
		GetlistRecord();
		while(1){
			
		}
	}
}