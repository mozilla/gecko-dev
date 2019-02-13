/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHTMLTags.h"
#include "nsCRT.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsStaticAtom.h"
#include "nsUnicharUtils.h"
#include "mozilla/HashFunctions.h"
#include <algorithm>

using namespace mozilla;

// C++ sucks! There's no way to do this with a macro, at least not
// that I know, if you know how to do this with a macro then please do
// so...
static const char16_t sHTMLTagUnicodeName_a[] =
  {'a', '\0'};
static const char16_t sHTMLTagUnicodeName_abbr[] =
  {'a', 'b', 'b', 'r', '\0'};
static const char16_t sHTMLTagUnicodeName_acronym[] =
  {'a', 'c', 'r', 'o', 'n', 'y', 'm', '\0'};
static const char16_t sHTMLTagUnicodeName_address[] =
  {'a', 'd', 'd', 'r', 'e', 's', 's', '\0'};
static const char16_t sHTMLTagUnicodeName_applet[] =
  {'a', 'p', 'p', 'l', 'e', 't', '\0'};
static const char16_t sHTMLTagUnicodeName_area[] =
  {'a', 'r', 'e', 'a', '\0'};
static const char16_t sHTMLTagUnicodeName_article[] =
  {'a', 'r', 't', 'i', 'c', 'l', 'e', '\0'};
static const char16_t sHTMLTagUnicodeName_aside[] =
  {'a', 's', 'i', 'd', 'e', '\0'};
static const char16_t sHTMLTagUnicodeName_audio[] =
  {'a', 'u', 'd', 'i', 'o', '\0'};
static const char16_t sHTMLTagUnicodeName_b[] =
  {'b', '\0'};
static const char16_t sHTMLTagUnicodeName_base[] =
  {'b', 'a', 's', 'e', '\0'};
static const char16_t sHTMLTagUnicodeName_basefont[] =
  {'b', 'a', 's', 'e', 'f', 'o', 'n', 't', '\0'};
static const char16_t sHTMLTagUnicodeName_bdo[] =
  {'b', 'd', 'o', '\0'};
static const char16_t sHTMLTagUnicodeName_bgsound[] =
  {'b', 'g', 's', 'o', 'u', 'n', 'd', '\0'};
static const char16_t sHTMLTagUnicodeName_big[] =
  {'b', 'i', 'g', '\0'};
static const char16_t sHTMLTagUnicodeName_blockquote[] =
  {'b', 'l', 'o', 'c', 'k', 'q', 'u', 'o', 't', 'e', '\0'};
static const char16_t sHTMLTagUnicodeName_body[] =
  {'b', 'o', 'd', 'y', '\0'};
static const char16_t sHTMLTagUnicodeName_br[] =
  {'b', 'r', '\0'};
static const char16_t sHTMLTagUnicodeName_button[] =
  {'b', 'u', 't', 't', 'o', 'n', '\0'};
static const char16_t sHTMLTagUnicodeName_canvas[] =
  {'c', 'a', 'n', 'v', 'a', 's', '\0'};
static const char16_t sHTMLTagUnicodeName_caption[] =
  {'c', 'a', 'p', 't', 'i', 'o', 'n', '\0'};
static const char16_t sHTMLTagUnicodeName_center[] =
  {'c', 'e', 'n', 't', 'e', 'r', '\0'};
static const char16_t sHTMLTagUnicodeName_cite[] =
  {'c', 'i', 't', 'e', '\0'};
static const char16_t sHTMLTagUnicodeName_code[] =
  {'c', 'o', 'd', 'e', '\0'};
static const char16_t sHTMLTagUnicodeName_col[] =
  {'c', 'o', 'l', '\0'};
static const char16_t sHTMLTagUnicodeName_colgroup[] =
  {'c', 'o', 'l', 'g', 'r', 'o', 'u', 'p', '\0'};
