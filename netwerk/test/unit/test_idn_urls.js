// Test algorithm for unicode display of IDNA URL (bug 722299)

"use strict";

const testcases = [
  //  Original             Punycode or         Expected UTF-8
  //    URL              normalized form

  // Latin script
  ["cuillère", "xn--cuillre-6xa", true],

  // repeated non-spacing marks
  ["gruz̀̀ere", "xn--gruzere-ogea", false],

  // non-XID character
  ["I♥NY", "xn--iny-zx5a", false],

  /*
  Behaviour of this test changed in IDNA2008, replacing the non-XID
  character with U+FFFD replacement character - when all platforms use
  IDNA2008 it can be uncommented and the punycode URL changed to
   "xn--mgbl3eb85703a"

    // new non-XID character in Unicode 6.3
    ["حلا\u061cل", "xn--bgbvr6gc",                    false],
*/

  // U+30FB KATAKANA MIDDLE DOT is excluded from non-XID characters (bug 857490)
  ["乾燥肌・石けん", "xn--08j4gylj12hz80b0uhfup", true],

  // Cyrillic alone
  ["толсто́й", "xn--lsa83dealbred", true],

  // Mixed script Cyrillic/Latin
  ["толсто́й-in-Russian", "xn---in-russian-1jg071b0a8bb4cpd", false],

  // Mixed script Latin/Cyrillic
  ["war-and-миръ", "xn--war-and--b9g3b7b3h", false],

  // Cherokee (Restricted script)
  ["ᏣᎳᎩ", "xn--f9dt7l", false],

  // Yi (former Aspirational script, now Restricted per Unicode 10.0 update to UAX 31)
  ["ꆈꌠꁱꂷ", "xn--4o7a6e1x64c", false],

  // Greek alone
  ["πλάτων", "xn--hxa3ahjw4a", true],

  // Mixed script Greek/Latin
  ["πλάτωνicrelationship", "xn--icrelationship-96j4t9a3cwe2e", false],

  // Mixed script Latin/Greek
  ["spaceὈδύσσεια", "xn--space-h9dui0b0ga2j1562b", false],

  // Devanagari alone
  ["मराठी", "xn--d2b1ag0dl", true],

  // Devanagari with Armenian
  ["मराठीՀայաստան", "xn--y9aaa1d0ai1cq964f8dwa2o1a", false],

  // Devanagari with common
  ["मराठी123", "xn--123-mhh3em2hra", true],

  // Common with Devanagari
  ["123मराठी", "xn--123-phh3em2hra", true],

  // Latin with Han
  ["chairman毛", "xn--chairman-k65r", true],

  // Han with Latin
  ["山葵sauce", "xn--sauce-6j9ii40v", true],

  // Latin with Han, Hiragana and Katakana
  ["van語ではドイ", "xn--van-ub4bpb6w0in486d", true],

  // Latin with Han, Katakana and Hiragana
  ["van語ドイでは", "xn--van-ub4bpb4w0ip486d", true],

  // Latin with Hiragana, Han and Katakana
  ["vanでは語ドイ", "xn--van-ub4bpb6w0ip486d", true],

  // Latin with Hiragana, Katakana and Han
  ["vanではドイ語", "xn--van-ub4bpb6w0ir486d", true],

  // Latin with Katakana, Han and Hiragana
  ["vanドイ語では", "xn--van-ub4bpb4w0ir486d", true],

  // Latin with Katakana, Hiragana and Han
  ["vanドイでは語", "xn--van-ub4bpb4w0it486d", true],

  // Han with Latin, Hiragana and Katakana
  ["語vanではドイ", "xn--van-ub4bpb6w0ik486d", true],

  // Han with Latin, Katakana and Hiragana
  ["語vanドイでは", "xn--van-ub4bpb4w0im486d", true],

  // Han with Hiragana, Latin and Katakana
  ["語ではvanドイ", "xn--van-rb4bpb9w0ik486d", true],

  // Han with Hiragana, Katakana and Latin
  ["語ではドイvan", "xn--van-rb4bpb6w0in486d", true],

  // Han with Katakana, Latin and Hiragana
  ["語ドイvanでは", "xn--van-ub4bpb1w0ip486d", true],

  // Han with Katakana, Hiragana and Latin
  ["語ドイではvan", "xn--van-rb4bpb4w0ip486d", true],

  // Hiragana with Latin, Han and Katakana
  ["イツvan語ではド", "xn--van-ub4bpb1wvhsbx330n", true],

  // Hiragana with Latin, Katakana and Han
  ["ではvanドイ語", "xn--van-rb4bpb9w0ir486d", true],

  // Hiragana with Han, Latin and Katakana
  ["では語vanドイ", "xn--van-rb4bpb9w0im486d", true],

  // Hiragana with Han, Katakana and Latin
  ["では語ドイvan", "xn--van-rb4bpb6w0ip486d", true],

  // Hiragana with Katakana, Latin and Han
  ["ではドイvan語", "xn--van-rb4bpb6w0iu486d", true],

  // Hiragana with Katakana, Han and Latin
  ["ではドイ語van", "xn--van-rb4bpb6w0ir486d", true],

  // Katakana with Latin, Han and Hiragana
  ["ドイvan語では", "xn--van-ub4bpb1w0iu486d", true],

  // Katakana with Latin, Hiragana and Han
  ["ドイvanでは語", "xn--van-ub4bpb1w0iw486d", true],

  // Katakana with Han, Latin and Hiragana
  ["ドイ語vanでは", "xn--van-ub4bpb1w0ir486d", true],

  // Katakana with Han, Hiragana and Latin
  ["ドイ語ではvan", "xn--van-rb4bpb4w0ir486d", true],

  // Katakana with Hiragana, Latin and Han
  ["ドイではvan語", "xn--van-rb4bpb4w0iw486d", true],

  // Katakana with Hiragana, Han and Latin
  ["ドイでは語van", "xn--van-rb4bpb4w0it486d", true],

  // Han with common
  ["中国123", "xn--123-u68dy61b", true],

  // common with Han
  ["123中国", "xn--123-x68dy61b", true],

  // Characters that normalize to permitted characters
  //  (also tests Plane 1 supplementary characters)
  ["super𝟖", "super8", true],

  // Han from Plane 2
  ["𠀀𠀁𠀂", "xn--j50icd", true],

  // Han from Plane 2 with js (UTF-16) escapes
  ["\uD840\uDC00\uD840\uDC01\uD840\uDC02", "xn--j50icd", true],

  // Same with a lone high surrogate at the end
  // Throws due to unpaired surrogate
  //  ["\uD840\uDC00\uD840\uDC01\uD840", "xn--zn7c0336bda", false],

  // Latin text and Bengali digits
  ["super৪", "xn--super-k2l", false],

  // Bengali digits and Latin text
  ["৫ab", "xn--ab-x5f", false],

  // Bengali text and Latin digits
  ["অঙ্কুর8", "xn--8-70d2cp0j6dtd", true],

  // Latin digits and Bengali text
  ["5াব", "xn--5-h3d7c", true],

  // Mixed numbering systems
  // Throws due to bidi rule violation
  // ["٢٠۰٠", "xn--8hbae38c", false],

  // Traditional Chinese
  ["萬城", "xn--uis754h", true],

  // Simplified Chinese
  ["万城", "xn--chq31v", true],

  // Simplified-only and Traditional-only Chinese in the same label
  ["万萬城", "xn--chq31vsl1b", true],

  // Traditional-only and Simplified-only Chinese in the same label
  ["萬万城", "xn--chq31vrl1b", true],

  // Han and Latin and Bopomofo
  ["注音符号bopomofoㄅㄆㄇㄈ", "xn--bopomofo-hj5gkalm1637i876cuw0brk5f", true],

  // Han, bopomofo, Latin
  // Bug 1885096: Since the last character of "ㄅㄆㄇㄈ" is a CJK Ideograph,
  // just use the first character "ㄅ" from the sequence "ㄅㄆㄇㄈ".
  ["注音符号ㄅbopomofo", "xn--bopomofo-8i5gx891aylvccz9asi4e", true],

  // Latin, Han, Bopomofo
  ["bopomofo注音符号ㄅㄆㄇㄈ", "xn--bopomofo-hj5gkalm9637i876cuw0brk5f", true],

  // Latin, Bopomofo, Han
  ["bopomofoㄅㄆㄇㄈ注音符号", "xn--bopomofo-hj5gkalm3737i876cuw0brk5f", true],

  // Bopomofo, Han, Latin
  ["ㄅㄆㄇㄈ注音符号bopomofo", "xn--bopomofo-8i5gkalm3737i876cuw0brk5f", true],

  // Bopomofo, Latin, Han
  // Bug 1885096: Since the last character of "ㄅㄆㄇㄈ" is a CJK Ideograph,
  // just use the first character "ㄅ" from the sequence "ㄅㄆㄇㄈ".
  ["ㄅbopomofo注音符号", "xn--bopomofo-8i5g6891aylvccz9asi4e", true],

  // Han, bopomofo and katakana
  ["注音符号ㄅㄆㄇㄈボポモフォ", "xn--jckteuaez1shij0450gylvccz9asi4e", false],

  // Han, katakana, bopomofo
  ["注音符号ボポモフォㄅㄆㄇㄈ", "xn--jckteuaez6shij5350gylvccz9asi4e", false],

  // bopomofo, han, katakana
  ["ㄅㄆㄇㄈ注音符号ボポモフォ", "xn--jckteuaez1shij4450gylvccz9asi4e", false],

  // bopomofo, katakana, han
  ["ㄅㄆㄇㄈボポモフォ注音符号", "xn--jckteuaez1shij9450gylvccz9asi4e", false],

  // katakana, Han, bopomofo
  ["ボポモフォ注音符号ㄅㄆㄇㄈ", "xn--jckteuaez6shij0450gylvccz9asi4e", false],

  // katakana, bopomofo, Han
  ["ボポモフォㄅㄆㄇㄈ注音符号", "xn--jckteuaez6shij4450gylvccz9asi4e", false],

  // Han, Hangul and Latin
  ["韓한글hangul", "xn--hangul-2m5ti09k79ze", true],

  // Han, Latin and Hangul
  ["韓hangul한글", "xn--hangul-2m5to09k79ze", true],

  // Hangul, Han and Latin
  ["한글韓hangul", "xn--hangul-2m5th09k79ze", true],

  // Hangul, Latin and Han
  ["한글hangul韓", "xn--hangul-8m5t898k79ze", true],

  // Latin, Han and Hangul
  ["hangul韓한글", "xn--hangul-8m5ti09k79ze", true],

  // Latin, Hangul and Han
  ["hangul한글韓", "xn--hangul-8m5th09k79ze", true],

  // Hangul and katakana
  ["한글ハングル", "xn--qck1c2d4a9266lkmzb", false],

  // Katakana and Hangul
  ["ハングル한글", "xn--qck1c2d4a2366lkmzb", false],

  // Thai (also tests that node with over 63 UTF-8 octets doesn't fail)
  [
    "เครื่องทําน้ําทําน้ําแข็ง",
    "xn--22cdjb2fanb9fyepcbbb9dwh4a3igze4fdcd",
    true,
  ],

  // Effect of adding valid or invalid subdomains (bug 1399540)
  ["䕮䕵䕶䕱.ascii", "xn--google.ascii", true],
  ["ascii.䕮䕵䕶䕱", "ascii.xn--google", true],
  ["中国123.䕮䕵䕶䕱", "xn--123-u68dy61b.xn--google", true],
  ["䕮䕵䕶䕱.中国123", "xn--google.xn--123-u68dy61b", true],
  // Throw due to bogus Punycode
  // [
  //   "xn--accountlogin.䕮䕵䕶䕱",
  //   "xn--accountlogin.xn--google",
  //   true,
  // ],
  // [
  //   "䕮䕵䕶䕱.xn--accountlogin",
  //   "xn--google.xn--accountlogin",
  //   true,
  // ],

  // Arabic diacritic not allowed in Latin text (bug 1370497)
  ["goo\u0650gle", "xn--google-yri", false],
  // ...but Arabic diacritics are allowed on Arabic text
  ["العَرَبِي", "xn--mgbc0a5a6cxbzabt", true],

  // Hebrew diacritic also not allowed in Latin text (bug 1404349)
  ["goo\u05b4gle", "xn--google-rvh", false],

  // Accents above dotless-i are not allowed
  ["na\u0131\u0308ve", "xn--nave-mza04z", false],
  ["d\u0131\u0302ner", "xn--dner-lza40z", false],
  // but the corresponding accented-i (based on dotted i) is OK
  ["na\u00efve.com", "xn--nave-6pa.com", true],
  ["d\u00eener.com", "xn--dner-0pa.com", true],
];

