#include "src/activities/reader/TxtContentIdentity.h"
#include "src/activities/reader/TxtPageIndex.h"
#include "HalStorage.h"
#include <cassert>
#include <chrono>
#include <iostream>
int main(){
 std::vector<uint8_t> a={'a','b','c'},b={'a','b','d'};HalFile f{&a},g{&b};std::array<uint8_t,32>x{},y{};
 assert(txt_index::digest(f,x));assert(txt_index::digest(g,y));assert(x!=y);assert(x[0]==0xba&&x[1]==0x78&&x[31]==0xad);
 // Same path/size and identical header/TOC, changed font metric payload.
 a.assign(256,0);b=a;b[200]=1;f={&a};g={&b};assert(txt_index::digest(f,x)&&txt_index::digest(g,y)&&x!=y);
 txt_index::Identity id;id.fileSize=300;id.layout=17;id.font=x;std::vector<size_t> offsets={0,100,200},out;std::vector<uint8_t> bytes;f={&bytes};assert(txt_index::save(f,id,offsets));
 auto changed=id;changed.font=y;f.pos=0;assert(!txt_index::load(f,changed,out));
 for(size_t n:{size_t(1048576),size_t(10485760)}){
  a.assign(n,'a');f={&a};auto start=std::chrono::steady_clock::now();assert(txt_index::digest(f,x));
  const auto us=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
  std::cout<<"HOST_MEMORY_SHA256 bytes="<<n<<" us="<<us<<" reads="<<(n+511)/512<<"; excludes SD latency\n";
 }
 std::cout<<"PASS actual digest loop with real system SHA256; abc vector; same-size text and unchanged-header font revision cache rejection\n";
}
