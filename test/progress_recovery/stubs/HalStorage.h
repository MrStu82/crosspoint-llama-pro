#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include <stdexcept>
struct HalFile {std::string path; size_t pos=0; size_t write(const uint8_t*,size_t); int read(void*,size_t); size_t size(); void flush(); bool close();};
struct MockStorage {
 std::map<std::string,std::vector<uint8_t>> files;
 int operations=0,cut=-1,fail=-1,commits=0;
 bool mkdir(const char*p){files[p]={};return true;}
 bool step(){++operations;if(operations==cut)throw std::runtime_error("power cut");return operations!=fail;}
 bool exists(const char*p){return files.count(p);}
 bool openFileForWrite(const char*,const std::string&p,HalFile&f){if(!step())return false;files[p]={};f.path=p;f.pos=0;return true;}
 bool openFileForRead(const char*,const std::string&p,HalFile&f){if(!step()||!files.count(p))return false;f.path=p;f.pos=0;return true;}
 bool remove(const char*p){if(!step())return false;return files.erase(p);}
 bool rename(const char*a,const char*b){if(!step()||!files.count(a)||files.count(b))return false;files[b]=files[a];files.erase(a);if(std::string(a).find(".tmp")!=std::string::npos)++commits;return true;}
};
inline MockStorage Storage;
inline size_t HalFile::write(const uint8_t*p,size_t n){if(!Storage.step())return 0;Storage.files[path]={p,p+n};return n;}
inline int HalFile::read(void*p,size_t n){if(!Storage.step())return -1;auto&v=Storage.files[path];n=std::min(n,v.size()-pos);memcpy(p,v.data()+pos,n);pos+=n;return n;}
inline size_t HalFile::size(){return Storage.files[path].size();}
inline void HalFile::flush(){Storage.step();}
inline bool HalFile::close(){return Storage.step();}
