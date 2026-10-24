#include <gtest/gtest.h>

#include "BlockElementFallback.h"
#include "GlyphSubstitute.h"

// "hence its (tentative) █ categorization": U+2588 must take the solid-box path.
TEST(BlockElementFallback, CoversBlockElementsOnly) {
  EXPECT_TRUE(isBlockElement(0x2588));  // FULL BLOCK
  EXPECT_TRUE(isBlockElement(0x2580));
  EXPECT_TRUE(isBlockElement(0x259F));
  EXPECT_FALSE(isBlockElement(0x257F));  // box drawing
  EXPECT_TRUE(isBlockElement(0x25A0));   // black square
  EXPECT_FALSE(isBlockElement(0x25A1));  // white square
  EXPECT_FALSE(isBlockElement(0xFFFD));
  EXPECT_FALSE(isBlockElement('X'));
}

// The Antimemetics EPUB codepoints the built-in reader fonts lack.
TEST(GlyphSubstitute, MapsMissingCodepoints) {
  EXPECT_EQ(glyphSubstitute(0x0101), 'a');
  EXPECT_EQ(glyphSubstitute(0x011B), 'e');
  EXPECT_EQ(glyphSubstitute(0x02BB), 0x2018u);
  EXPECT_EQ(glyphSubstitute(0x2003), ' ');
  EXPECT_EQ(glyphSubstitute(0x0393), 'G');  // Gamma
  EXPECT_EQ(glyphSubstitute(0x03B5), 'e');  // epsilon
  EXPECT_EQ(glyphSubstitute(0x03C9), 'w');  // omega
  EXPECT_EQ(glyphSubstitute(0x03A3), 'S');  // Sigma
  EXPECT_EQ(glyphSubstitute(0x03C2), 's');  // final sigma
  EXPECT_EQ(glyphSubstitute(0x25A0), 0u);   // drawn by the block fallback
  EXPECT_EQ(glyphSubstitute('x'), 0u);
  EXPECT_EQ(glyphSubstitute(0xFFFD), 0u);
}
