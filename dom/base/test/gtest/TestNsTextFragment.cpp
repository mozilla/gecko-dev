/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/OriginAttributes.h"
#include "mozilla/dom/Document.h"
#include "nsCOMPtr.h"
#include "nsIURI.h"
#include "nsTextFragment.h"
#include "nsTextNode.h"
#include "nsString.h"

namespace mozilla::dom {

using WhitespaceOption = nsTextFragment::WhitespaceOption;
using WhitespaceOptions = nsTextFragment::WhitespaceOptions;

static already_AddRefed<Document> CreateHTMLDoc() {
  nsCOMPtr<nsIURI> uri;
  NS_NewURI(getter_AddRefs(uri), "data:text/html,");

  RefPtr<BasePrincipal> principal =
      BasePrincipal::CreateContentPrincipal(uri, OriginAttributes());
  MOZ_RELEASE_ASSERT(principal);

  nsCOMPtr<mozilla::dom::Document> doc;
  MOZ_ALWAYS_SUCCEEDS(NS_NewDOMDocument(getter_AddRefs(doc),
                                        u""_ns,   // aNamespaceURI
                                        u""_ns,   // aQualifiedName
                                        nullptr,  // aDoctype
                                        uri, uri, principal,
                                        false,    // aLoadedAsData
                                        nullptr,  // aEventObject
                                        DocumentFlavor::HTML));
  MOZ_RELEASE_ASSERT(doc);
  return doc.forget();
}

struct TestData {
  TestData(const char16_t* aData, const char16_t* aScanData,
           uint32_t aStartOffset, uint32_t aExpectedOffset)
      : mData(aData),
        mScanData(aScanData),
        mStartOffset(aStartOffset),
        mExpectedOffset(aExpectedOffset) {}

  friend std::ostream& operator<<(std::ostream& aStream,
                                  const TestData& aData) {
    return aStream << "Scan \"" << aData.mScanData << "\" in \"" << aData.mData
                   << "\" starting from " << aData.mStartOffset;
  }

