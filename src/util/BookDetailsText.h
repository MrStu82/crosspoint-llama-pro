#pragma once

#include <cstdint>
#include <string>

// Pure text helpers for the Library book card (host-tested).
namespace BookDetailsText {
// One paragraph of plain text from an HTML fragment: tags/entities resolved,
// whitespace collapsed, cut on a UTF-8 boundary at maxBytes.
std::string plainParagraph(const std::string& html, size_t maxBytes);
// The author bio from an "About the Author" section: text from the anchor
// element (or <body>) on, without the heading line itself.
std::string bioFromSection(const std::string& html, const std::string& anchor, const std::string& tocTitle,
                           size_t maxBytes);
// Highest arabic page label in an EPUB 3 nav page-list or NCX pageList; 0 if none.
uint32_t lastPrintPage(const std::string& navOrNcx);
}  // namespace BookDetailsText
