/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "mozilla/ModuleUtils.h"
#include "nsICharsetConverterManager.h"
#include "nsEncoderDecoderUtils.h"
#include "nsIUnicodeDecoder.h"
#include "nsIUnicodeEncoder.h"

#include "nsUConvCID.h"
#include "nsCharsetConverterManager.h"
#include "nsTextToSubURI.h"
#include "nsUTF8ConverterService.h"
#include "nsConverterInputStream.h"
#include "nsConverterOutputStream.h"
#include "nsScriptableUConv.h"
#include "nsIOutputStream.h"
#include "nsITextToSubURI.h"

#include "nsISO88591ToUnicode.h"
#include "nsCP1252ToUnicode.h"
#include "nsMacRomanToUnicode.h"
#include "nsReplacementToUnicode.h"
#include "nsUTF8ToUnicode.h"
#include "nsUnicodeToISO88591.h"
#include "nsUnicodeToCP1252.h"
#include "nsUnicodeToMacRoman.h"
#include "nsUnicodeToUTF8.h"

// ucvlatin
#include "nsUCvLatinCID.h"
#include "nsAsciiToUnicode.h"
#include "nsISO88592ToUnicode.h"
#include "nsISO88593ToUnicode.h"
#include "nsISO88594ToUnicode.h"
#include "nsISO88595ToUnicode.h"
#include "nsISO88596ToUnicode.h"
#include "nsISO88596EToUnicode.h"
#include "nsISO88596IToUnicode.h"
#include "nsISO88597ToUnicode.h"
#include "nsISO88598ToUnicode.h"
#include "nsISO88598EToUnicode.h"
#include "nsISO88598IToUnicode.h"
#include "nsISO88599ToUnicode.h"
#include "nsISO885910ToUnicode.h"
#include "nsISO885913ToUnicode.h"
#include "nsISO885914ToUnicode.h"
#include "nsISO885915ToUnicode.h"
#include "nsISO885916ToUnicode.h"
#include "nsISOIR111ToUnicode.h"
#include "nsCP1250ToUnicode.h"
#include "nsCP1251ToUnicode.h"
#include "nsCP1253ToUnicode.h"
#include "nsCP1254ToUnicode.h"
#include "nsCP1255ToUnicode.h"
#include "nsCP1256ToUnicode.h"
#include "nsCP1257ToUnicode.h"
#include "nsCP1258ToUnicode.h"
#include "nsCP874ToUnicode.h"
#include "nsISO885911ToUnicode.h"
#include "nsTIS620ToUnicode.h"
#include "nsCP866ToUnicode.h"
#include "nsKOI8RToUnicode.h"
#include "nsKOI8UToUnicode.h"
#include "nsMacCEToUnicode.h"
#include "nsMacGreekToUnicode.h"
#include "nsMacTurkishToUnicode.h"
#include "nsMacCroatianToUnicode.h"
#include "nsMacRomanianToUnicode.h"
#include "nsMacCyrillicToUnicode.h"
#include "nsMacIcelandicToUnicode.h"
#include "nsARMSCII8ToUnicode.h"
#include "nsTCVN5712ToUnicode.h"
#include "nsVISCIIToUnicode.h"
#include "nsVPSToUnicode.h"
#include "nsUTF7ToUnicode.h"
#include "nsMUTF7ToUnicode.h"
#include "nsUTF16ToUnicode.h"
#include "nsT61ToUnicode.h"
#include "nsUserDefinedToUnicode.h"
#include "nsUnicodeToAscii.h"
#include "nsUnicodeToISO88592.h"
#include "nsUnicodeToISO88593.h"
#include "nsUnicodeToISO88594.h"
#include "nsUnicodeToISO88595.h"
#include "nsUnicodeToISO88596.h"
#include "nsUnicodeToISO88596E.h"
#include "nsUnicodeToISO88596I.h"
#include "nsUnicodeToISO88597.h"
#include "nsUnicodeToISO88598.h"
#include "nsUnicodeToISO88598E.h"
#include "nsUnicodeToISO88598I.h"
#include "nsUnicodeToISO88599.h"
#include "nsUnicodeToISO885910.h"
#include "nsUnicodeToISO885913.h"
#include "nsUnicodeToISO885914.h"
#include "nsUnicodeToISO885915.h"
#include "nsUnicodeToISO885916.h"
#include "nsUnicodeToISOIR111.h"
#include "nsUnicodeToCP1250.h"
#include "nsUnicodeToCP1251.h"
#include "nsUnicodeToCP1253.h"
#include "nsUnicodeToCP1254.h"
#include "nsUnicodeToCP1255.h"
#include "nsUnicodeToCP1256.h"
#include "nsUnicodeToCP1257.h"
#include "nsUnicodeToCP1258.h"
#include "nsUnicodeToCP874.h"
#include "nsUnicodeToISO885911.h"
#include "nsUnicodeToTIS620.h"
#include "nsUnicodeToCP866.h"
#include "nsUnicodeToKOI8R.h"
#include "nsUnicodeToKOI8U.h"
#include "nsUnicodeToMacCE.h"
#include "nsUnicodeToMacGreek.h"
#include "nsUnicodeToMacTurkish.h"
#include "nsUnicodeToMacCroatian.h"
#include "nsUnicodeToMacRomanian.h"
#include "nsUnicodeToMacCyrillic.h"
#include "nsUnicodeToMacIcelandic.h"
#include "nsUnicodeToARMSCII8.h"
#include "nsUnicodeToTCVN5712.h"
#include "nsUnicodeToVISCII.h"
#include "nsUnicodeToVPS.h"
#include "nsUnicodeToUTF7.h"
#include "nsUnicodeToMUTF7.h"
#include "nsUnicodeToUTF16.h"
#include "nsUnicodeToT61.h"
#include "nsUnicodeToUserDefined.h"
#include "nsMacArabicToUnicode.h"
#include "nsMacDevanagariToUnicode.h"
#include "nsMacFarsiToUnicode.h"
#include "nsMacGujaratiToUnicode.h"
#include "nsMacGurmukhiToUnicode.h"
#include "nsMacHebrewToUnicode.h"
#include "nsUnicodeToMacArabic.h"
#include "nsUnicodeToMacDevanagari.h"
#include "nsUnicodeToMacFarsi.h"
#include "nsUnicodeToMacGujarati.h"
#include "nsUnicodeToMacGurmukhi.h"
#include "nsUnicodeToMacHebrew.h"

// ucvibm
#include "nsUCvIBMCID.h"
#include "nsCP850ToUnicode.h"
#include "nsCP852ToUnicode.h"
#include "nsCP855ToUnicode.h"
#include "nsCP857ToUnicode.h"
#include "nsCP862ToUnicode.h"
#include "nsCP864ToUnicode.h"
#ifdef XP_OS2
#include "nsCP869ToUnicode.h"
#include "nsCP1125ToUnicode.h"
#include "nsCP1131ToUnicode.h"
#endif
#include "nsUnicodeToCP850.h"
#include "nsUnicodeToCP852.h"
#include "nsUnicodeToCP855.h"
#include "nsUnicodeToCP857.h"
#include "nsUnicodeToCP862.h"
#include "nsUnicodeToCP864.h"
#ifdef XP_OS2
#include "nsUnicodeToCP869.h"
#include "nsUnicodeToCP1125.h"
#include "nsUnicodeToCP1131.h"
#endif

// ucvja
#include "nsUCVJACID.h"
#include "nsUCVJA2CID.h"
#include "nsUCVJADll.h"
#include "nsJapaneseToUnicode.h"
#include "nsUnicodeToSJIS.h"
#include "nsUnicodeToEUCJP.h"
#include "nsUnicodeToISO2022JP.h"
#include "nsUnicodeToJISx0201.h"

// ucvtw2
#include "nsUCvTW2CID.h"
#include "nsUCvTW2Dll.h"
#include "nsEUCTWToUnicode.h"
#include "nsUnicodeToEUCTW.h"

// ucvtw
#include "nsUCvTWCID.h"
#include "nsUCvTWDll.h"
#include "nsBIG5ToUnicode.h"
#include "nsUnicodeToBIG5.h"
#include "nsBIG5HKSCSToUnicode.h"
#include "nsUnicodeToBIG5HKSCS.h"
#include "nsUnicodeToHKSCS.h"

