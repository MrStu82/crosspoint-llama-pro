#pragma once
#include <functional>
#include <string>
// Path-keyed like the real Xtc cache: moving the book changes its cache dir.
struct Xtc { std::string p; Xtc(const std::string& path,const char*):p(path){} std::string getCachePath()const{return "/.crosspoint/xtc_"+std::to_string(std::hash<std::string>{}(p));} };
