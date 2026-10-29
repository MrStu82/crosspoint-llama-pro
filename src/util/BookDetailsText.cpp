#include "BookDetailsText.h"

#include <cctype>

#include "HtmlToPlainText.h"

namespace {
std::string lower(std::string text) {
  for (auto& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return text;
}

std::string collapse(const std::string& text, const size_t maxBytes) {
  std::string out;
  for (const char c : text) {
    if (std::isspace(static_cast<unsigned char>(c))) {
      if (!out.empty() && out.back() != ' ') out.push_back(' ');
    } else {
      out.push_back(c);
    }
  }
  if (!out.empty() && out.back() == ' ') out.pop_back();
  if (out.size() > maxBytes) {
    size_t cut = maxBytes;
    while (cut > 0 && (static_cast<unsigned char>(out[cut]) & 0xC0) == 0x80) --cut;
    out.resize(cut);
  }
  return out;
}
}  // namespace

namespace BookDetailsText {

std::string plainParagraph(const std::string& html, const size_t maxBytes) {
  return collapse(htmlToPlainText(html), maxBytes);
}

std::string bioFromSection(const std::string& html, const std::string& anchor, const std::string& tocTitle,
                           const size_t maxBytes) {
  size_t start = std::string::npos;
  if (!anchor.empty()) {
    start = html.find("id=\"" + anchor + "\"");
    if (start != std::string::npos) start = html.rfind('<', start);
  }
  if (start == std::string::npos) start = lower(html).find("<body");
  if (start == std::string::npos) start = 0;

  // Drop the leading heading lines ("About the Author" or the TOC label).
  const std::string text = htmlToPlainText(html.substr(start));
  const std::string title = lower(collapse(tocTitle, tocTitle.size()));
  size_t pos = 0;
  while (pos < text.size()) {
    size_t end = text.find('\n', pos);
    if (end == std::string::npos) end = text.size();
    const std::string line = lower(collapse(text.substr(pos, end - pos), end - pos));
    if (!line.empty() && line != "about the author" && line != title) break;
    pos = end + 1;
  }
  return pos < text.size() ? collapse(text.substr(pos), maxBytes) : std::string();
}

uint32_t lastPrintPage(const std::string& navOrNcx) {
  const std::string doc = lower(navOrNcx);
  size_t begin = doc.find("<pagelist");
  size_t end = std::string::npos;
  if (begin != std::string::npos) {
    end = doc.find("</pagelist>", begin);
  } else if ((begin = doc.find("\"page-list\"")) != std::string::npos) {
    end = doc.find("</nav>", begin);
  }
  if (begin == std::string::npos) return 0;
  if (end == std::string::npos) end = doc.size();

  // Every text node in the list is a page label; keep the largest number.
  uint32_t last = 0;
  for (size_t gt = doc.find('>', begin); gt != std::string::npos && gt < end; gt = doc.find('>', gt + 1)) {
    size_t i = gt + 1;
    while (i < end && std::isspace(static_cast<unsigned char>(doc[i]))) ++i;
    uint32_t value = 0;
    size_t digits = 0;
    while (i < end && std::isdigit(static_cast<unsigned char>(doc[i])) && digits < 6) {
      value = value * 10 + static_cast<uint32_t>(doc[i] - '0');
      ++i;
      ++digits;
    }
    while (i < end && std::isspace(static_cast<unsigned char>(doc[i]))) ++i;
    if (digits > 0 && i < end && doc[i] == '<' && value > last) last = value;
  }
  return last;
}

}  // namespace BookDetailsText