// ucvko
#include "nsUCvKOCID.h"
#include "nsUCvKODll.h"
#include "nsJohabToUnicode.h"
#include "nsUnicodeToJohab.h"
#include "nsCP949ToUnicode.h"
#include "nsUnicodeToCP949.h"
#include "nsISO2022KRToUnicode.h"

// ucvcn
#include "nsUCvCnCID.h"
#include "nsHZToUnicode.h"
#include "nsUnicodeToHZ.h"
#include "nsGBKToUnicode.h"
#include "nsUnicodeToGBK.h"
#include "nsGB2312ToUnicodeV2.h"
#include "nsUnicodeToGB2312V2.h"
#include "nsISO2022CNToUnicode.h"
#include "gbku.h"

NS_CONVERTER_REGISTRY_START
NS_UCONV_REG_UNREG("ISO-8859-1", NS_ISO88591TOUNICODE_CID, NS_UNICODETOISO88591_CID)
NS_UCONV_REG_UNREG("windows-1252", NS_CP1252TOUNICODE_CID, NS_UNICODETOCP1252_CID)
NS_UCONV_REG_UNREG("macintosh", NS_MACROMANTOUNICODE_CID, NS_UNICODETOMACROMAN_CID)
NS_UCONV_REG_UNREG("UTF-8", NS_UTF8TOUNICODE_CID, NS_UNICODETOUTF8_CID)
NS_UCONV_REG_UNREG("replacement", NS_REPLACEMENTTOUNICODE_CID, NS_UNICODETOUTF8_CID)

  // ucvlatin
NS_UCONV_REG_UNREG("us-ascii", NS_ASCIITOUNICODE_CID, NS_UNICODETOASCII_CID)
NS_UCONV_REG_UNREG("ISO-8859-2", NS_ISO88592TOUNICODE_CID, NS_UNICODETOISO88592_CID)
NS_UCONV_REG_UNREG("ISO-8859-3", NS_ISO88593TOUNICODE_CID, NS_UNICODETOISO88593_CID)
NS_UCONV_REG_UNREG("ISO-8859-4", NS_ISO88594TOUNICODE_CID, NS_UNICODETOISO88594_CID)
NS_UCONV_REG_UNREG("ISO-8859-5", NS_ISO88595TOUNICODE_CID, NS_UNICODETOISO88595_CID)
NS_UCONV_REG_UNREG("ISO-8859-6", NS_ISO88596TOUNICODE_CID, NS_UNICODETOISO88596_CID)
NS_UCONV_REG_UNREG("ISO-8859-6-I", NS_ISO88596ITOUNICODE_CID, NS_UNICODETOISO88596I_CID)
NS_UCONV_REG_UNREG("ISO-8859-6-E", NS_ISO88596ETOUNICODE_CID, NS_UNICODETOISO88596E_CID)
NS_UCONV_REG_UNREG("ISO-8859-7", NS_ISO88597TOUNICODE_CID, NS_UNICODETOISO88597_CID)
NS_UCONV_REG_UNREG("ISO-8859-8", NS_ISO88598TOUNICODE_CID, NS_UNICODETOISO88598_CID)
NS_UCONV_REG_UNREG("ISO-8859-8-I", NS_ISO88598ITOUNICODE_CID, NS_UNICODETOISO88598I_CID)
NS_UCONV_REG_UNREG("ISO-8859-8-E", NS_ISO88598ETOUNICODE_CID, NS_UNICODETOISO88598E_CID)
NS_UCONV_REG_UNREG("ISO-8859-9", NS_ISO88599TOUNICODE_CID, NS_UNICODETOISO88599_CID)
NS_UCONV_REG_UNREG("ISO-8859-10", NS_ISO885910TOUNICODE_CID, NS_UNICODETOISO885910_CID)
NS_UCONV_REG_UNREG("ISO-8859-13", NS_ISO885913TOUNICODE_CID, NS_UNICODETOISO885913_CID)
NS_UCONV_REG_UNREG("ISO-8859-14", NS_ISO885914TOUNICODE_CID, NS_UNICODETOISO885914_CID)
NS_UCONV_REG_UNREG("ISO-8859-15", NS_ISO885915TOUNICODE_CID, NS_UNICODETOISO885915_CID)
NS_UCONV_REG_UNREG("ISO-8859-16", NS_ISO885916TOUNICODE_CID, NS_UNICODETOISO885916_CID)
NS_UCONV_REG_UNREG("ISO-IR-111", NS_ISOIR111TOUNICODE_CID, NS_UNICODETOISOIR111_CID)
NS_UCONV_REG_UNREG("windows-1250", NS_CP1250TOUNICODE_CID, NS_UNICODETOCP1250_CID)
NS_UCONV_REG_UNREG("windows-1251", NS_CP1251TOUNICODE_CID, NS_UNICODETOCP1251_CID)
NS_UCONV_REG_UNREG("windows-1253", NS_CP1253TOUNICODE_CID, NS_UNICODETOCP1253_CID)
NS_UCONV_REG_UNREG("windows-1254", NS_CP1254TOUNICODE_CID, NS_UNICODETOCP1254_CID)
NS_UCONV_REG_UNREG("windows-1255", NS_CP1255TOUNICODE_CID, NS_UNICODETOCP1255_CID)
NS_UCONV_REG_UNREG("windows-1256", NS_CP1256TOUNICODE_CID, NS_UNICODETOCP1256_CID)
NS_UCONV_REG_UNREG("windows-1257", NS_CP1257TOUNICODE_CID, NS_UNICODETOCP1257_CID)
NS_UCONV_REG_UNREG("windows-1258", NS_CP1258TOUNICODE_CID, NS_UNICODETOCP1258_CID)
NS_UCONV_REG_UNREG("TIS-620", NS_TIS620TOUNICODE_CID, NS_UNICODETOTIS620_CID)
NS_UCONV_REG_UNREG("windows-874", NS_CP874TOUNICODE_CID, NS_UNICODETOCP874_CID)
NS_UCONV_REG_UNREG("ISO-8859-11", NS_ISO885911TOUNICODE_CID, NS_UNICODETOISO885911_CID)
NS_UCONV_REG_UNREG("IBM866", NS_CP866TOUNICODE_CID, NS_UNICODETOCP866_CID)
NS_UCONV_REG_UNREG("KOI8-R", NS_KOI8RTOUNICODE_CID, NS_UNICODETOKOI8R_CID)
NS_UCONV_REG_UNREG("KOI8-U", NS_KOI8UTOUNICODE_CID, NS_UNICODETOKOI8U_CID)
NS_UCONV_REG_UNREG("x-mac-ce", NS_MACCETOUNICODE_CID, NS_UNICODETOMACCE_CID)
NS_UCONV_REG_UNREG("x-mac-greek", NS_MACGREEKTOUNICODE_CID, NS_UNICODETOMACGREEK_CID)
NS_UCONV_REG_UNREG("x-mac-turkish", NS_MACTURKISHTOUNICODE_CID, NS_UNICODETOMACTURKISH_CID)
NS_UCONV_REG_UNREG("x-mac-croatian", NS_MACCROATIANTOUNICODE_CID, NS_UNICODETOMACCROATIAN_CID)
NS_UCONV_REG_UNREG("x-mac-romanian", NS_MACROMANIANTOUNICODE_CID, NS_UNICODETOMACROMANIAN_CID)
NS_UCONV_REG_UNREG("x-mac-cyrillic", NS_MACCYRILLICTOUNICODE_CID, NS_UNICODETOMACCYRILLIC_CID)
NS_UCONV_REG_UNREG("x-mac-icelandic", NS_MACICELANDICTOUNICODE_CID, NS_UNICODETOMACICELANDIC_CID)
NS_UCONV_REG_UNREG("armscii-8", NS_ARMSCII8TOUNICODE_CID, NS_UNICODETOARMSCII8_CID)
NS_UCONV_REG_UNREG("x-viet-tcvn5712", NS_TCVN5712TOUNICODE_CID, NS_UNICODETOTCVN5712_CID)
NS_UCONV_REG_UNREG("VISCII", NS_VISCIITOUNICODE_CID, NS_UNICODETOVISCII_CID)
NS_UCONV_REG_UNREG("x-viet-vps", NS_VPSTOUNICODE_CID, NS_UNICODETOVPS_CID)
NS_UCONV_REG_UNREG("UTF-7", NS_UTF7TOUNICODE_CID, NS_UNICODETOUTF7_CID)
NS_UCONV_REG_UNREG("x-imap4-modified-utf7", NS_MUTF7TOUNICODE_CID, NS_UNICODETOMUTF7_CID)
NS_UCONV_REG_UNREG("UTF-16", NS_UTF16TOUNICODE_CID, NS_UNICODETOUTF16_CID)
NS_UCONV_REG_UNREG("UTF-16BE", NS_UTF16BETOUNICODE_CID, NS_UNICODETOUTF16BE_CID)
NS_UCONV_REG_UNREG("UTF-16LE", NS_UTF16LETOUNICODE_CID, NS_UNICODETOUTF16LE_CID)
NS_UCONV_REG_UNREG("T.61-8bit", NS_T61TOUNICODE_CID, NS_UNICODETOT61_CID)
NS_UCONV_REG_UNREG("x-user-defined", NS_USERDEFINEDTOUNICODE_CID, NS_UNICODETOUSERDEFINED_CID)
NS_UCONV_REG_UNREG("x-mac-arabic" , NS_MACARABICTOUNICODE_CID, NS_UNICODETOMACARABIC_CID)
NS_UCONV_REG_UNREG("x-mac-devanagari" , NS_MACDEVANAGARITOUNICODE_CID, NS_UNICODETOMACDEVANAGARI_CID)
NS_UCONV_REG_UNREG("x-mac-farsi" , NS_MACFARSITOUNICODE_CID, NS_UNICODETOMACFARSI_CID)
NS_UCONV_REG_UNREG("x-mac-gurmukhi" , NS_MACGURMUKHITOUNICODE_CID, NS_UNICODETOMACGURMUKHI_CID)
NS_UCONV_REG_UNREG("x-mac-gujarati" , NS_MACGUJARATITOUNICODE_CID, NS_UNICODETOMACGUJARATI_CID)
NS_UCONV_REG_UNREG("x-mac-hebrew" , NS_MACHEBREWTOUNICODE_CID, NS_UNICODETOMACHEBREW_CID)

  // ucvibm