static const char16_t sHTMLTagUnicodeName_content[] =
  {'c', 'o', 'n', 't', 'e', 'n', 't', '\0'};
static const char16_t sHTMLTagUnicodeName_data[] =
  {'d', 'a', 't', 'a', '\0'};
static const char16_t sHTMLTagUnicodeName_datalist[] =
  {'d', 'a', 't', 'a', 'l', 'i', 's', 't', '\0'};
static const char16_t sHTMLTagUnicodeName_dd[] =
  {'d', 'd', '\0'};
static const char16_t sHTMLTagUnicodeName_del[] =
  {'d', 'e', 'l', '\0'};
static const char16_t sHTMLTagUnicodeName_dfn[] =
  {'d', 'f', 'n', '\0'};
static const char16_t sHTMLTagUnicodeName_dir[] =
  {'d', 'i', 'r', '\0'};
static const char16_t sHTMLTagUnicodeName_div[] =
  {'d', 'i', 'v', '\0'};
static const char16_t sHTMLTagUnicodeName_dl[] =
  {'d', 'l', '\0'};
static const char16_t sHTMLTagUnicodeName_dt[] =
  {'d', 't', '\0'};
static const char16_t sHTMLTagUnicodeName_em[] =
  {'e', 'm', '\0'};
static const char16_t sHTMLTagUnicodeName_embed[] =
  {'e', 'm', 'b', 'e', 'd', '\0'};
static const char16_t sHTMLTagUnicodeName_fieldset[] =
  {'f', 'i', 'e', 'l', 'd', 's', 'e', 't', '\0'};
static const char16_t sHTMLTagUnicodeName_figcaption[] =
  {'f', 'i', 'g', 'c', 'a', 'p', 't', 'i', 'o', 'n', '\0'};
static const char16_t sHTMLTagUnicodeName_figure[] =
  {'f', 'i', 'g', 'u', 'r', 'e', '\0'};
static const char16_t sHTMLTagUnicodeName_font[] =
  {'f', 'o', 'n', 't', '\0'};
static const char16_t sHTMLTagUnicodeName_footer[] =
  {'f', 'o', 'o', 't', 'e', 'r', '\0'};
static const char16_t sHTMLTagUnicodeName_form[] =
  {'f', 'o', 'r', 'm', '\0'};
static const char16_t sHTMLTagUnicodeName_frame[] =
  {'f', 'r', 'a', 'm', 'e', '\0'};
static const char16_t sHTMLTagUnicodeName_frameset[] =
  {'f', 'r', 'a', 'm', 'e', 's', 'e', 't', '\0'};
static const char16_t sHTMLTagUnicodeName_h1[] =
  {'h', '1', '\0'};
static const char16_t sHTMLTagUnicodeName_h2[] =
  {'h', '2', '\0'};
static const char16_t sHTMLTagUnicodeName_h3[] =
  {'h', '3', '\0'};
static const char16_t sHTMLTagUnicodeName_h4[] =
  {'h', '4', '\0'};
static const char16_t sHTMLTagUnicodeName_h5[] =
  {'h', '5', '\0'};
static const char16_t sHTMLTagUnicodeName_h6[] =
  {'h', '6', '\0'};
static const char16_t sHTMLTagUnicodeName_head[] =
  {'h', 'e', 'a', 'd', '\0'};
static const char16_t sHTMLTagUnicodeName_header[] =
  {'h', 'e', 'a', 'd', 'e', 'r', '\0'};
static const char16_t sHTMLTagUnicodeName_hgroup[] =
  {'h', 'g', 'r', 'o', 'u', 'p', '\0'};
static const char16_t sHTMLTagUnicodeName_hr[] =
  {'h', 'r', '\0'};
static const char16_t sHTMLTagUnicodeName_html[] =
  {'h', 't', 'm', 'l', '\0'};
static const char16_t sHTMLTagUnicodeName_i[] =
  {'i', '\0'};
