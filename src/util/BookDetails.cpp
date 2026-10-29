#include "BookDetails.h"

#include <ArduinoJson.h>
#include <Epub.h>
#include <FsHelpers.h>
#include <HalStorage.h>
#include <Logging.h>

#include <cctype>
#include <cstdlib>

#include "BookDetailsText.h"
#include "activities/reader/ProgressFile.h"
#include "network/AuthorBio.h"

namespace {
constexpr char kFileName[] = "details.json";
constexpr size_t kMaxBlurbBytes = 1500;
constexpr size_t kMaxBioBytes = 1000;
constexpr size_t kMaxItemBytes = 64 * 1024;

std::string readItem(const Epub& epub, const std::string& href) {
  size_t size = 0;
  if (href.empty() || !epub.getItemSize(href, &size) || size == 0 || size > kMaxItemBytes) return {};
  uint8_t* bytes = epub.readItemContentsToBytes(href, &size, true);
  if (!bytes) return {};
  std::string text(reinterpret_cast<const char*>(bytes), size);
  free(bytes);  // ZipFile::readFileToMemory mallocs
  return text;
}

std::string authorBio(const Epub& epub) {
  for (int i = 0; i < epub.getTocItemsCount(); ++i) {
    const auto entry = epub.getTocItem(i);
    std::string title = entry.title;
    for (auto& c : title) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (title.find("about the author") == std::string::npos || entry.spineIndex < 0) continue;
    return BookDetailsText::bioFromSection(readItem(epub, epub.getSpineItem(entry.spineIndex).href), entry.anchor,
                                           entry.title, kMaxBioBytes);
  }
  return {};
}

void save(const std::string& bookPath, const BookDetails& details) {
  JsonDocument root;
  root["publisher"] = details.publisher;
  root["date"] = details.date;
  root["description"] = details.description;
  root["bio"] = details.bio;
  root["printPages"] = details.printPages;
  if (details.bioFromSync) root["bioFromSync"] = true;
  std::string json;
  serializeJson(root, json);
  if (!ProgressFile::writeAtomic(Epub(bookPath, "/.crosspoint").getCachePath(),
                                 reinterpret_cast<const uint8_t*>(json.data()), json.size(), kFileName))
    LOG_ERR("BDT", "Could not cache details for %s", bookPath.c_str());
}
}  // namespace

namespace BookDetailsStore {

std::optional<BookDetails> loadCached(const std::string& bookPath) {
  if (!FsHelpers::hasEpubExtension(bookPath)) return std::nullopt;
  HalFile file;
  if (!ProgressFile::openForRead("BDT", Epub(bookPath, "/.crosspoint").getCachePath() + "/" + kFileName, file))
    return std::nullopt;
  JsonDocument root;
  if (deserializeJson(root, file)) return std::nullopt;
  BookDetails details;
  details.publisher = root["publisher"] | "";
  details.date = root["date"] | "";
  details.description = root["description"] | "";
  details.bio = root["bio"] | "";
  details.printPages = root["printPages"] | 0U;
  details.bioFromSync = root["bioFromSync"] | false;
  return details;
}

BookDetails load(const std::string& bookPath) {
  if (auto cached = loadCached(bookPath)) return *cached;
  BookDetails details;
  if (!FsHelpers::hasEpubExtension(bookPath)) return details;

  Epub epub(bookPath, "/.crosspoint");
  BookMetadataCache::BookMetadata opf;
  if (!epub.load(true, true) || !epub.readOpfDetails(opf)) {
    LOG_ERR("BDT", "No OPF details for %s", bookPath.c_str());
    return details;
  }
  details.publisher = BookDetailsText::plainParagraph(opf.publisher, 120);
  details.date = opf.date.substr(0, 10);
  details.description = BookDetailsText::plainParagraph(opf.description, kMaxBlurbBytes);
  details.bio = authorBio(epub);
  details.printPages = BookDetailsText::lastPrintPage(readItem(epub, opf.pageListHref));
  save(bookPath, details);
  return details;
}

void backfillBio(const std::string& bookPath, const std::string& author) {
  if (!FsHelpers::hasEpubExtension(bookPath)) return;
  BookDetails details = load(bookPath);
  if (!details.bio.empty()) return;
  details.bio = BookDetailsText::plainParagraph(AuthorBio::fetch(author), kMaxBioBytes);
  if (details.bio.empty()) return;
  details.bioFromSync = true;
  save(bookPath, details);
}

}  // namespace BookDetailsStore
