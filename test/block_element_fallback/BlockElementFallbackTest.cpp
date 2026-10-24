#include <gtest/gtest.h>

#include "BlockElementFallback.h"

// "hence its (tentative) █ categorization": U+2588 must take the solid-box path.
TEST(BlockElementFallback, CoversBlockElementsOnly) {
  EXPECT_TRUE(isBlockElement(0x2588));  // FULL BLOCK
  EXPECT_TRUE(isBlockElement(0x2580));
  EXPECT_TRUE(isBlockElement(0x259F));
  EXPECT_FALSE(isBlockElement(0x257F));  // box drawing
  EXPECT_FALSE(isBlockElement(0x25A0));  // geometric shapes
  EXPECT_FALSE(isBlockElement(0xFFFD));
  EXPECT_FALSE(isBlockElement('X'));
}
