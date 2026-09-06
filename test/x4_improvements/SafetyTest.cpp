#include "lib/Xtc/Xtc/XtcParser.h"
#include "lib/Xtc/Xtc/XtcPageBounds.h"
#include "src/activities/reader/TxtPageIndex.h"
#include <cassert>
#include <iostream>
using namespace xtc;
template<class T>void put(std::vector<uint8_t>&v,size_t o,const T&x){memcpy(v.data()+o,&x,sizeof(x));}
std::vector<uint8_t> book(uint16_t w=480,uint16_t h=800,uint8_t depth=2){
 size_t bytes=depth==2?(size_t(w)*h+7)/8*2:(w+7)/8*size_t(h);
 std::vector<uint8_t> v(56+16+22+bytes,0);
 XtcHeader hdr{};hdr.magic=depth==2?XTCH_MAGIC:XTC_MAGIC;hdr.versionMajor=1;hdr.pageCount=1;hdr.pageTableOffset=56;hdr.dataOffset=72;put(v,0,hdr);
 PageTableEntry entry{72,uint32_t(22+bytes),w,h};put(v,56,entry);
 XtgPageHeader page{};page.magic=depth==2?XTH_MAGIC:XTG_MAGIC;page.width=w;page.height=h;page.dataSize=bytes;put(v,72,page);
 for(size_t i=94;i<v.size();++i)v[i]=uint8_t(i*37);return v;
}
bool load(std::vector<uint8_t> v,bool streaming=false){Storage.files["book"]=v;XtcParser p;if(p.open("book")!=XtcError::OK)return false;PageInfo info{};if(!p.getPageInfo(0,info))return false;std::vector<uint8_t> data(pageBitmapBytes(info.width,info.height,info.bitDepth));
 if(streaming){size_t got=0;auto rc=p.loadPageStreaming(0,[&](const uint8_t*b,size_t n,size_t o){assert(o==got);assert(o+n<=data.size());memcpy(data.data()+o,b,n);got+=n;},257);if(rc!=XtcError::OK)return false;assert(got==data.size());}
 else if(p.loadPage(0,data.data(),data.size())!=data.size())return false;
 assert(std::equal(data.begin(),data.end(),v.begin()+94));return true;}
int main(){
 auto valid=book();assert(load(valid));assert(load(valid,true));assert(load(book(800,480)));assert(load(book(480,800,1)));
 for(auto dims: {std::pair<int,int>{0,800},{480,0},{480,799},{480,801},{801,800},{65535,8}}){assert(!load(book(dims.first,dims.second)));}
 // Every possible truncation of a valid bitmap is rejected through actual parser.
 for(size_t n=0;n<valid.size();++n){auto v=valid;v.resize(n);assert(!load(v));}
 for(bool streaming:{false,true}){
  auto v=valid;uint16_t w=479;put(v,76,w);assert(!load(v,streaming));
  v=valid;uint32_t wrong=1;put(v,82,wrong);assert(!load(v,streaming));
  v=valid;v[81]=1;assert(!load(v,streaming));
  v=valid;uint64_t large=UINT64_MAX;put(v,56,large);assert(!load(v,streaming));
 }
 txt_index::Identity id;id.fileSize=100;id.layout=7;id.content[0]=19;std::vector<size_t> expected={0,21,77},out={9};std::vector<uint8_t> bytes;HalFile f{&bytes};assert(txt_index::save(f,id,expected));
 f.pos=0;assert(txt_index::load(f,id,out)&&out==expected);
 for(size_t n=0;n<bytes.size();++n){auto shortFile=bytes;shortFile.resize(n);HalFile t{&shortFile};out={9};assert(!txt_index::load(t,id,out)&&out==std::vector<size_t>{9});}
 for(uint32_t count:{0U,UINT32_MAX,65537U,4U}){auto v=bytes;put(v,77,count);HalFile t{&v};assert(!txt_index::load(t,id,out));}
 for(uint32_t offset:{0U,100U,UINT32_MAX}){auto v=bytes;put(v,85,offset);HalFile t{&v};assert(!txt_index::load(t,id,out));}
 auto changed=id;changed.content[0]++;f.pos=0;assert(!txt_index::load(f,changed,out));changed=id;changed.layout++;f.pos=0;assert(!txt_index::load(f,changed,out));
 auto v=bytes;v[4]=3;HalFile old{&v};assert(!txt_index::load(old,id,out));
 std::cout<<"PASS actual XTC parser aligned/landscape/XTG, "<<valid.size()<<" truncations, malformed bounds/header/stream; TXT checked cache corpus/identity/legacy\n";
}
