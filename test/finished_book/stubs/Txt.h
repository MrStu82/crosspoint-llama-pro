#pragma once
#include <functional>
#include <string>
// Path-keyed like the real Txt cache: moving the book changes its cache dir.
struct Txt { std::string p; Txt(const std::string& path,const char*):p(path){} std::string getCachePath()const{return "/.crosspoint/txt_"+std::to_string(std::hash<std::string>{}(p));} };