NS_UCONV_REG_UNREG("IBM850", NS_CP850TOUNICODE_CID, NS_UNICODETOCP850_CID)
NS_UCONV_REG_UNREG("IBM852", NS_CP852TOUNICODE_CID, NS_UNICODETOCP852_CID)
NS_UCONV_REG_UNREG("IBM855", NS_CP855TOUNICODE_CID, NS_UNICODETOCP855_CID)
NS_UCONV_REG_UNREG("IBM857", NS_CP857TOUNICODE_CID, NS_UNICODETOCP857_CID)
NS_UCONV_REG_UNREG("IBM862", NS_CP862TOUNICODE_CID, NS_UNICODETOCP862_CID)
NS_UCONV_REG_UNREG("IBM864", NS_CP864TOUNICODE_CID, NS_UNICODETOCP864_CID)
#ifdef XP_OS2
NS_UCONV_REG_UNREG("IBM869", NS_CP869TOUNICODE_CID, NS_UNICODETOCP869_CID)
NS_UCONV_REG_UNREG("IBM1125", NS_CP1125TOUNICODE_CID, NS_UNICODETOCP1125_CID)
NS_UCONV_REG_UNREG("IBM1131", NS_CP1131TOUNICODE_CID, NS_UNICODETOCP1131_CID)
#endif

    // ucvja
NS_UCONV_REG_UNREG("Shift_JIS", NS_SJISTOUNICODE_CID, NS_UNICODETOSJIS_CID)
NS_UCONV_REG_UNREG("ISO-2022-JP", NS_ISO2022JPTOUNICODE_CID, NS_UNICODETOISO2022JP_CID)
NS_UCONV_REG_UNREG("EUC-JP", NS_EUCJPTOUNICODE_CID, NS_UNICODETOEUCJP_CID)
  
NS_UCONV_REG_UNREG_ENCODER("jis_0201" , NS_UNICODETOJISX0201_CID)

    // ucvtw2
NS_UCONV_REG_UNREG("x-euc-tw", NS_EUCTWTOUNICODE_CID, NS_UNICODETOEUCTW_CID)

    // ucvtw
NS_UCONV_REG_UNREG("Big5", NS_BIG5TOUNICODE_CID, NS_UNICODETOBIG5_CID)
NS_UCONV_REG_UNREG("Big5-HKSCS", NS_BIG5HKSCSTOUNICODE_CID, NS_UNICODETOBIG5HKSCS_CID)
  
NS_UCONV_REG_UNREG_ENCODER("hkscs-1" , NS_UNICODETOHKSCS_CID)

    // ucvko
NS_UCONV_REG_UNREG("EUC-KR", NS_EUCKRTOUNICODE_CID, NS_UNICODETOEUCKR_CID)
NS_UCONV_REG_UNREG("x-johab", NS_JOHABTOUNICODE_CID, NS_UNICODETOJOHAB_CID)
NS_UCONV_REG_UNREG_DECODER("ISO-2022-KR", NS_ISO2022KRTOUNICODE_CID)

// ucvcn
NS_UCONV_REG_UNREG("GB2312", NS_GB2312TOUNICODE_CID, NS_UNICODETOGB2312_CID)
NS_UCONV_REG_UNREG("gbk", NS_GBKTOUNICODE_CID, NS_UNICODETOGBK_CID)
NS_UCONV_REG_UNREG("HZ-GB-2312", NS_HZTOUNICODE_CID, NS_UNICODETOHZ_CID)
NS_UCONV_REG_UNREG("gb18030", NS_GB18030TOUNICODE_CID, NS_UNICODETOGB18030_CID)
NS_UCONV_REG_UNREG_DECODER("ISO-2022-CN", NS_ISO2022CNTOUNICODE_CID)

{ NS_TITLE_BUNDLE_CATEGORY, "chrome://global/locale/charsetTitles.properties", "" },
{ NS_DATA_BUNDLE_CATEGORY, "resource://gre-resources/charsetData.properties", "" },
  
NS_CONVERTER_REGISTRY_END

NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToUTF8)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUTF8ToUnicode)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsReplacementToUnicode)

// ucvlatin
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUTF7ToUnicode)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMUTF7ToUnicode)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUTF16ToUnicode)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUTF16BEToUnicode)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUTF16LEToUnicode)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToUTF7)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToMUTF7)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToUTF16BE)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToUTF16LE)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToUTF16)

// ucvibm

// ucvja
NS_GENERIC_FACTORY_CONSTRUCTOR(nsShiftJISToUnicode)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsEUCJPToUnicodeV2)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsISO2022JPToUnicodeV2)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToISO2022JP)

// ucvtw2

// ucvtw

// ucvko
NS_GENERIC_FACTORY_CONSTRUCTOR(nsISO2022KRToUnicode)

// ucvcn
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToGB2312V2)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToGBK)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsHZToUnicode)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToHZ)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsGB18030ToUnicode)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUnicodeToGB18030)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsISO2022CNToUnicode)


//----------------------------------------------------------------------------
// Global functions and data [declaration]

// ucvja
const uint16_t g_uf0201Mapping[] = {
#include "jis0201.uf"
};

const uint16_t g_uf0201GLMapping[] = {
#include "jis0201gl.uf"
};

const uint16_t g_uf0208Mapping[] = {
#include "jis0208.uf"
};

const uint16_t g_uf0208extMapping[] = {
#include "jis0208ext.uf"
};

// ucvtw2
const uint16_t g_ufCNS1MappingTable[] = {
#include "cns_1.uf"
};

const uint16_t g_ufCNS2MappingTable[] = {
#include "cns_2.uf"
};

const uint16_t g_ufCNS3MappingTable[] = {
#include "cns3.uf"
};

const uint16_t g_ufCNS4MappingTable[] = {
#include "cns4.uf"
};

const uint16_t g_ufCNS5MappingTable[] = {
#include "cns5.uf"
};

