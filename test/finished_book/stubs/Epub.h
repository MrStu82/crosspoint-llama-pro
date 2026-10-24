#pragma once
#include <functional>
#include <string>
// Path-keyed like the real Epub cache: moving the book changes its cache dir.
struct Epub { std::string p; Epub(const std::string& path,const char*):p(path){} std::string getCachePath()const{return "/.crosspoint/epub_"+std::to_string(std::hash<std::string>{}(p));} };