  const char16_t* const mData;
  const char16_t* const mScanData;
  const uint32_t mStartOffset;
  const uint32_t mExpectedOffset;
};

TEST(nsTextFragmentTest, FindFirstDifferentCharOffsetIn1b)
{
  const RefPtr<Document> doc = CreateHTMLDoc();
  const RefPtr<nsTextNode> textNode = doc->CreateTextNode(EmptyString());
  MOZ_RELEASE_ASSERT(textNode);
  const nsTextFragment& textFragment = textNode->TextFragment();

  for (const auto& testData : {
           TestData(u"abcdef", u"abc", 0, nsTextFragment::kNotFound),
           TestData(u"abcdef", u"Abc", 0, 0),
           TestData(u"abcdef", u"aBc", 0, 1),
           TestData(u"abcdef", u"abC", 0, 2),
           TestData(u"abcdef", u"def", 3, nsTextFragment::kNotFound),
           TestData(u"abcdef", u"Def", 3, 3),
           TestData(u"abcdef", u"dEf", 3, 4),
           TestData(u"abcdef", u"deF", 3, 5),
       }) {
    textNode->SetData(nsDependentString(testData.mData), IgnoreErrors());
    MOZ_ASSERT(!textFragment.Is2b());
    const uint32_t ret = textFragment.FindFirstDifferentCharOffset(
        NS_ConvertUTF16toUTF8(testData.mScanData), testData.mStartOffset);
    EXPECT_EQ(ret, testData.mExpectedOffset) << testData;
  }
}

TEST(nsTextFragmentTest, FindFirstDifferentCharOffsetIn2b)
{
  const RefPtr<Document> doc = CreateHTMLDoc();
  const RefPtr<nsTextNode> textNode = doc->CreateTextNode(EmptyString());
  MOZ_RELEASE_ASSERT(textNode);
  textNode->MarkAsMaybeModifiedFrequently();
  const nsTextFragment& textFragment = textNode->TextFragment();

  for (const auto& testData : {
           TestData(u"abcdef", u"abc", 0, nsTextFragment::kNotFound),
           TestData(u"abcdef", u"Abc", 0, 0),
           TestData(u"abcdef", u"aBc", 0, 1),
           TestData(u"abcdef", u"abC", 0, 2),
           TestData(u"abcdef", u"def", 3, nsTextFragment::kNotFound),
           TestData(u"abcdef", u"Def", 3, 3),
           TestData(u"abcdef", u"dEf", 3, 4),
           TestData(u"abcdef", u"deF", 3, 5),
       }) {
    textNode->SetData(nsDependentString(testData.mData), IgnoreErrors());
    MOZ_ASSERT(textFragment.Is2b());
    const uint32_t ret = textFragment.FindFirstDifferentCharOffset(
        NS_ConvertUTF16toUTF8(testData.mScanData), testData.mStartOffset);
    EXPECT_EQ(ret, testData.mExpectedOffset) << testData;
  }
}

TEST(nsTextFragmentTest, RFindFirstDifferentCharOffsetIn1b)
{
  const RefPtr<Document> doc = CreateHTMLDoc();
  const RefPtr<nsTextNode> textNode = doc->CreateTextNode(EmptyString());
  MOZ_RELEASE_ASSERT(textNode);
  const nsTextFragment& textFragment = textNode->TextFragment();

  for (const auto& testData : {
           TestData(u"abcdef", u"abc", 3, nsTextFragment::kNotFound),
           TestData(u"abcdef", u"Abc", 3, 0),
           TestData(u"abcdef", u"aBc", 3, 1),
           TestData(u"abcdef", u"abC", 3, 2),
           TestData(u"abcdef", u"def", 6, nsTextFragment::kNotFound),
           TestData(u"abcdef", u"Def", 6, 3),
           TestData(u"abcdef", u"dEf", 6, 4),
           TestData(u"abcdef", u"deF", 6, 5),
       }) {
    textNode->SetData(nsDependentString(testData.mData), IgnoreErrors());
    MOZ_ASSERT(!textFragment.Is2b());
    const uint32_t ret = textFragment.RFindFirstDifferentCharOffset(
        NS_ConvertUTF16toUTF8(testData.mScanData), testData.mStartOffset);
    EXPECT_EQ(ret, testData.mExpectedOffset) << testData;
  }
}

TEST(nsTextFragmentTest, RFindFirstDifferentCharOffsetIn2b)
{
  const RefPtr<Document> doc = CreateHTMLDoc();
  const RefPtr<nsTextNode> textNode = doc->CreateTextNode(EmptyString());
  MOZ_RELEASE_ASSERT(textNode);
  textNode->MarkAsMaybeModifiedFrequently();
  const nsTextFragment& textFragment = textNode->TextFragment();

  for (const auto& testData : {
           TestData(u"abcdef", u"abc", 3, nsTextFragment::kNotFound),
           TestData(u"abcdef", u"Abc", 3, 0),
           TestData(u"abcdef", u"aBc", 3, 1),
           TestData(u"abcdef", u"abC", 3, 2),
           TestData(u"abcdef", u"def", 6, nsTextFragment::kNotFound),
           TestData(u"abcdef", u"Def", 6, 3),
           TestData(u"abcdef", u"dEf", 6, 4),
           TestData(u"abcdef", u"deF", 6, 5),
       }) {
    textNode->SetData(nsDependentString(testData.mData), IgnoreErrors());
    MOZ_ASSERT(textFragment.Is2b());
    const uint32_t ret = textFragment.RFindFirstDifferentCharOffset(
        NS_ConvertUTF16toUTF8(testData.mScanData), testData.mStartOffset);
    EXPECT_EQ(ret, testData.mExpectedOffset) << testData;
  }
}

struct TestDataForFindNonWhitespace {
  TestDataForFindNonWhitespace(const char16_t* aData, uint32_t aOffset,
                               const WhitespaceOptions& aOptions,
                               uint32_t aExpectedOffset)
      : mData(aData),
        mOffset(aOffset),
        mExpectedOffset(aExpectedOffset),
        mOptions(aOptions) {}