const uint16_t g_ufCNS6MappingTable[] = {
#include "cns6.uf"
};

const uint16_t g_ufCNS7MappingTable[] = {
#include "cns7.uf"
};

const uint16_t g_utCNS1MappingTable[] = {
#include "cns_1.ut"
};

const uint16_t g_utCNS2MappingTable[] = {
#include "cns_2.ut"
};

const uint16_t g_utCNS3MappingTable[] = {
#include "cns3.ut"
};

const uint16_t g_utCNS4MappingTable[] = {
#include "cns4.ut"
};

const uint16_t g_utCNS5MappingTable[] = {
#include "cns5.ut"
};

const uint16_t g_utCNS6MappingTable[] = {
#include "cns6.ut"
};

const uint16_t g_utCNS7MappingTable[] = {
#include "cns7.ut"
};

const uint16_t g_ASCIIMappingTable[] = {
  0x0001, 0x0004, 0x0005, 0x0008, 0x0000, 0x0000, 0x007F, 0x0000
};

// ucvtw
const uint16_t g_ufBig5Mapping[] = {
#include "big5.uf"
};

const uint16_t g_utBIG5Mapping[] = {
#include "big5.ut"
};

const uint16_t g_ufBig5HKSCSMapping[] = {
#include "hkscs.uf"
};

const uint16_t g_utBig5HKSCSMapping[] = {
#include "hkscs.ut"
};

// ucvko
const uint16_t g_utKSC5601Mapping[] = {
#include "u20kscgl.ut"
};

const uint16_t g_ufKSC5601Mapping[] = {
#include "u20kscgl.uf"
};

const uint16_t g_HangulNullMapping[] ={
  0x0001, 0x0004, 0x0005, 0x0008, 0x0000, 0xAC00, 0xD7A3, 0xAC00
};

const uint16_t g_ufJohabJamoMapping[] ={   
#include "johabjamo.uf"
};

NS_GENERIC_FACTORY_CONSTRUCTOR(nsCharsetConverterManager)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsTextToSubURI)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUTF8ConverterService)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsConverterInputStream)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsConverterOutputStream)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsScriptableUnicodeConverter)

NS_DEFINE_NAMED_CID(NS_ICHARSETCONVERTERMANAGER_CID);
NS_DEFINE_NAMED_CID(NS_TEXTTOSUBURI_CID);
NS_DEFINE_NAMED_CID(NS_CONVERTERINPUTSTREAM_CID);
NS_DEFINE_NAMED_CID(NS_CONVERTEROUTPUTSTREAM_CID);
NS_DEFINE_NAMED_CID(NS_ISCRIPTABLEUNICODECONVERTER_CID);
NS_DEFINE_NAMED_CID(NS_UTF8CONVERTERSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_ISO88591TOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_CP1252TOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_MACROMANTOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_UTF8TOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_REPLACEMENTTOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOISO88591_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOCP1252_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOMACROMAN_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOUTF8_CID);
NS_DEFINE_NAMED_CID(NS_ASCIITOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_ISO88592TOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_ISO88593TOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_ISO88594TOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_ISO88595TOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_ISO88596TOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_ISO88596ITOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_ISO88596ETOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_ISO88597TOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_ISO88598TOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_ISO88598ITOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_ISO88598ETOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_ISO88599TOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_ISO885910TOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_ISO885913TOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_ISO885914TOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_ISO885915TOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_ISO885916TOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_ISOIR111TOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_CP1250TOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_CP1251TOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_CP1253TOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_CP1254TOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_CP1255TOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_CP1256TOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_CP1257TOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_CP1258TOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_TIS620TOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_ISO885911TOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_CP874TOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_CP866TOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_KOI8RTOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_KOI8UTOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_MACCETOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_MACGREEKTOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_MACTURKISHTOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_MACCROATIANTOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_MACROMANIANTOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_MACCYRILLICTOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_MACICELANDICTOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_ARMSCII8TOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_TCVN5712TOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_VISCIITOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_VPSTOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_UTF7TOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_MUTF7TOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_UTF16TOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_UTF16BETOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_UTF16LETOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_T61TOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_USERDEFINEDTOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_MACARABICTOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_MACDEVANAGARITOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_MACFARSITOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_MACGURMUKHITOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_MACGUJARATITOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_MACHEBREWTOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOASCII_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOISO88592_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOISO88593_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOISO88594_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOISO88595_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOISO88596_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOISO88596I_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOISO88596E_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOISO88597_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOISO88598_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOISO88598I_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOISO88598E_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOISO88599_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOISO885910_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOISO885913_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOISO885914_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOISO885915_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOISO885916_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOISOIR111_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOCP1250_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOCP1251_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOCP1253_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOCP1254_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOCP1255_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOCP1256_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOCP1257_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOCP1258_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOTIS620_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOISO885911_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOCP874_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOCP866_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOKOI8R_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOKOI8U_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOMACCE_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOMACGREEK_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOMACTURKISH_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOMACCROATIAN_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOMACROMANIAN_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOMACCYRILLIC_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOMACICELANDIC_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOARMSCII8_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOTCVN5712_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOVISCII_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOVPS_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOUTF7_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOMUTF7_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOUTF16BE_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOUTF16LE_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOUTF16_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOT61_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOUSERDEFINED_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOMACARABIC_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOMACDEVANAGARI_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOMACFARSI_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOMACGURMUKHI_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOMACGUJARATI_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOMACHEBREW_CID);
NS_DEFINE_NAMED_CID(NS_CP850TOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_CP852TOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_CP855TOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_CP857TOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_CP862TOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_CP864TOUNICODE_CID);
#ifdef XP_OS2
NS_DEFINE_NAMED_CID(NS_CP869TOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_CP1125TOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_CP1131TOUNICODE_CID);
#endif
NS_DEFINE_NAMED_CID(NS_UNICODETOCP850_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOCP852_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOCP855_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOCP857_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOCP862_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOCP864_CID);
#ifdef XP_OS2
NS_DEFINE_NAMED_CID(NS_UNICODETOCP869_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOCP1125_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOCP1131_CID);
#endif
NS_DEFINE_NAMED_CID(NS_SJISTOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_EUCJPTOUNICODE_CID);
NS_DEFINE_NAMED_CID( NS_ISO2022JPTOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOSJIS_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOEUCJP_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOISO2022JP_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOJISX0201_CID);
NS_DEFINE_NAMED_CID(NS_EUCTWTOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOEUCTW_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOBIG5_CID);
NS_DEFINE_NAMED_CID(NS_BIG5TOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOBIG5HKSCS_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOHKSCS_CID);
NS_DEFINE_NAMED_CID(NS_BIG5HKSCSTOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_EUCKRTOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOEUCKR_CID);
NS_DEFINE_NAMED_CID(NS_JOHABTOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOJOHAB_CID);
NS_DEFINE_NAMED_CID(NS_ISO2022KRTOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_GB2312TOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOGB2312_CID);
NS_DEFINE_NAMED_CID(NS_GBKTOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOGBK_CID);
NS_DEFINE_NAMED_CID(NS_HZTOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOHZ_CID);
NS_DEFINE_NAMED_CID(NS_GB18030TOUNICODE_CID);
NS_DEFINE_NAMED_CID(NS_UNICODETOGB18030_CID);
NS_DEFINE_NAMED_CID(NS_ISO2022CNTOUNICODE_CID);