static const char16_t sHTMLTagUnicodeName_iframe[] =
  {'i', 'f', 'r', 'a', 'm', 'e', '\0'};
static const char16_t sHTMLTagUnicodeName_image[] =
  {'i', 'm', 'a', 'g', 'e', '\0'};
static const char16_t sHTMLTagUnicodeName_img[] =
  {'i', 'm', 'g', '\0'};
static const char16_t sHTMLTagUnicodeName_input[] =
  {'i', 'n', 'p', 'u', 't', '\0'};
static const char16_t sHTMLTagUnicodeName_ins[] =
  {'i', 'n', 's', '\0'};
static const char16_t sHTMLTagUnicodeName_kbd[] =
  {'k', 'b', 'd', '\0'};
static const char16_t sHTMLTagUnicodeName_keygen[] =
  {'k', 'e', 'y', 'g', 'e', 'n', '\0'};
static const char16_t sHTMLTagUnicodeName_label[] =
  {'l', 'a', 'b', 'e', 'l', '\0'};
static const char16_t sHTMLTagUnicodeName_legend[] =
  {'l', 'e', 'g', 'e', 'n', 'd', '\0'};
static const char16_t sHTMLTagUnicodeName_li[] =
  {'l', 'i', '\0'};
static const char16_t sHTMLTagUnicodeName_link[] =
  {'l', 'i', 'n', 'k', '\0'};
static const char16_t sHTMLTagUnicodeName_listing[] =
  {'l', 'i', 's', 't', 'i', 'n', 'g', '\0'};
static const char16_t sHTMLTagUnicodeName_main[] =
  {'m', 'a', 'i', 'n', '\0'};
static const char16_t sHTMLTagUnicodeName_map[] =
  {'m', 'a', 'p', '\0'};
static const char16_t sHTMLTagUnicodeName_mark[] =
  {'m', 'a', 'r', 'k', '\0'};
static const char16_t sHTMLTagUnicodeName_marquee[] =
  {'m', 'a', 'r', 'q', 'u', 'e', 'e', '\0'};
static const char16_t sHTMLTagUnicodeName_menu[] =
  {'m', 'e', 'n', 'u', '\0'};
static const char16_t sHTMLTagUnicodeName_menuitem[] =
  {'m', 'e', 'n', 'u', 'i', 't', 'e', 'm', '\0'};
static const char16_t sHTMLTagUnicodeName_meta[] =
  {'m', 'e', 't', 'a', '\0'};
static const char16_t sHTMLTagUnicodeName_meter[] =
  {'m', 'e', 't', 'e', 'r', '\0'};
static const char16_t sHTMLTagUnicodeName_multicol[] =
  {'m', 'u', 'l', 't', 'i', 'c', 'o', 'l', '\0'};
static const char16_t sHTMLTagUnicodeName_nav[] =
  {'n', 'a', 'v', '\0'};
static const char16_t sHTMLTagUnicodeName_nobr[] =
  {'n', 'o', 'b', 'r', '\0'};
static const char16_t sHTMLTagUnicodeName_noembed[] =
  {'n', 'o', 'e', 'm', 'b', 'e', 'd', '\0'};
static const char16_t sHTMLTagUnicodeName_noframes[] =
  {'n', 'o', 'f', 'r', 'a', 'm', 'e', 's', '\0'};
static const char16_t sHTMLTagUnicodeName_noscript[] =
  {'n', 'o', 's', 'c', 'r', 'i', 'p', 't', '\0'};
static const char16_t sHTMLTagUnicodeName_object[] =
  {'o', 'b', 'j', 'e', 'c', 't', '\0'};
static const char16_t sHTMLTagUnicodeName_ol[] =
  {'o', 'l', '\0'};
static const char16_t sHTMLTagUnicodeName_optgroup[] =
  {'o', 'p', 't', 'g', 'r', 'o', 'u', 'p', '\0'};