function run_test() {
  var idnService = Cc["@mozilla.org/network/idn-service;1"].getService(
    Ci.nsIIDNService
  );

  for (var j = 0; j < testcases.length; ++j) {
    var test = testcases[j];
    var URL = test[0] + ".com";
    var punycodeURL = test[1] + ".com";
    var expectedUnicode = test[2];

    var result;
    try {
      result = idnService.convertToDisplayIDN(URL);
    } catch (e) {
      result = ".com";
    }
    if (
      punycodeURL.substr(0, 4) == "xn--" ||
      punycodeURL.indexOf(".xn--") > 0
    ) {
      // test convertToDisplayIDN with a Unicode URL and with a
      //  Punycode URL if we have one
      Assert.equal(
        escape(result),
        expectedUnicode ? escape(URL) : escape(punycodeURL)
      );

      result = idnService.convertToDisplayIDN(punycodeURL);
      Assert.equal(
        escape(result),
        expectedUnicode ? escape(URL) : escape(punycodeURL)
      );
    } else {
      // The "punycode" URL isn't punycode. This happens in testcases
      // where the Unicode URL has become normalized to an ASCII URL,
      // so, even though expectedUnicode is true, the expected result
      // is equal to punycodeURL
      Assert.equal(escape(result), escape(punycodeURL));
    }
  }
}
