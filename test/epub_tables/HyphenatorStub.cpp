#include <Epub/hyphenation/Hyphenator.h>

const LanguageHyphenator* Hyphenator::cachedHyphenator_ = nullptr;

std::vector<Hyphenator::BreakInfo> Hyphenator::breakOffsets(const std::string&, bool) { return {}; }
void Hyphenator::setPreferredLanguage(const std::string&) {}
