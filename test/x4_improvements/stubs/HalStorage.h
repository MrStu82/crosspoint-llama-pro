#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>
struct HalFile {
 std::vector<uint8_t>* data=nullptr; size_t pos=0;
 bool isOpen()const{return data;}
 operator bool()const{return data;}
 bool seek64(uint64_t n){if(!data||n>data->size())return false;pos=n;return true;}
 bool seek(uint64_t n){return seek64(n);}
 size_t fileSize64()const{return data?data->size():0;}
 size_t size()const{return fileSize64();}
 int read(void* p,size_t n){if(!data)return -1;n=std::min(n,data->size()-pos);if(n)memcpy(p,data->data()+pos,n);pos+=n;return n;}
 size_t write(const uint8_t*p,size_t n){data->insert(data->end(),p,p+n);return n;}
 bool close(){data=nullptr;return true;}
};
struct MockStorage {std::map<std::string,std::vector<uint8_t>> files;
 bool openFileForRead(const char*,const std::string&p,HalFile&f){if(!files.count(p))return false;f.data=&files[p];f.pos=0;return true;}
};
inline MockStorage Storage;