  friend std::ostream& operator<<(std::ostream& aStream,
                                  const TestDataForFindNonWhitespace& aData) {
    aStream << "Scan with options={";
    bool isFirstOption = true;
    if (aData.mOptions.contains(WhitespaceOption::FormFeedIsSignificant)) {
      aStream << "WhitespaceOption::FormFeedIsSignificant";
      isFirstOption = false;
    }
    if (aData.mOptions.contains(WhitespaceOption::NewLineIsSignificant)) {
      if (!isFirstOption) {
        aStream << ", ";
      }
      aStream << "WhitespaceOption::NewLineIsSignificant";
      isFirstOption = false;
    }
    if (aData.mOptions.contains(WhitespaceOption::TreatNBSPAsCollapsible)) {
      if (!isFirstOption) {
        aStream << ", ";
      }
      aStream << "WhitespaceOption::TreatNBSPAsCollapsible";
      isFirstOption = false;
    }
    return aStream << "} in \"" << aData.FormatUTF8Data().get()
                   << "\" starting from " << aData.mOffset;
  }

  nsAutoCString FormatUTF8Data() const {
    nsAutoString data(mData);
    data.ReplaceSubstring(u"\n"_ns, u"\\n"_ns);
    data.ReplaceSubstring(u"\t"_ns, u"\\t"_ns);
    data.ReplaceSubstring(u"\r"_ns, u"\\r"_ns);
    data.ReplaceSubstring(u"\f"_ns, u"\\f"_ns);
    data.ReplaceSubstring(u"\u00A0"_ns, u"&nbsp;"_ns);
    return NS_ConvertUTF16toUTF8(data);
  }