static const mozilla::Module::CIDEntry kUConvCIDs[] = {
  { &kNS_ICHARSETCONVERTERMANAGER_CID, false, nullptr, nsCharsetConverterManagerConstructor },
  { &kNS_TEXTTOSUBURI_CID, false, nullptr, nsTextToSubURIConstructor },
  { &kNS_CONVERTERINPUTSTREAM_CID, false, nullptr, nsConverterInputStreamConstructor },
  { &kNS_CONVERTEROUTPUTSTREAM_CID, false, nullptr, nsConverterOutputStreamConstructor },
  { &kNS_ISCRIPTABLEUNICODECONVERTER_CID, false, nullptr, nsScriptableUnicodeConverterConstructor },
  { &kNS_UTF8CONVERTERSERVICE_CID, false, nullptr, nsUTF8ConverterServiceConstructor },
  { &kNS_ISO88591TOUNICODE_CID, false, nullptr, nsISO88591ToUnicodeConstructor },
  { &kNS_CP1252TOUNICODE_CID, false, nullptr, nsCP1252ToUnicodeConstructor },
  { &kNS_MACROMANTOUNICODE_CID, false, nullptr, nsMacRomanToUnicodeConstructor },
  { &kNS_REPLACEMENTTOUNICODE_CID, false, nullptr, nsReplacementToUnicodeConstructor },
  { &kNS_UTF8TOUNICODE_CID, false, nullptr, nsUTF8ToUnicodeConstructor },
  { &kNS_UNICODETOISO88591_CID, false, nullptr, nsUnicodeToISO88591Constructor },
  { &kNS_UNICODETOCP1252_CID, false, nullptr, nsUnicodeToCP1252Constructor },
  { &kNS_UNICODETOMACROMAN_CID, false, nullptr, nsUnicodeToMacRomanConstructor },
  { &kNS_UNICODETOUTF8_CID, false, nullptr, nsUnicodeToUTF8Constructor },
  { &kNS_ASCIITOUNICODE_CID, false, nullptr, nsAsciiToUnicodeConstructor },
  { &kNS_ISO88592TOUNICODE_CID, false, nullptr, nsISO88592ToUnicodeConstructor },
  { &kNS_ISO88593TOUNICODE_CID, false, nullptr, nsISO88593ToUnicodeConstructor },
  { &kNS_ISO88594TOUNICODE_CID, false, nullptr, nsISO88594ToUnicodeConstructor },
  { &kNS_ISO88595TOUNICODE_CID, false, nullptr, nsISO88595ToUnicodeConstructor },
  { &kNS_ISO88596TOUNICODE_CID, false, nullptr, nsISO88596ToUnicodeConstructor },
  { &kNS_ISO88596ITOUNICODE_CID, false, nullptr, nsISO88596IToUnicodeConstructor },
  { &kNS_ISO88596ETOUNICODE_CID, false, nullptr, nsISO88596EToUnicodeConstructor },
  { &kNS_ISO88597TOUNICODE_CID, false, nullptr, nsISO88597ToUnicodeConstructor },
  { &kNS_ISO88598TOUNICODE_CID, false, nullptr, nsISO88598ToUnicodeConstructor },
  { &kNS_ISO88598ITOUNICODE_CID, false, nullptr, nsISO88598IToUnicodeConstructor },
  { &kNS_ISO88598ETOUNICODE_CID, false, nullptr, nsISO88598EToUnicodeConstructor },
  { &kNS_ISO88599TOUNICODE_CID, false, nullptr, nsISO88599ToUnicodeConstructor },
  { &kNS_ISO885910TOUNICODE_CID, false, nullptr, nsISO885910ToUnicodeConstructor },
  { &kNS_ISO885913TOUNICODE_CID, false, nullptr, nsISO885913ToUnicodeConstructor },
  { &kNS_ISO885914TOUNICODE_CID, false, nullptr, nsISO885914ToUnicodeConstructor },
  { &kNS_ISO885915TOUNICODE_CID, false, nullptr, nsISO885915ToUnicodeConstructor },
  { &kNS_ISO885916TOUNICODE_CID, false, nullptr, nsISO885916ToUnicodeConstructor },
  { &kNS_ISOIR111TOUNICODE_CID, false, nullptr, nsISOIR111ToUnicodeConstructor },
  { &kNS_CP1250TOUNICODE_CID, false, nullptr, nsCP1250ToUnicodeConstructor },
  { &kNS_CP1251TOUNICODE_CID, false, nullptr, nsCP1251ToUnicodeConstructor },
  { &kNS_CP1253TOUNICODE_CID, false, nullptr, nsCP1253ToUnicodeConstructor },
  { &kNS_CP1254TOUNICODE_CID, false, nullptr, nsCP1254ToUnicodeConstructor },
  { &kNS_CP1255TOUNICODE_CID, false, nullptr, nsCP1255ToUnicodeConstructor },
  { &kNS_CP1256TOUNICODE_CID, false, nullptr, nsCP1256ToUnicodeConstructor },
  { &kNS_CP1257TOUNICODE_CID, false, nullptr, nsCP1257ToUnicodeConstructor },
  { &kNS_CP1258TOUNICODE_CID, false, nullptr, nsCP1258ToUnicodeConstructor },
  { &kNS_TIS620TOUNICODE_CID, false, nullptr, nsTIS620ToUnicodeConstructor },
  { &kNS_ISO885911TOUNICODE_CID, false, nullptr, nsISO885911ToUnicodeConstructor },
  { &kNS_CP874TOUNICODE_CID, false, nullptr, nsCP874ToUnicodeConstructor },
  { &kNS_CP866TOUNICODE_CID, false, nullptr, nsCP866ToUnicodeConstructor },
  { &kNS_KOI8RTOUNICODE_CID, false, nullptr, nsKOI8RToUnicodeConstructor },
  { &kNS_KOI8UTOUNICODE_CID, false, nullptr, nsKOI8UToUnicodeConstructor },
  { &kNS_MACCETOUNICODE_CID, false, nullptr, nsMacCEToUnicodeConstructor },
  { &kNS_MACGREEKTOUNICODE_CID, false, nullptr, nsMacGreekToUnicodeConstructor },
  { &kNS_MACTURKISHTOUNICODE_CID, false, nullptr, nsMacTurkishToUnicodeConstructor },
  { &kNS_MACCROATIANTOUNICODE_CID, false, nullptr, nsMacCroatianToUnicodeConstructor },
  { &kNS_MACROMANIANTOUNICODE_CID, false, nullptr, nsMacRomanianToUnicodeConstructor },
  { &kNS_MACCYRILLICTOUNICODE_CID, false, nullptr, nsMacCyrillicToUnicodeConstructor },
  { &kNS_MACICELANDICTOUNICODE_CID, false, nullptr, nsMacIcelandicToUnicodeConstructor },
  { &kNS_ARMSCII8TOUNICODE_CID, false, nullptr, nsARMSCII8ToUnicodeConstructor },
  { &kNS_TCVN5712TOUNICODE_CID, false, nullptr, nsTCVN5712ToUnicodeConstructor },
  { &kNS_VISCIITOUNICODE_CID, false, nullptr, nsVISCIIToUnicodeConstructor },
  { &kNS_VPSTOUNICODE_CID, false, nullptr, nsVPSToUnicodeConstructor },
  { &kNS_UTF7TOUNICODE_CID, false, nullptr, nsUTF7ToUnicodeConstructor },
  { &kNS_MUTF7TOUNICODE_CID, false, nullptr, nsMUTF7ToUnicodeConstructor },
  { &kNS_UTF16TOUNICODE_CID, false, nullptr, nsUTF16ToUnicodeConstructor },
  { &kNS_UTF16BETOUNICODE_CID, false, nullptr, nsUTF16BEToUnicodeConstructor },
  { &kNS_UTF16LETOUNICODE_CID, false, nullptr, nsUTF16LEToUnicodeConstructor },
  { &kNS_T61TOUNICODE_CID, false, nullptr, nsT61ToUnicodeConstructor },
  { &kNS_USERDEFINEDTOUNICODE_CID, false, nullptr, nsUserDefinedToUnicodeConstructor },
  { &kNS_MACARABICTOUNICODE_CID, false, nullptr, nsMacArabicToUnicodeConstructor },
  { &kNS_MACDEVANAGARITOUNICODE_CID, false, nullptr, nsMacDevanagariToUnicodeConstructor },
  { &kNS_MACFARSITOUNICODE_CID, false, nullptr, nsMacFarsiToUnicodeConstructor },
  { &kNS_MACGURMUKHITOUNICODE_CID, false, nullptr, nsMacGurmukhiToUnicodeConstructor },
  { &kNS_MACGUJARATITOUNICODE_CID, false, nullptr, nsMacGujaratiToUnicodeConstructor },
  { &kNS_MACHEBREWTOUNICODE_CID, false, nullptr, nsMacHebrewToUnicodeConstructor },
  { &kNS_UNICODETOASCII_CID, false, nullptr, nsUnicodeToAsciiConstructor },
  { &kNS_UNICODETOISO88592_CID, false, nullptr, nsUnicodeToISO88592Constructor },
  { &kNS_UNICODETOISO88593_CID, false, nullptr, nsUnicodeToISO88593Constructor },
  { &kNS_UNICODETOISO88594_CID, false, nullptr, nsUnicodeToISO88594Constructor },
  { &kNS_UNICODETOISO88595_CID, false, nullptr, nsUnicodeToISO88595Constructor },
  { &kNS_UNICODETOISO88596_CID, false, nullptr, nsUnicodeToISO88596Constructor },
  { &kNS_UNICODETOISO88596I_CID, false, nullptr, nsUnicodeToISO88596IConstructor },
  { &kNS_UNICODETOISO88596E_CID, false, nullptr, nsUnicodeToISO88596EConstructor },
  { &kNS_UNICODETOISO88597_CID, false, nullptr, nsUnicodeToISO88597Constructor },
  { &kNS_UNICODETOISO88598_CID, false, nullptr, nsUnicodeToISO88598Constructor },
  { &kNS_UNICODETOISO88598I_CID, false, nullptr, nsUnicodeToISO88598IConstructor },
  { &kNS_UNICODETOISO88598E_CID, false, nullptr, nsUnicodeToISO88598EConstructor },
  { &kNS_UNICODETOISO88599_CID, false, nullptr, nsUnicodeToISO88599Constructor },
  { &kNS_UNICODETOISO885910_CID, false, nullptr, nsUnicodeToISO885910Constructor },
  { &kNS_UNICODETOISO885913_CID, false, nullptr, nsUnicodeToISO885913Constructor },
  { &kNS_UNICODETOISO885914_CID, false, nullptr, nsUnicodeToISO885914Constructor },
  { &kNS_UNICODETOISO885915_CID, false, nullptr, nsUnicodeToISO885915Constructor },
  { &kNS_UNICODETOISO885916_CID, false, nullptr, nsUnicodeToISO885916Constructor },
  { &kNS_UNICODETOISOIR111_CID, false, nullptr, nsUnicodeToISOIR111Constructor },
  { &kNS_UNICODETOCP1250_CID, false, nullptr, nsUnicodeToCP1250Constructor },
  { &kNS_UNICODETOCP1251_CID, false, nullptr, nsUnicodeToCP1251Constructor },
  { &kNS_UNICODETOCP1253_CID, false, nullptr, nsUnicodeToCP1253Constructor },
  { &kNS_UNICODETOCP1254_CID, false, nullptr, nsUnicodeToCP1254Constructor },
  { &kNS_UNICODETOCP1255_CID, false, nullptr, nsUnicodeToCP1255Constructor },
  { &kNS_UNICODETOCP1256_CID, false, nullptr, nsUnicodeToCP1256Constructor },
  { &kNS_UNICODETOCP1257_CID, false, nullptr, nsUnicodeToCP1257Constructor },
  { &kNS_UNICODETOCP1258_CID, false, nullptr, nsUnicodeToCP1258Constructor },
  { &kNS_UNICODETOTIS620_CID, false, nullptr, nsUnicodeToTIS620Constructor },
  { &kNS_UNICODETOISO885911_CID, false, nullptr, nsUnicodeToISO885911Constructor },
  { &kNS_UNICODETOCP874_CID, false, nullptr, nsUnicodeToCP874Constructor },
  { &kNS_UNICODETOCP866_CID, false, nullptr, nsUnicodeToCP866Constructor },
  { &kNS_UNICODETOKOI8R_CID, false, nullptr, nsUnicodeToKOI8RConstructor },
  { &kNS_UNICODETOKOI8U_CID, false, nullptr, nsUnicodeToKOI8UConstructor },
  { &kNS_UNICODETOMACCE_CID, false, nullptr, nsUnicodeToMacCEConstructor },
  { &kNS_UNICODETOMACGREEK_CID, false, nullptr, nsUnicodeToMacGreekConstructor },
  { &kNS_UNICODETOMACTURKISH_CID, false, nullptr, nsUnicodeToMacTurkishConstructor },
  { &kNS_UNICODETOMACCROATIAN_CID, false, nullptr, nsUnicodeToMacCroatianConstructor },
  { &kNS_UNICODETOMACROMANIAN_CID, false, nullptr, nsUnicodeToMacRomanianConstructor },
  { &kNS_UNICODETOMACCYRILLIC_CID, false, nullptr, nsUnicodeToMacCyrillicConstructor },
  { &kNS_UNICODETOMACICELANDIC_CID, false, nullptr, nsUnicodeToMacIcelandicConstructor },
  { &kNS_UNICODETOARMSCII8_CID, false, nullptr, nsUnicodeToARMSCII8Constructor },
  { &kNS_UNICODETOTCVN5712_CID, false, nullptr, nsUnicodeToTCVN5712Constructor },
  { &kNS_UNICODETOVISCII_CID, false, nullptr, nsUnicodeToVISCIIConstructor },
  { &kNS_UNICODETOVPS_CID, false, nullptr, nsUnicodeToVPSConstructor },
  { &kNS_UNICODETOUTF7_CID, false, nullptr, nsUnicodeToUTF7Constructor },
  { &kNS_UNICODETOMUTF7_CID, false, nullptr, nsUnicodeToMUTF7Constructor },
  { &kNS_UNICODETOUTF16BE_CID, false, nullptr, nsUnicodeToUTF16BEConstructor },
  { &kNS_UNICODETOUTF16LE_CID, false, nullptr, nsUnicodeToUTF16LEConstructor },
  { &kNS_UNICODETOUTF16_CID, false, nullptr, nsUnicodeToUTF16Constructor },
  { &kNS_UNICODETOT61_CID, false, nullptr, nsUnicodeToT61Constructor },
  { &kNS_UNICODETOUSERDEFINED_CID, false, nullptr, nsUnicodeToUserDefinedConstructor },
  { &kNS_UNICODETOMACARABIC_CID, false, nullptr, nsUnicodeToMacArabicConstructor },
  { &kNS_UNICODETOMACDEVANAGARI_CID, false, nullptr, nsUnicodeToMacDevanagariConstructor },
  { &kNS_UNICODETOMACFARSI_CID, false, nullptr, nsUnicodeToMacFarsiConstructor },
  { &kNS_UNICODETOMACGURMUKHI_CID, false, nullptr, nsUnicodeToMacGurmukhiConstructor },
  { &kNS_UNICODETOMACGUJARATI_CID, false, nullptr, nsUnicodeToMacGujaratiConstructor },
  { &kNS_UNICODETOMACHEBREW_CID, false, nullptr, nsUnicodeToMacHebrewConstructor },
  { &kNS_CP850TOUNICODE_CID, false, nullptr, nsCP850ToUnicodeConstructor },
  { &kNS_CP852TOUNICODE_CID, false, nullptr, nsCP852ToUnicodeConstructor },
  { &kNS_CP855TOUNICODE_CID, false, nullptr, nsCP855ToUnicodeConstructor },
  { &kNS_CP857TOUNICODE_CID, false, nullptr, nsCP857ToUnicodeConstructor },
  { &kNS_CP862TOUNICODE_CID, false, nullptr, nsCP862ToUnicodeConstructor },
  { &kNS_CP864TOUNICODE_CID, false, nullptr, nsCP864ToUnicodeConstructor },
#ifdef XP_OS2
  { &kNS_CP869TOUNICODE_CID, false, nullptr, nsCP869ToUnicodeConstructor },
  { &kNS_CP1125TOUNICODE_CID, false, nullptr, nsCP1125ToUnicodeConstructor },
  { &kNS_CP1131TOUNICODE_CID, false, nullptr, nsCP1131ToUnicodeConstructor },
#endif
  { &kNS_UNICODETOCP850_CID, false, nullptr, nsUnicodeToCP850Constructor },
  { &kNS_UNICODETOCP852_CID, false, nullptr, nsUnicodeToCP852Constructor },
  { &kNS_UNICODETOCP855_CID, false, nullptr, nsUnicodeToCP855Constructor },
  { &kNS_UNICODETOCP857_CID, false, nullptr, nsUnicodeToCP857Constructor },
  { &kNS_UNICODETOCP862_CID, false, nullptr, nsUnicodeToCP862Constructor },
  { &kNS_UNICODETOCP864_CID, false, nullptr, nsUnicodeToCP864Constructor },
#ifdef XP_OS2
  { &kNS_UNICODETOCP869_CID, false, nullptr, nsUnicodeToCP869Constructor },
  { &kNS_UNICODETOCP1125_CID, false, nullptr, nsUnicodeToCP1125Constructor },
  { &kNS_UNICODETOCP1131_CID, false, nullptr, nsUnicodeToCP1131Constructor },
#endif
  { &kNS_SJISTOUNICODE_CID, false, nullptr, nsShiftJISToUnicodeConstructor },
  { &kNS_EUCJPTOUNICODE_CID, false, nullptr, nsEUCJPToUnicodeV2Constructor },
  { &kNS_ISO2022JPTOUNICODE_CID, false, nullptr, nsISO2022JPToUnicodeV2Constructor },
  { &kNS_UNICODETOSJIS_CID, false, nullptr, nsUnicodeToSJISConstructor },
  { &kNS_UNICODETOEUCJP_CID, false, nullptr, nsUnicodeToEUCJPConstructor },
  { &kNS_UNICODETOISO2022JP_CID, false, nullptr, nsUnicodeToISO2022JPConstructor },
  { &kNS_UNICODETOJISX0201_CID, false, nullptr, nsUnicodeToJISx0201Constructor },
  { &kNS_EUCTWTOUNICODE_CID, false, nullptr, nsEUCTWToUnicodeConstructor },
  { &kNS_UNICODETOEUCTW_CID, false, nullptr, nsUnicodeToEUCTWConstructor },
  { &kNS_UNICODETOBIG5_CID, false, nullptr, nsUnicodeToBIG5Constructor },
  { &kNS_BIG5TOUNICODE_CID, false, nullptr, nsBIG5ToUnicodeConstructor },
  { &kNS_UNICODETOBIG5HKSCS_CID, false, nullptr, nsUnicodeToBIG5HKSCSConstructor },
  { &kNS_UNICODETOHKSCS_CID, false, nullptr, nsUnicodeToHKSCSConstructor },
  { &kNS_BIG5HKSCSTOUNICODE_CID, false, nullptr, nsBIG5HKSCSToUnicodeConstructor },
  { &kNS_EUCKRTOUNICODE_CID, false, nullptr, nsCP949ToUnicodeConstructor },
  { &kNS_UNICODETOEUCKR_CID, false, nullptr, nsUnicodeToCP949Constructor },
  { &kNS_JOHABTOUNICODE_CID, false, nullptr, nsJohabToUnicodeConstructor },
  { &kNS_UNICODETOJOHAB_CID, false, nullptr, nsUnicodeToJohabConstructor },
  { &kNS_ISO2022KRTOUNICODE_CID, false, nullptr, nsISO2022KRToUnicodeConstructor },
  { &kNS_GB2312TOUNICODE_CID, false, nullptr, nsGB18030ToUnicodeConstructor },
  { &kNS_UNICODETOGB2312_CID, false, nullptr, nsUnicodeToGB2312V2Constructor },
  { &kNS_GBKTOUNICODE_CID, false, nullptr, nsGB18030ToUnicodeConstructor },
  { &kNS_UNICODETOGBK_CID, false, nullptr, nsUnicodeToGBKConstructor },
  { &kNS_HZTOUNICODE_CID, false, nullptr, nsHZToUnicodeConstructor },
  { &kNS_UNICODETOHZ_CID, false, nullptr, nsUnicodeToHZConstructor },
  { &kNS_GB18030TOUNICODE_CID, false, nullptr, nsGB18030ToUnicodeConstructor },
  { &kNS_UNICODETOGB18030_CID, false, nullptr, nsUnicodeToGB18030Constructor },
  { &kNS_ISO2022CNTOUNICODE_CID, false, nullptr, nsISO2022CNToUnicodeConstructor },
  { nullptr },
};

