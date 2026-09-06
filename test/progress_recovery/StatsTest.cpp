#include <cassert>
#include <cstdio>
#include <HalStorage.h>
#include "src/util/BookReadingStats.h"
int main(){using namespace BookReadingRate;
 QualifiedPageSample sample{30,123,456,ContentBasis::ExactPages,100,0,0};
 assert(BookReadingStats::recordQualifiedPage("book.epub",sample,42));assert(Storage.commits==2);
 auto result=BookReadingStats::read("book.epub");assert(result.totalSeconds==42);assert(result.qualifiedSamples==1);assert(result.pagesPerMinuteQ16==2*kQ16One);
 assert(BookReadingStats::updatePosition("book.epub",124,ContentBasis::ExactPages,80,0));
 result=BookReadingStats::read("book.epub");assert(result.totalSeconds==42);assert(result.qualifiedSamples==0);
 // Corrupted committed state is not silently overwritten with zeros.
 Storage.files["/book/reading_stats.bin"]={1,2};assert(!BookReadingStats::add("book.epub",10,0));assert(Storage.files["/book/reading_stats.bin"].size()==2);
 // Genuine v1 migration retains TIME READ and bounded legacy pace.
 LegacyBookV1 old;old.totalSeconds=600;old.forwardPages=10;
 auto p=reinterpret_cast<const uint8_t*>(&old);Storage.files["/book/reading_stats.bin"]={p,p+sizeof(old)};
 result=BookReadingStats::read("book.epub");assert(result.totalSeconds==600);assert(result.pagesPerMinuteQ16==kQ16One);
 // Restart at every storage operation: committed book stays old or complete new.
 for(int mode=0;mode<2;++mode)for(int op=1;op<=65;++op){
  Storage=MockStorage{};assert(BookReadingStats::recordQualifiedPage("book.epub",sample,42));
  Storage.operations=0;if(mode==0)Storage.cut=op;else Storage.fail=op;
  try{BookReadingStats::recordQualifiedPage("book.epub",sample,17);}catch(const std::runtime_error&){}
  Storage.cut=Storage.fail=-1;
  auto recovered=BookReadingStats::read("book.epub");
  assert((recovered.totalSeconds==42&&recovered.qualifiedSamples==1)||
         (recovered.totalSeconds==59&&recovered.qualifiedSamples==2));
 }
 puts("PASS actual stats module: 130 failure/restart points; two commits, elapsed/sample preservation, layout reset, corruption refusal, v1 migration");}
