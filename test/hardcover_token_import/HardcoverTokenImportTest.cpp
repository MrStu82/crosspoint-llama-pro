#include <gtest/gtest.h>

#include "util/HardcoverTokenImport.h"

using HardcoverTokenImport::sanitise;

// The file is written by a human with a text editor, so every plausible way one
// of those mangles a single line has to survive intact.
TEST(HardcoverTokenImport, KeepsAPlainToken) {
  EXPECT_EQ(sanitise("hc_pat_abcdef0123456789"), "hc_pat_abcdef0123456789");
}

TEST(HardcoverTokenImport, StripsSurroundingWhitespace) {
  EXPECT_EQ(sanitise("  \thc_pat_abcdef0123456789 \t "), "hc_pat_abcdef0123456789");
}

TEST(HardcoverTokenImport, StopsAtTheFirstLineBreak) {
  EXPECT_EQ(sanitise("hc_pat_abcdef0123456789\nnotes to self\n"), "hc_pat_abcdef0123456789");
  EXPECT_EQ(sanitise("hc_pat_abcdef0123456789\r\n"), "hc_pat_abcdef0123456789");
}

TEST(HardcoverTokenImport, SkipsLeadingBlankLines) {
  EXPECT_EQ(sanitise("\n\n  hc_pat_abcdef0123456789\n"), "hc_pat_abcdef0123456789");
}

TEST(HardcoverTokenImport, EmptyAndWhitespaceOnlyYieldNothing) {
  EXPECT_EQ(sanitise(""), "");
  EXPECT_EQ(sanitise("   \r\n  "), "");
}

// Anything left in the middle is preserved verbatim, so a mistyped token is
// rejected by the validator rather than silently repaired here.
TEST(HardcoverTokenImport, DoesNotAlterInteriorCharacters) {
  EXPECT_EQ(sanitise(" hc_pat_ab cd \n"), "hc_pat_ab cd");
}
