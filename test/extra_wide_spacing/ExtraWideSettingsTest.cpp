#include <gtest/gtest.h>

#include <ArduinoJson.h>

#include "CrossPointSettings.h"
#include "ReaderLineSpacing.h"

#include <array>
#include <cmath>
#include <cstring>

namespace {

CrossPointSettings& settings() {
  auto& value = CrossPointSettings::getInstance();
  value.lineSpacing = CrossPointSettings::NORMAL;
  value.fontFamily = CrossPointSettings::NOTOSERIF;
  value.fontPointSize = CrossPointSettings::DEFAULT_FONT_POINT_SIZE;
  value.sdFontFamilyName[0] = '\0';
  return value;
}

TEST(ExtraWideSettings, EnumAndJsonRoundTripKeepTheAppendedValue) {
  auto& value = settings();
  static_assert(CrossPointSettings::TIGHT == 0);
  static_assert(CrossPointSettings::NORMAL == 1);
  static_assert(CrossPointSettings::WIDE == 2);
  static_assert(CrossPointSettings::EXTRA_WIDE == 3);
  static_assert(CrossPointSettings::LINE_COMPRESSION_COUNT == 4);

  value.lineSpacing = CrossPointSettings::EXTRA_WIDE;
  JsonDocument saved;
  value.toJson(saved);
  ASSERT_EQ(saved["lineSpacing"].as<uint8_t>(), CrossPointSettings::EXTRA_WIDE);

  value.lineSpacing = CrossPointSettings::NORMAL;
  ASSERT_TRUE(value.fromJson(saved.as<JsonVariantConst>()));
  EXPECT_EQ(value.lineSpacing, CrossPointSettings::EXTRA_WIDE);
}

TEST(ExtraWideSettings, ExistingMultipliersRemainExact) {
  auto& value = settings();
  struct Expectation {
    uint8_t family;
    std::array<float, 4> multipliers;
  };
  const Expectation cases[] = {
      {CrossPointSettings::NOTOSERIF, {0.95f, 1.0f, 1.1f, 1.2f}},
      {CrossPointSettings::NOTOSANS, {0.90f, 0.95f, 1.0f, 1.05f}},
      {CrossPointSettings::LEXEND_DECA, {0.95f, 1.0f, 1.1f, 1.2f}},
      {CrossPointSettings::BITTER, {0.95f, 1.0f, 1.1f, 1.2f}},
  };

  for (const auto& testCase : cases) {
    value.fontFamily = testCase.family;
    for (uint8_t spacing = 0; spacing < CrossPointSettings::LINE_COMPRESSION_COUNT; ++spacing) {
      value.lineSpacing = spacing;
      EXPECT_FLOAT_EQ(value.getReaderLineCompression(), testCase.multipliers[spacing]);
    }
  }

  std::strcpy(value.sdFontFamilyName, "Fixture Sans");
  for (uint8_t spacing = 0; spacing < CrossPointSettings::LINE_COMPRESSION_COUNT; ++spacing) {
    value.lineSpacing = spacing;
    EXPECT_FLOAT_EQ(value.getReaderLineCompression(), cases[0].multipliers[spacing]);
  }
}

TEST(ExtraWideSettings, RenderSpecCarriesExactMultiplierAndLineHeight) {
  auto& value = settings();
  value.lineSpacing = CrossPointSettings::EXTRA_WIDE;
  const auto spec = value.readerRenderSpec(480, 800);
  EXPECT_FLOAT_EQ(spec.lineCompression, 1.2f);
  EXPECT_EQ(static_cast<int>(12 * spec.lineCompression), 14);
  EXPECT_EQ(spec.viewportWidth, 480);
  EXPECT_EQ(spec.viewportHeight, 800);
}

TEST(ExtraWideSettings, InvalidStoredValueFallsBackWithoutReinterpretingExistingValues) {
  auto& value = settings();
  JsonDocument invalid;
  invalid["lineSpacing"] = 99;
  ASSERT_TRUE(value.fromJson(invalid.as<JsonVariantConst>()));
  EXPECT_EQ(value.lineSpacing, CrossPointSettings::NORMAL);

  for (uint8_t spacing = CrossPointSettings::TIGHT; spacing <= CrossPointSettings::WIDE; ++spacing) {
    JsonDocument existing;
    existing["lineSpacing"] = spacing;
    value.lineSpacing = CrossPointSettings::NORMAL;
    ASSERT_TRUE(value.fromJson(existing.as<JsonVariantConst>()));
    EXPECT_EQ(value.lineSpacing, spacing);
  }
}

TEST(ExtraWideSettings, SharedSettingsAndTouchLabelsStayInEnumOrder) {
  static_assert(ReaderLineSpacing::LABEL_IDS.size() == CrossPointSettings::LINE_COMPRESSION_COUNT);
  EXPECT_EQ(ReaderLineSpacing::LABEL_IDS[CrossPointSettings::TIGHT], StrId::STR_TIGHT);
  EXPECT_EQ(ReaderLineSpacing::LABEL_IDS[CrossPointSettings::NORMAL], StrId::STR_NORMAL);
  EXPECT_EQ(ReaderLineSpacing::LABEL_IDS[CrossPointSettings::WIDE], StrId::STR_WIDE);
  EXPECT_EQ(ReaderLineSpacing::LABEL_IDS[CrossPointSettings::EXTRA_WIDE], StrId::STR_EXTRA_WIDE);
}

}  // namespace
