#pragma once
#include <string>
struct Epub { Epub(const std::string&,const char*){} std::string getCachePath()const{return "/book";} };