static const char16_t sHTMLTagUnicodeName_option[] =
  {'o', 'p', 't', 'i', 'o', 'n', '\0'};
static const char16_t sHTMLTagUnicodeName_output[] =
  {'o', 'u', 't', 'p', 'u', 't', '\0'};
static const char16_t sHTMLTagUnicodeName_p[] =
  {'p', '\0'};
static const char16_t sHTMLTagUnicodeName_param[] =
  {'p', 'a', 'r', 'a', 'm', '\0'};
static const char16_t sHTMLTagUnicodeName_picture[] =
  {'p', 'i', 'c', 't', 'u', 'r', 'e', '\0'};
static const char16_t sHTMLTagUnicodeName_plaintext[] =
  {'p', 'l', 'a', 'i', 'n', 't', 'e', 'x', 't', '\0'};
static const char16_t sHTMLTagUnicodeName_pre[] =
  {'p', 'r', 'e', '\0'};
static const char16_t sHTMLTagUnicodeName_progress[] =
  {'p', 'r', 'o', 'g', 'r', 'e', 's', 's', '\0'};
static const char16_t sHTMLTagUnicodeName_q[] =
  {'q', '\0'};
static const char16_t sHTMLTagUnicodeName_rb[] =
  {'r', 'b', '\0'};
static const char16_t sHTMLTagUnicodeName_rp[] =
  {'r', 'p', '\0'};
static const char16_t sHTMLTagUnicodeName_rt[] =
  {'r', 't', '\0'};
static const char16_t sHTMLTagUnicodeName_rtc[] =
  {'r', 't', 'c', '\0'};
static const char16_t sHTMLTagUnicodeName_ruby[] =
  {'r', 'u', 'b', 'y', '\0'};
static const char16_t sHTMLTagUnicodeName_s[] =
  {'s', '\0'};
static const char16_t sHTMLTagUnicodeName_samp[] =
  {'s', 'a', 'm', 'p', '\0'};
static const char16_t sHTMLTagUnicodeName_script[] =
  {'s', 'c', 'r', 'i', 'p', 't', '\0'};
static const char16_t sHTMLTagUnicodeName_section[] =
  {'s', 'e', 'c', 't', 'i', 'o', 'n', '\0'};
static const char16_t sHTMLTagUnicodeName_select[] =
  {'s', 'e', 'l', 'e', 'c', 't', '\0'};
static const char16_t sHTMLTagUnicodeName_shadow[] =
  {'s', 'h', 'a', 'd', 'o', 'w', '\0'};
static const char16_t sHTMLTagUnicodeName_small[] =
  {'s', 'm', 'a', 'l', 'l', '\0'};
static const char16_t sHTMLTagUnicodeName_source[] =
  {'s', 'o', 'u', 'r', 'c', 'e', '\0'};
static const char16_t sHTMLTagUnicodeName_span[] =
  {'s', 'p', 'a', 'n', '\0'};
static const char16_t sHTMLTagUnicodeName_strike[] =
  {'s', 't', 'r', 'i', 'k', 'e', '\0'};
static const char16_t sHTMLTagUnicodeName_strong[] =
  {'s', 't', 'r', 'o', 'n', 'g', '\0'};
static const char16_t sHTMLTagUnicodeName_style[] =
  {'s', 't', 'y', 'l', 'e', '\0'};
static const char16_t sHTMLTagUnicodeName_sub[] =
  {'s', 'u', 'b', '\0'};
static const char16_t sHTMLTagUnicodeName_sup[] =
  {'s', 'u', 'p', '\0'};
static const char16_t sHTMLTagUnicodeName_table[] =
  {'t', 'a', 'b', 'l', 'e', '\0'};
static const char16_t sHTMLTagUnicodeName_tbody[] =
  {'t', 'b', 'o', 'd', 'y', '\0'};
static const char16_t sHTMLTagUnicodeName_td[] =
  {'t', 'd', '\0'};
