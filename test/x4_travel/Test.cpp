#include <cassert>
#include <iostream>
#include "lib/hal/X4ProSleep.h"
std::vector<std::string> events;uint32_t nowMs=0,releaseAt=0;bool stuck=false;
int main(){
 x4pro_sleep::holdFrontlightOff();
 assert((events==std::vector<std::string>{"detach8","release8","mode8:1","write8:0","hold8","detach9","release9","mode9:1","write9:0","hold9"}));
 events.clear();x4pro_sleep::releaseFrontlightHold();
 assert((events==std::vector<std::string>{"mode8:1","write8:0","release8","mode9:1","write9:0","release9"}));
 events.clear();x4pro_sleep::holdPanelMasterRailOff();
 assert((events==std::vector<std::string>{"release1","mode1:1","write1:0","hold1"}));
 for(unsigned release: {0u,10u,1990u}){events.clear();nowMs=0;releaseAt=release;stuck=false;assert(!x4pro_sleep::armBoundedPowerWake());assert(nowMs<=2000);assert(events.back()=="wake8:1");}
 events.clear();nowMs=0;stuck=true;assert(x4pro_sleep::armBoundedPowerWake());assert(nowMs==2000);assert(events.back()=="wake8:0");
 events.clear();nowMs=0;stuck=false;releaseAt=0;assert(!x4pro_sleep::armBoundedPowerWake());assert(events.back()=="wake8:1");
 std::cout<<"PASS actual X4 helper: dual off/hold/release ordering, normal/late/stuck/release-wake rearm; bounded2s, no timer\n";
}
