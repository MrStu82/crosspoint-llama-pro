#include <cassert>
#include <cstdio>
#include "src/activities/reader/ProgressFile.h"
int main(){
 const std::string path="/c/progress.bin";const std::vector<uint8_t> old={1,2,3};const uint8_t next[]={4,5,6,7};
 for(int mode=0;mode<2;++mode)for(int op=1;op<=20;++op){
  Storage=MockStorage{};Storage.files[path]=old;
  if(mode==0)Storage.cut=op;else Storage.fail=op;
  try{ProgressFile::writeAtomic("/c",next,4);}catch(const std::runtime_error&){}
  Storage.cut=Storage.fail=-1;HalFile f;assert(ProgressFile::openForRead("T",path,f));
  assert(Storage.files[path]==old||Storage.files[path]==std::vector<uint8_t>(next,next+4));
 }
 Storage=MockStorage{};Storage.files[path+".tmp"]={9};Storage.files[path+".bak"]=old;
 HalFile f;assert(ProgressFile::openForRead("T",path,f));assert(Storage.files[path]==old);
 Storage=MockStorage{};Storage.files[path+".tmp"]={9};assert(!ProgressFile::openForRead("T",path,f));
 puts("PASS 40 failure/restart points + backup recovery + stale temp rejection");
}