static const char16_t sHTMLTagUnicodeName_textarea[] =
  {'t', 'e', 'x', 't', 'a', 'r', 'e', 'a', '\0'};
static const char16_t sHTMLTagUnicodeName_tfoot[] =
  {'t', 'f', 'o', 'o', 't', '\0'};
static const char16_t sHTMLTagUnicodeName_th[] =
  {'t', 'h', '\0'};
static const char16_t sHTMLTagUnicodeName_thead[] =
  {'t', 'h', 'e', 'a', 'd', '\0'};
static const char16_t sHTMLTagUnicodeName_template[] =
  {'t', 'e', 'm', 'p', 'l', 'a', 't', 'e', '\0'};
static const char16_t sHTMLTagUnicodeName_time[] =
  {'t', 'i', 'm', 'e', '\0'};
static const char16_t sHTMLTagUnicodeName_title[] =
  {'t', 'i', 't', 'l', 'e', '\0'};
static const char16_t sHTMLTagUnicodeName_tr[] =
  {'t', 'r', '\0'};
static const char16_t sHTMLTagUnicodeName_track[] =
  {'t', 'r', 'a', 'c', 'k', '\0'};
static const char16_t sHTMLTagUnicodeName_tt[] =
  {'t', 't', '\0'};
static const char16_t sHTMLTagUnicodeName_u[] =
  {'u', '\0'};
static const char16_t sHTMLTagUnicodeName_ul[] =
  {'u', 'l', '\0'};
static const char16_t sHTMLTagUnicodeName_var[] =
  {'v', 'a', 'r', '\0'};
static const char16_t sHTMLTagUnicodeName_video[] =
  {'v', 'i', 'd', 'e', 'o', '\0'};
static const char16_t sHTMLTagUnicodeName_wbr[] =
  {'w', 'b', 'r', '\0'};
static const char16_t sHTMLTagUnicodeName_xmp[] =
  {'x', 'm', 'p', '\0'};

// static array of unicode tag names
#define HTML_TAG(_tag, _classname) sHTMLTagUnicodeName_##_tag,
#define HTML_HTMLELEMENT_TAG(_tag) sHTMLTagUnicodeName_##_tag,
#define HTML_OTHER(_tag)
const char16_t* const nsHTMLTags::sTagUnicodeTable[] = {
#include "nsHTMLTagList.h"
};
#undef HTML_TAG
#undef HTML_HTMLELEMENT_TAG
#undef HTML_OTHER

// static array of tag atoms
nsIAtom* nsHTMLTags::sTagAtomTable[eHTMLTag_userdefined - 1];

int32_t nsHTMLTags::gTableRefCount;
PLHashTable* nsHTMLTags::gTagTable;
PLHashTable* nsHTMLTags::gTagAtomTable;


// char16_t* -> id hash
static PLHashNumber
HTMLTagsHashCodeUCPtr(const void *key)
{
  return HashString(static_cast<const char16_t*>(key));
}

static int
HTMLTagsKeyCompareUCPtr(const void *key1, const void *key2)
{
  const char16_t *str1 = (const char16_t *)key1;
  const char16_t *str2 = (const char16_t *)key2;

  return nsCRT::strcmp(str1, str2) == 0;
}

// nsIAtom* -> id hash
static PLHashNumber
HTMLTagsHashCodeAtom(const void *key)
{
  return NS_PTR_TO_INT32(key) >> 2;
}

#define NS_HTMLTAG_NAME_MAX_LENGTH 10

#define HTML_TAG(_tag, _classname) NS_STATIC_ATOM_BUFFER(Atombuffer_##_tag, #_tag)
#define HTML_HTMLELEMENT_TAG(_tag) NS_STATIC_ATOM_BUFFER(Atombuffer_##_tag, #_tag)
#define HTML_OTHER(_tag)
#include "nsHTMLTagList.h"
#undef HTML_TAG
#undef HTML_HTMLELEMENT_TAG
#undef HTML_OTHER


