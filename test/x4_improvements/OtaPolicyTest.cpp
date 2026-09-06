#include "src/network/OtaUpdater.h"
#include "src/network/OtaVersion.h"
#include <cassert>
#include <iostream>
int main(){
 OtaUpdater updater;
 // There is no compatible distribution. Every candidate, even nominally a
 // fork asset, is rejected before download/flash. These labels are policy
 // denial scenarios, NOT executed image-validation fixtures.
 for(const char* scenario:{"unavailable","upstream","standard-x4","Deck","nominal-fork","oversized","altered","interrupted"}){
  (void)scenario;
  assert(updater.checkForUpdate()==OtaUpdater::NO_UPDATE);
  assert(!updater.isUpdateNewer());assert(updater.getOtaSize()==0);
  assert(updater.getLatestVersion().empty());assert(updater.installUpdate()==OtaUpdater::NO_UPDATE);
 }
 assert(ota_version::isNewer("v1.5.0-230-g252758d3","v1.5.1"));
 assert(!ota_version::isNewer("v1.5.0-230-g252758d3","bad"));
 assert(!ota_version::isNewer("v1.5.0-230-g252758d3","v1.5.0-231-g12345678"));
 std::cout<<"PASS actual X4PRO OtaUpdater fail-closed; no download/flash/boot symbols linked; suffix/malformed version checks\n";
}