  const char16_t* const mData;
  const uint32_t mOffset;
  const uint32_t mExpectedOffset;
  const WhitespaceOptions mOptions;
};

TEST(nsTextFragmentTest, FindNonWhitespaceIn1b)
{
  const RefPtr<Document> doc = CreateHTMLDoc();
  const RefPtr<nsTextNode> textNode = doc->CreateTextNode(EmptyString());
  MOZ_RELEASE_ASSERT(textNode);
  const nsTextFragment& textFragment = textNode->TextFragment();

  for (const auto& testData : {
           TestDataForFindNonWhitespace(u"", 0, {}, nsTextFragment::kNotFound),
           TestDataForFindNonWhitespace(u" ", 0, {}, nsTextFragment::kNotFound),
           TestDataForFindNonWhitespace(u"  ", 0, {},
                                        nsTextFragment::kNotFound),
           TestDataForFindNonWhitespace(u"\t\n\r\f", 0, {},
                                        nsTextFragment::kNotFound),
           TestDataForFindNonWhitespace(u" \t\n\r\f", 0, {},
                                        nsTextFragment::kNotFound),
           TestDataForFindNonWhitespace(u"a", 0, {}, 0),
           TestDataForFindNonWhitespace(u" a", 0, {}, 1),
           TestDataForFindNonWhitespace(u"\u00A0", 0, {}, 0),
           TestDataForFindNonWhitespace(u" \u00A0", 0, {}, 1),
           TestDataForFindNonWhitespace(u"a b", 1, {}, 2),
           TestDataForFindNonWhitespace(u"a b", 2, {}, 2),
           TestDataForFindNonWhitespace(
               u"\fa", 0, {WhitespaceOption::FormFeedIsSignificant}, 0),
           TestDataForFindNonWhitespace(
               u" \fa", 0, {WhitespaceOption::FormFeedIsSignificant}, 1),
           TestDataForFindNonWhitespace(
               u"\n", 0, {WhitespaceOption::NewLineIsSignificant}, 0),
           TestDataForFindNonWhitespace(
               u" \n", 0, {WhitespaceOption::NewLineIsSignificant}, 1),
           TestDataForFindNonWhitespace(
               u"\u00A0", 0, {WhitespaceOption::TreatNBSPAsCollapsible},
               nsTextFragment::kNotFound),
           TestDataForFindNonWhitespace(
               u" \u00A0", 0, {WhitespaceOption::TreatNBSPAsCollapsible},
               nsTextFragment::kNotFound),
       }) {
    textNode->SetData(nsDependentString(testData.mData), IgnoreErrors());
    MOZ_ASSERT(!textFragment.Is2b());
    const uint32_t ret =
        textFragment.FindNonWhitespaceChar(testData.mOptions, testData.mOffset);
    EXPECT_EQ(ret, testData.mExpectedOffset) << testData;
  }
}

TEST(nsTextFragmentTest, FindNonWhitespaceIn2b)
{
  const RefPtr<Document> doc = CreateHTMLDoc();
  const RefPtr<nsTextNode> textNode = doc->CreateTextNode(EmptyString());
  MOZ_RELEASE_ASSERT(textNode);
  textNode->MarkAsMaybeModifiedFrequently();
  const nsTextFragment& textFragment = textNode->TextFragment();

  for (const auto& testData : {
           TestDataForFindNonWhitespace(u" ", 0, {}, nsTextFragment::kNotFound),
           TestDataForFindNonWhitespace(u"  ", 0, {},
                                        nsTextFragment::kNotFound),
           TestDataForFindNonWhitespace(u"\t\n\r\f", 0, {},
                                        nsTextFragment::kNotFound),
           TestDataForFindNonWhitespace(u" \t\n\r\f", 0, {},
                                        nsTextFragment::kNotFound),
           TestDataForFindNonWhitespace(u"a", 0, {}, 0),
           TestDataForFindNonWhitespace(u" a", 0, {}, 1),
           TestDataForFindNonWhitespace(u"\u00A0", 0, {}, 0),
           TestDataForFindNonWhitespace(u" \u00A0", 0, {}, 1),
           TestDataForFindNonWhitespace(u"a b", 1, {}, 2),
           TestDataForFindNonWhitespace(u"a b", 2, {}, 2),
           TestDataForFindNonWhitespace(
               u"\fa", 0, {WhitespaceOption::FormFeedIsSignificant}, 0),
           TestDataForFindNonWhitespace(
               u" \fa", 0, {WhitespaceOption::FormFeedIsSignificant}, 1),
           TestDataForFindNonWhitespace(
               u"\n", 0, {WhitespaceOption::NewLineIsSignificant}, 0),
           TestDataForFindNonWhitespace(
               u" \n", 0, {WhitespaceOption::NewLineIsSignificant}, 1),
           TestDataForFindNonWhitespace(
               u"\u00A0", 0, {WhitespaceOption::TreatNBSPAsCollapsible},
               nsTextFragment::kNotFound),
           TestDataForFindNonWhitespace(
               u" \u00A0", 0, {WhitespaceOption::TreatNBSPAsCollapsible},
               nsTextFragment::kNotFound),
       }) {
    textNode->SetData(nsDependentString(testData.mData), IgnoreErrors());
    MOZ_ASSERT(textFragment.Is2b());
    const uint32_t ret =
        textFragment.FindNonWhitespaceChar(testData.mOptions, testData.mOffset);
    EXPECT_EQ(ret, testData.mExpectedOffset) << testData;
  }
}

TEST(nsTextFragmentTest, RFindNonWhitespaceIn1b)
{
  const RefPtr<Document> doc = CreateHTMLDoc();
  const RefPtr<nsTextNode> textNode = doc->CreateTextNode(EmptyString());
  MOZ_RELEASE_ASSERT(textNode);
  const nsTextFragment& textFragment = textNode->TextFragment();

  for (const auto& testData : {
           TestDataForFindNonWhitespace(u"", UINT32_MAX, {},
                                        nsTextFragment::kNotFound),
           TestDataForFindNonWhitespace(u" ", UINT32_MAX, {},
                                        nsTextFragment::kNotFound),
           TestDataForFindNonWhitespace(u"  ", UINT32_MAX, {},
                                        nsTextFragment::kNotFound),
           TestDataForFindNonWhitespace(u"\t\n\r\f", UINT32_MAX, {},
                                        nsTextFragment::kNotFound),
           TestDataForFindNonWhitespace(u"\t\n\r\f ", UINT32_MAX, {},
                                        nsTextFragment::kNotFound),
           TestDataForFindNonWhitespace(u"a", UINT32_MAX, {}, 0),
           TestDataForFindNonWhitespace(u"a ", UINT32_MAX, {}, 0),
           TestDataForFindNonWhitespace(u"ab", UINT32_MAX, {}, 1),
           TestDataForFindNonWhitespace(u"ab ", UINT32_MAX, {}, 1),
           TestDataForFindNonWhitespace(u"a\u00A0", UINT32_MAX, {}, 1),
           TestDataForFindNonWhitespace(u"a\u00A0 ", UINT32_MAX, {}, 1),
           TestDataForFindNonWhitespace(u"a b", 1, {}, 0),
           TestDataForFindNonWhitespace(u"a b", 0, {}, 0),
           TestDataForFindNonWhitespace(
               u"a\f", UINT32_MAX, {WhitespaceOption::FormFeedIsSignificant},
               1),
           TestDataForFindNonWhitespace(
               u"a\f ", UINT32_MAX, {WhitespaceOption::FormFeedIsSignificant},
               1),
           TestDataForFindNonWhitespace(
               u"a\n", UINT32_MAX, {WhitespaceOption::NewLineIsSignificant}, 1),
           TestDataForFindNonWhitespace(
               u"a\n ", UINT32_MAX, {WhitespaceOption::NewLineIsSignificant},
               1),
           TestDataForFindNonWhitespace(
               u"a\u00A0", UINT32_MAX,
               {WhitespaceOption::TreatNBSPAsCollapsible}, 0),
           TestDataForFindNonWhitespace(
               u"a\u00A0 ", UINT32_MAX,
               {WhitespaceOption::TreatNBSPAsCollapsible}, 0),
       }) {
    textNode->SetData(nsDependentString(testData.mData), IgnoreErrors());
    MOZ_ASSERT(!textFragment.Is2b());
    const uint32_t ret = textFragment.RFindNonWhitespaceChar(testData.mOptions,
                                                             testData.mOffset);
    EXPECT_EQ(ret, testData.mExpectedOffset) << testData;
  }
}

TEST(nsTextFragmentTest, RFindNonWhitespaceIn2b)
{
  const RefPtr<Document> doc = CreateHTMLDoc();
  const RefPtr<nsTextNode> textNode = doc->CreateTextNode(EmptyString());
  MOZ_RELEASE_ASSERT(textNode);
  textNode->MarkAsMaybeModifiedFrequently();
  const nsTextFragment& textFragment = textNode->TextFragment();

  for (const auto& testData : {
           TestDataForFindNonWhitespace(u" ", UINT32_MAX, {},
                                        nsTextFragment::kNotFound),
           TestDataForFindNonWhitespace(u"  ", UINT32_MAX, {},
                                        nsTextFragment::kNotFound),
           TestDataForFindNonWhitespace(u"\t\n\r\f", UINT32_MAX, {},
                                        nsTextFragment::kNotFound),
           TestDataForFindNonWhitespace(u"\t\n\r\f ", UINT32_MAX, {},
                                        nsTextFragment::kNotFound),
           TestDataForFindNonWhitespace(u"a", UINT32_MAX, {}, 0),
           TestDataForFindNonWhitespace(u"a ", UINT32_MAX, {}, 0),
           TestDataForFindNonWhitespace(u"ab", UINT32_MAX, {}, 1),
           TestDataForFindNonWhitespace(u"ab ", UINT32_MAX, {}, 1),
           TestDataForFindNonWhitespace(u"a\u00A0", UINT32_MAX, {}, 1),
           TestDataForFindNonWhitespace(u"a\u00A0 ", UINT32_MAX, {}, 1),
           TestDataForFindNonWhitespace(u"a b", 1, {}, 0),
           TestDataForFindNonWhitespace(u"a b", 0, {}, 0),
           TestDataForFindNonWhitespace(
               u"a\f", UINT32_MAX, {WhitespaceOption::FormFeedIsSignificant},
               1),
           TestDataForFindNonWhitespace(
               u"a\f ", UINT32_MAX, {WhitespaceOption::FormFeedIsSignificant},
               1),
           TestDataForFindNonWhitespace(
               u"a\n", UINT32_MAX, {WhitespaceOption::NewLineIsSignificant}, 1),
           TestDataForFindNonWhitespace(
               u"a\n ", UINT32_MAX, {WhitespaceOption::NewLineIsSignificant},
               1),
           TestDataForFindNonWhitespace(
               u"a\u00A0", UINT32_MAX,
               {WhitespaceOption::TreatNBSPAsCollapsible}, 0),
           TestDataForFindNonWhitespace(
               u"a\u00A0 ", UINT32_MAX,
               {WhitespaceOption::TreatNBSPAsCollapsible}, 0),
       }) {
    textNode->SetData(nsDependentString(testData.mData), IgnoreErrors());
    MOZ_ASSERT(textFragment.Is2b());
    const uint32_t ret = textFragment.RFindNonWhitespaceChar(testData.mOptions,
                                                             testData.mOffset);
    EXPECT_EQ(ret, testData.mExpectedOffset) << testData;
  }
}

};  // namespace mozilla::dom