// static
nsresult
nsHTMLTags::AddRefTable(void)
{
  // static array of tag StaticAtom structs
#define HTML_TAG(_tag, _classname) NS_STATIC_ATOM(Atombuffer_##_tag, &nsHTMLTags::sTagAtomTable[eHTMLTag_##_tag - 1]),
#define HTML_HTMLELEMENT_TAG(_tag) NS_STATIC_ATOM(Atombuffer_##_tag, &nsHTMLTags::sTagAtomTable[eHTMLTag_##_tag - 1]),
#define HTML_OTHER(_tag)
  static const nsStaticAtom sTagAtoms_info[] = {
#include "nsHTMLTagList.h"
  };
#undef HTML_TAG
#undef HTML_HTMLELEMENT_TAG
#undef HTML_OTHER

  if (gTableRefCount++ == 0) {
    // Fill in our static atom pointers
    NS_RegisterStaticAtoms(sTagAtoms_info);


    NS_ASSERTION(!gTagTable && !gTagAtomTable, "pre existing hash!");

    gTagTable = PL_NewHashTable(64, HTMLTagsHashCodeUCPtr,
                                HTMLTagsKeyCompareUCPtr, PL_CompareValues,
                                nullptr, nullptr);
    NS_ENSURE_TRUE(gTagTable, NS_ERROR_OUT_OF_MEMORY);

    gTagAtomTable = PL_NewHashTable(64, HTMLTagsHashCodeAtom,
                                    PL_CompareValues, PL_CompareValues,
                                    nullptr, nullptr);
    NS_ENSURE_TRUE(gTagAtomTable, NS_ERROR_OUT_OF_MEMORY);

    // Fill in gTagTable with the above static char16_t strings as
    // keys and the value of the corresponding enum as the value in
    // the table.

    int32_t i;
    for (i = 0; i < NS_HTML_TAG_MAX; ++i) {
      PL_HashTableAdd(gTagTable, sTagUnicodeTable[i],
                      NS_INT32_TO_PTR(i + 1));

      PL_HashTableAdd(gTagAtomTable, sTagAtomTable[i],
                      NS_INT32_TO_PTR(i + 1));
    }



#if defined(DEBUG)
    {
      // let's verify that all names in the the table are lowercase...
      for (i = 0; i < NS_HTML_TAG_MAX; ++i) {
        nsAutoString temp1((char16_t*)sTagAtoms_info[i].mStringBuffer->Data());
        nsAutoString temp2((char16_t*)sTagAtoms_info[i].mStringBuffer->Data());
        ToLowerCase(temp1);
        NS_ASSERTION(temp1.Equals(temp2), "upper case char in table");
      }

      // let's verify that all names in the unicode strings above are
      // correct.
      for (i = 0; i < NS_HTML_TAG_MAX; ++i) {
        nsAutoString temp1(sTagUnicodeTable[i]);
        nsAutoString temp2((char16_t*)sTagAtoms_info[i].mStringBuffer->Data());
        NS_ASSERTION(temp1.Equals(temp2), "Bad unicode tag name!");
      }

      // let's verify that NS_HTMLTAG_NAME_MAX_LENGTH is correct
      uint32_t maxTagNameLength = 0;
      for (i = 0; i < NS_HTML_TAG_MAX; ++i) {
        uint32_t len = NS_strlen(sTagUnicodeTable[i]);
        maxTagNameLength = std::max(len, maxTagNameLength);        
      }
      NS_ASSERTION(maxTagNameLength == NS_HTMLTAG_NAME_MAX_LENGTH,
                   "NS_HTMLTAG_NAME_MAX_LENGTH not set correctly!");
    }
#endif
  }

  return NS_OK;
}

