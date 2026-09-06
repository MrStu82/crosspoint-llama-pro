#include "lib/Serialization/PersistableStore.h"
#include "src/HardcoverCredentialStore.h"
#include <HalStorage.h>
#include <cassert>
#include <iostream>
namespace obfuscation {
std::string obfuscateToBase64(const std::string& s){return s;}
std::string deobfuscateFromBase64(const char* s,size_t max,bool* ok,bool* too){std::string v=s;*too=v.size()>max;*ok=!*too;return *ok?v:std::string();}
}
int main(){
 int checks=0;
 for(const char* path:{"/.crosspoint/settings.json","/.crosspoint/state.json","/.crosspoint/wifi.json","/.crosspoint/opds.json","/.crosspoint/recent.json","/.crosspoint/solitaire.json","/.crosspoint/hardcover.json"}){
  Storage={};JsonDocument oldDoc,newDoc,readDoc;oldDoc["version"]=1;oldDoc["legacy"]=true;newDoc["version"]=2;
  assert(!PersistableStoreBase::readDocFromFile(path,readDoc));
  assert(PersistableStoreBase::writeDocToFile(path,oldDoc));const auto original=Storage.files;
  Storage.operations=0;assert(PersistableStoreBase::writeDocToFile(path,newDoc));int ops=Storage.operations;
  for(int cut=1;cut<=ops;++cut)for(bool crash:{false,true}){
   Storage={};Storage.files=original;Storage.cut=crash?cut:-1;Storage.fail=crash?-1:cut;
   try{PersistableStoreBase::writeDocToFile(path,newDoc);}catch(const std::runtime_error&){}
   Storage.cut=Storage.fail=-1;readDoc.clear();assert(PersistableStoreBase::readDocFromFile(path,readDoc));
   assert(readDoc["version"]==1||readDoc["version"]==2);++checks;
  }
  // Existing legacy payload accepted, no schema/default changes at this seam.
  Storage={};Storage.files=original;assert(PersistableStoreBase::readDocFromFile(path,readDoc));assert(readDoc["legacy"]==true);
 }
 auto& store=HardcoverCredentialStore::getInstance();
 const std::string synthetic="hc_pat_synthetic_fixture_only";
 Storage={};assert(store.replaceTokenAtomic(synthetic));const auto original=Storage.files;
 Storage.operations=0;assert(store.forget());int ops=Storage.operations;
 for(int cut=1;cut<=ops;++cut)for(bool crash:{false,true}){
  Storage={};Storage.files=original;assert(store.loadFromFile());Storage.operations=0;Storage.cut=crash?cut:-1;Storage.fail=crash?-1:cut;
  bool success=false;try{success=store.forget();}catch(const std::runtime_error&){}
  Storage.cut=Storage.fail=-1;assert(store.loadFromFile());if(success)assert(!store.hasToken());++checks;
 }
 // Successful forgetting survives repeated restart/recovery; stale temp never wins.
 Storage={};Storage.files=original;assert(store.loadFromFile());assert(store.forget());
 Storage.files["/.crosspoint/hardcover.json.tmp"]=original.at("/.crosspoint/hardcover.json");
 for(int n=0;n<10;++n){assert(store.loadFromFile());assert(!store.hasToken());}
 std::cout<<"PASS actual JSON persistence: "<<checks<<" operation failures/restarts over 7 paths + real Hardcover forget, legacy, first-use; synthetic obfuscation seam\n";
}
