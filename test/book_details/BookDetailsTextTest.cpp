// Host tests for the Library book card's text helpers.

#include <gtest/gtest.h>

#include <string>

#include "BookDetailsText.h"

namespace {

TEST(BookDetailsText, BlurbIsOneCappedParagraph) {
  EXPECT_EQ(BookDetailsText::plainParagraph("<p>A man.</p>\n<p>His  cat &amp; dog.</p>", 100),
            "A man. His cat & dog.");
  // "é" is two bytes; a cut inside it backs off to the boundary.
  EXPECT_EQ(BookDetailsText::plainParagraph("caf\xC3\xA9", 4), "caf");
}

TEST(BookDetailsText, BioSkipsHeadAndHeading) {
  const std::string html =
      "<html><head><title>About the Author</title></head><body>"
      "<h1><img alt=\"About the Author\"/></h1><h2>About the Author</h2>"
      "<p>QNTM is a writer.</p><p>They live in London.</p></body></html>";
  EXPECT_EQ(BookDetailsText::bioFromSection(html, "", "About the Author", 200),
            "QNTM is a writer. They live in London.");
}

TEST(BookDetailsText, BioStartsAtAnchor) {
  const std::string html =
      "<body><p>Acknowledgements text.</p><h2 id=\"bio\">About the Author</h2><p>Bio here.</p></body>";
  EXPECT_EQ(BookDetailsText::bioFromSection(html, "bio", "About the Author", 200), "Bio here.");
  EXPECT_EQ(BookDetailsText::bioFromSection("", "", "About the Author", 200), "");
}

TEST(BookDetailsText, LastPrintPageFromNavAndNcx) {
  const std::string nav =
      "<nav epub:type=\"toc\"><ol><li><a href=\"c1.xhtml\">1</a></li><li><a>900</a></li></ol></nav>"
      "<nav epub:type=\"page-list\" hidden=\"\"><ol><li><a href=\"a#i\">i</a></li>"
      "<li><a href=\"a#p1\">1</a></li><li><a href=\"b#p279\"> 279 </a></li></ol></nav>";
  EXPECT_EQ(BookDetailsText::lastPrintPage(nav), 279u);
  const std::string ncx =
      "<navMap><navPoint><navLabel><text>12</text></navLabel></navPoint></navMap>"
      "<pageList><pageTarget value=\"1\"><navLabel><text>1</text></navLabel></pageTarget>"
      "<pageTarget><navLabel><text>465</text></navLabel></pageTarget></pageList>";
  EXPECT_EQ(BookDetailsText::lastPrintPage(ncx), 465u);
  EXPECT_EQ(BookDetailsText::lastPrintPage("<nav epub:type=\"toc\"><a>12</a></nav>"), 0u);
}

}  // namespace