static const mozilla::Module::ContractIDEntry kUConvContracts[] = {
  { NS_CHARSETCONVERTERMANAGER_CONTRACTID, &kNS_ICHARSETCONVERTERMANAGER_CID },
  { NS_ITEXTTOSUBURI_CONTRACTID, &kNS_TEXTTOSUBURI_CID },
  { NS_CONVERTERINPUTSTREAM_CONTRACTID, &kNS_CONVERTERINPUTSTREAM_CID },
  { "@mozilla.org/intl/converter-output-stream;1", &kNS_CONVERTEROUTPUTSTREAM_CID },
  { NS_ISCRIPTABLEUNICODECONVERTER_CONTRACTID, &kNS_ISCRIPTABLEUNICODECONVERTER_CID },
  { NS_UTF8CONVERTERSERVICE_CONTRACTID, &kNS_UTF8CONVERTERSERVICE_CID },
  { NS_ISO88591TOUNICODE_CONTRACTID, &kNS_ISO88591TOUNICODE_CID },
  { NS_CP1252TOUNICODE_CONTRACTID, &kNS_CP1252TOUNICODE_CID },
  { NS_MACROMANTOUNICODE_CONTRACTID, &kNS_MACROMANTOUNICODE_CID },
  { NS_REPLACEMENTTOUNICODE_CONTRACTID, &kNS_REPLACEMENTTOUNICODE_CID },
  { NS_UTF8TOUNICODE_CONTRACTID, &kNS_UTF8TOUNICODE_CID },
  { NS_UNICODETOISO88591_CONTRACTID, &kNS_UNICODETOISO88591_CID },
  { NS_UNICODETOCP1252_CONTRACTID, &kNS_UNICODETOCP1252_CID },
  { NS_UNICODETOMACROMAN_CONTRACTID, &kNS_UNICODETOMACROMAN_CID },
  { NS_UNICODETOUTF8_CONTRACTID, &kNS_UNICODETOUTF8_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "us-ascii", &kNS_ASCIITOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "ISO-8859-2", &kNS_ISO88592TOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "ISO-8859-3", &kNS_ISO88593TOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "ISO-8859-4", &kNS_ISO88594TOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "ISO-8859-5", &kNS_ISO88595TOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "ISO-8859-6", &kNS_ISO88596TOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "ISO-8859-6-I", &kNS_ISO88596ITOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "ISO-8859-6-E", &kNS_ISO88596ETOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "ISO-8859-7", &kNS_ISO88597TOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "ISO-8859-8", &kNS_ISO88598TOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "ISO-8859-8-I", &kNS_ISO88598ITOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "ISO-8859-8-E", &kNS_ISO88598ETOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "ISO-8859-9", &kNS_ISO88599TOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "ISO-8859-10", &kNS_ISO885910TOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "ISO-8859-13", &kNS_ISO885913TOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "ISO-8859-14", &kNS_ISO885914TOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "ISO-8859-15", &kNS_ISO885915TOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "ISO-8859-16", &kNS_ISO885916TOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "ISO-IR-111", &kNS_ISOIR111TOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "windows-1250", &kNS_CP1250TOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "windows-1251", &kNS_CP1251TOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "windows-1253", &kNS_CP1253TOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "windows-1254", &kNS_CP1254TOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "windows-1255", &kNS_CP1255TOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "windows-1256", &kNS_CP1256TOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "windows-1257", &kNS_CP1257TOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "windows-1258", &kNS_CP1258TOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "TIS-620", &kNS_TIS620TOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "ISO-8859-11", &kNS_ISO885911TOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "windows-874", &kNS_CP874TOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "IBM866", &kNS_CP866TOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "KOI8-R", &kNS_KOI8RTOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "KOI8-U", &kNS_KOI8UTOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "x-mac-ce", &kNS_MACCETOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "x-mac-greek", &kNS_MACGREEKTOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "x-mac-turkish", &kNS_MACTURKISHTOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "x-mac-croatian", &kNS_MACCROATIANTOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "x-mac-romanian", &kNS_MACROMANIANTOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "x-mac-cyrillic", &kNS_MACCYRILLICTOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "x-mac-icelandic", &kNS_MACICELANDICTOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "armscii-8", &kNS_ARMSCII8TOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "x-viet-tcvn5712", &kNS_TCVN5712TOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "VISCII", &kNS_VISCIITOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "x-viet-vps", &kNS_VPSTOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "UTF-7", &kNS_UTF7TOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "x-imap4-modified-utf7", &kNS_MUTF7TOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "UTF-16", &kNS_UTF16TOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "UTF-16BE", &kNS_UTF16BETOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "UTF-16LE", &kNS_UTF16LETOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "T.61-8bit", &kNS_T61TOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "x-user-defined", &kNS_USERDEFINEDTOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "x-mac-arabic", &kNS_MACARABICTOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "x-mac-devanagari", &kNS_MACDEVANAGARITOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "x-mac-farsi", &kNS_MACFARSITOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "x-mac-gurmukhi", &kNS_MACGURMUKHITOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "x-mac-gujarati", &kNS_MACGUJARATITOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "x-mac-hebrew", &kNS_MACHEBREWTOUNICODE_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "us-ascii", &kNS_UNICODETOASCII_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "ISO-8859-2", &kNS_UNICODETOISO88592_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "ISO-8859-3", &kNS_UNICODETOISO88593_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "ISO-8859-4", &kNS_UNICODETOISO88594_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "ISO-8859-5", &kNS_UNICODETOISO88595_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "ISO-8859-6", &kNS_UNICODETOISO88596_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "ISO-8859-6-I", &kNS_UNICODETOISO88596I_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "ISO-8859-6-E", &kNS_UNICODETOISO88596E_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "ISO-8859-7", &kNS_UNICODETOISO88597_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "ISO-8859-8", &kNS_UNICODETOISO88598_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "ISO-8859-8-I", &kNS_UNICODETOISO88598I_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "ISO-8859-8-E", &kNS_UNICODETOISO88598E_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "ISO-8859-9", &kNS_UNICODETOISO88599_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "ISO-8859-10", &kNS_UNICODETOISO885910_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "ISO-8859-13", &kNS_UNICODETOISO885913_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "ISO-8859-14", &kNS_UNICODETOISO885914_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "ISO-8859-15", &kNS_UNICODETOISO885915_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "ISO-8859-16", &kNS_UNICODETOISO885916_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "ISO-IR-111", &kNS_UNICODETOISOIR111_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "windows-1250", &kNS_UNICODETOCP1250_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "windows-1251", &kNS_UNICODETOCP1251_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "windows-1253", &kNS_UNICODETOCP1253_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "windows-1254", &kNS_UNICODETOCP1254_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "windows-1255", &kNS_UNICODETOCP1255_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "windows-1256", &kNS_UNICODETOCP1256_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "windows-1257", &kNS_UNICODETOCP1257_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "windows-1258", &kNS_UNICODETOCP1258_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "TIS-620", &kNS_UNICODETOTIS620_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "ISO-8859-11", &kNS_UNICODETOISO885911_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "windows-874", &kNS_UNICODETOCP874_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "IBM866", &kNS_UNICODETOCP866_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "KOI8-R", &kNS_UNICODETOKOI8R_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "KOI8-U", &kNS_UNICODETOKOI8U_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "x-mac-ce", &kNS_UNICODETOMACCE_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "x-mac-greek", &kNS_UNICODETOMACGREEK_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "x-mac-turkish", &kNS_UNICODETOMACTURKISH_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "x-mac-croatian", &kNS_UNICODETOMACCROATIAN_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "x-mac-romanian", &kNS_UNICODETOMACROMANIAN_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "x-mac-cyrillic", &kNS_UNICODETOMACCYRILLIC_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "x-mac-icelandic", &kNS_UNICODETOMACICELANDIC_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "armscii-8", &kNS_UNICODETOARMSCII8_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "x-viet-tcvn5712", &kNS_UNICODETOTCVN5712_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "VISCII", &kNS_UNICODETOVISCII_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "x-viet-vps", &kNS_UNICODETOVPS_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "UTF-7", &kNS_UNICODETOUTF7_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "x-imap4-modified-utf7", &kNS_UNICODETOMUTF7_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "UTF-16BE", &kNS_UNICODETOUTF16BE_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "UTF-16LE", &kNS_UNICODETOUTF16LE_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "UTF-16", &kNS_UNICODETOUTF16_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "T.61-8bit", &kNS_UNICODETOT61_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "x-user-defined", &kNS_UNICODETOUSERDEFINED_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "x-mac-arabic", &kNS_UNICODETOMACARABIC_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "x-mac-devanagari", &kNS_UNICODETOMACDEVANAGARI_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "x-mac-farsi", &kNS_UNICODETOMACFARSI_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "x-mac-gurmukhi", &kNS_UNICODETOMACGURMUKHI_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "x-mac-gujarati", &kNS_UNICODETOMACGUJARATI_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "x-mac-hebrew", &kNS_UNICODETOMACHEBREW_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "IBM850", &kNS_CP850TOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "IBM852", &kNS_CP852TOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "IBM855", &kNS_CP855TOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "IBM857", &kNS_CP857TOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "IBM862", &kNS_CP862TOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "IBM864", &kNS_CP864TOUNICODE_CID },
#ifdef XP_OS2
  { NS_UNICODEDECODER_CONTRACTID_BASE "IBM869", &kNS_CP869TOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "IBM1125", &kNS_CP1125TOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "IBM1131", &kNS_CP1131TOUNICODE_CID },
#endif
  { NS_UNICODEENCODER_CONTRACTID_BASE "IBM850", &kNS_UNICODETOCP850_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "IBM852", &kNS_UNICODETOCP852_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "IBM855", &kNS_UNICODETOCP855_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "IBM857", &kNS_UNICODETOCP857_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "IBM862", &kNS_UNICODETOCP862_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "IBM864", &kNS_UNICODETOCP864_CID },
#ifdef XP_OS2
  { NS_UNICODEENCODER_CONTRACTID_BASE "IBM869", &kNS_UNICODETOCP869_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "IBM1125", &kNS_UNICODETOCP1125_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "IBM1131", &kNS_UNICODETOCP1131_CID },
#endif
  { NS_UNICODEDECODER_CONTRACTID_BASE "Shift_JIS", &kNS_SJISTOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "EUC-JP", &kNS_EUCJPTOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "ISO-2022-JP", &kNS_ISO2022JPTOUNICODE_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "Shift_JIS", &kNS_UNICODETOSJIS_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "EUC-JP", &kNS_UNICODETOEUCJP_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "ISO-2022-JP", &kNS_UNICODETOISO2022JP_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "jis_0201", &kNS_UNICODETOJISX0201_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "x-euc-tw", &kNS_EUCTWTOUNICODE_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "x-euc-tw", &kNS_UNICODETOEUCTW_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "Big5", &kNS_UNICODETOBIG5_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "Big5", &kNS_BIG5TOUNICODE_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "Big5-HKSCS", &kNS_UNICODETOBIG5HKSCS_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "hkscs-1", &kNS_UNICODETOHKSCS_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "Big5-HKSCS", &kNS_BIG5HKSCSTOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "EUC-KR", &kNS_EUCKRTOUNICODE_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "EUC-KR", &kNS_UNICODETOEUCKR_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "x-johab", &kNS_JOHABTOUNICODE_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "x-johab", &kNS_UNICODETOJOHAB_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "ISO-2022-KR", &kNS_ISO2022KRTOUNICODE_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "GB2312", &kNS_GB2312TOUNICODE_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "GB2312", &kNS_UNICODETOGB2312_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "gbk", &kNS_GBKTOUNICODE_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "gbk", &kNS_UNICODETOGBK_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "HZ-GB-2312", &kNS_HZTOUNICODE_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "HZ-GB-2312", &kNS_UNICODETOHZ_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "gb18030", &kNS_GB18030TOUNICODE_CID },
  { NS_UNICODEENCODER_CONTRACTID_BASE "gb18030", &kNS_UNICODETOGB18030_CID },
  { NS_UNICODEDECODER_CONTRACTID_BASE "ISO-2022-CN", &kNS_ISO2022CNTOUNICODE_CID },
  { nullptr }
};

static const mozilla::Module kUConvModule = {
  mozilla::Module::kVersion,
  kUConvCIDs,
  kUConvContracts,
  kUConvCategories
};

NSMODULE_DEFN(nsUConvModule) = &kUConvModule;