// static
void
nsHTMLTags::ReleaseTable(void)
{
  if (0 == --gTableRefCount) {
    if (gTagTable) {
      // Nothing to delete/free in this table, just destroy the table.

      PL_HashTableDestroy(gTagTable);
      PL_HashTableDestroy(gTagAtomTable);
      gTagTable = nullptr;
      gTagAtomTable = nullptr;
    }
  }
}

// static
nsHTMLTag
nsHTMLTags::LookupTag(const nsAString& aTagName)
{
  uint32_t length = aTagName.Length();

  if (length > NS_HTMLTAG_NAME_MAX_LENGTH) {
    return eHTMLTag_userdefined;
  }

  char16_t buf[NS_HTMLTAG_NAME_MAX_LENGTH + 1];

  nsAString::const_iterator iter;
  uint32_t i = 0;
  char16_t c;

  aTagName.BeginReading(iter);

  // Fast lowercasing-while-copying of ASCII characters into a
  // char16_t buffer

  while (i < length) {
    c = *iter;

    if (c <= 'Z' && c >= 'A') {
      c |= 0x20; // Lowercase the ASCII character.
    }

    buf[i] = c; // Copy ASCII character.

    ++i;
    ++iter;
  }

  buf[i] = 0;

  return CaseSensitiveLookupTag(buf);
}

#ifdef DEBUG
void
nsHTMLTags::TestTagTable()
{
     const char16_t *tag;
     nsHTMLTag id;
     nsCOMPtr<nsIAtom> atom;

     nsHTMLTags::AddRefTable();
     // Make sure we can find everything we are supposed to
     for (int i = 0; i < NS_HTML_TAG_MAX; ++i) {
       tag = sTagUnicodeTable[i];
       id = LookupTag(nsDependentString(tag));
       NS_ASSERTION(id != eHTMLTag_userdefined, "can't find tag id");
       const char16_t* check = GetStringValue(id);
       NS_ASSERTION(0 == nsCRT::strcmp(check, tag), "can't map id back to tag");

       nsAutoString uname(tag);
       ToUpperCase(uname);
       NS_ASSERTION(id == LookupTag(uname), "wrong id");

       NS_ASSERTION(id == CaseSensitiveLookupTag(tag), "wrong id");

       atom = do_GetAtom(tag);
       NS_ASSERTION(id == CaseSensitiveLookupTag(atom), "wrong id");
       NS_ASSERTION(atom == GetAtom(id), "can't map id back to atom");
     }

     // Make sure we don't find things that aren't there
     id = LookupTag(NS_LITERAL_STRING("@"));
     NS_ASSERTION(id == eHTMLTag_userdefined, "found @");
     id = LookupTag(NS_LITERAL_STRING("zzzzz"));
     NS_ASSERTION(id == eHTMLTag_userdefined, "found zzzzz");

     atom = do_GetAtom("@");
     id = CaseSensitiveLookupTag(atom);
     NS_ASSERTION(id == eHTMLTag_userdefined, "found @");
     atom = do_GetAtom("zzzzz");
     id = CaseSensitiveLookupTag(atom);
     NS_ASSERTION(id == eHTMLTag_userdefined, "found zzzzz");

     tag = GetStringValue((nsHTMLTag) 0);
     NS_ASSERTION(!tag, "found enum 0");
     tag = GetStringValue((nsHTMLTag) -1);
     NS_ASSERTION(!tag, "found enum -1");
     tag = GetStringValue((nsHTMLTag) (NS_HTML_TAG_MAX + 1));
     NS_ASSERTION(!tag, "found past max enum");

     atom = GetAtom((nsHTMLTag) 0);
     NS_ASSERTION(!atom, "found enum 0");
     atom = GetAtom((nsHTMLTag) -1);
     NS_ASSERTION(!atom, "found enum -1");
     atom = GetAtom((nsHTMLTag) (NS_HTML_TAG_MAX + 1));
     NS_ASSERTION(!atom, "found past max enum");

     ReleaseTable();
}

#endif // DEBUG
