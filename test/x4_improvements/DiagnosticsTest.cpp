#define X4_READER_DIAGNOSTICS 1
#include "src/util/ReaderDiagnostics.h"
#include <cassert>
#include <iostream>
int main(){
 for(int n=0;n<256;++n)for(int s=0;s<static_cast<int>(reader_diagnostics::Stage::Count);++s){reader_diagnostics::Scope scope(static_cast<reader_diagnostics::Stage>(s));}
 for(const auto&m:reader_diagnostics::metrics)assert(m.count==200);
 std::cout<<"PASS diagnostic collector: 200 bounded samples/stage; HOST self-test, not page-turn timings or hardware resource data\n";
}
