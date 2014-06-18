/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCSPUtils_h___
#define nsCSPUtils_h___

#include "nsCOMPtr.h"
#include "nsIContentPolicy.h"
#include "nsIURI.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsUnicharUtils.h"
#include "prlog.h"

/* =============== Logging =================== */

void CSP_LogLocalizedStr(const char16_t* aName,
                         const char16_t** aParams,
                         uint32_t aLength,
                         const nsAString& aSourceName,
                         const nsAString& aSourceLine,
                         uint32_t aLineNumber,
                         uint32_t aColumnNumber,
                         uint32_t aFlags,
                         const char* aCategory,
                         uint32_t aInnerWindowID);

void CSP_GetLocalizedStr(const char16_t* aName,
                         const char16_t** aParams,
                         uint32_t aLength,
                         char16_t** outResult);

void CSP_LogStrMessage(const nsAString& aMsg);

void CSP_LogMessage(const nsAString& aMessage,
                    const nsAString& aSourceName,
                    const nsAString& aSourceLine,
                    uint32_t aLineNumber,
                    uint32_t aColumnNumber,
                    uint32_t aFlags,
                    const char* aCategory,
                    uint32_t aInnerWindowID);


/* =============== Constant and Type Definitions ================== */

#define INLINE_STYLE_VIOLATION_OBSERVER_TOPIC   "violated base restriction: Inline Stylesheets will not apply"
#define INLINE_SCRIPT_VIOLATION_OBSERVER_TOPIC  "violated base restriction: Inline Scripts will not execute"
#define EVAL_VIOLATION_OBSERVER_TOPIC           "violated base restriction: Code will not be created from strings"
#define SCRIPT_NONCE_VIOLATION_OBSERVER_TOPIC   "Inline Script had invalid nonce"
#define STYLE_NONCE_VIOLATION_OBSERVER_TOPIC    "Inline Style had invalid nonce"
#define SCRIPT_HASH_VIOLATION_OBSERVER_TOPIC    "Inline Script had invalid hash"
#define STYLE_HASH_VIOLATION_OBSERVER_TOPIC     "Inline Style had invalid hash"


// Please add any new enum items not only to CSPDirective, but also add
// a string version for every enum >> using the same index << to
// CSPStrDirectives underneath.
enum CSPDirective {
  CSP_DEFAULT_SRC = 0,
  CSP_SCRIPT_SRC,
  CSP_OBJECT_SRC,
  CSP_STYLE_SRC,
  CSP_IMG_SRC,
  CSP_MEDIA_SRC,
  CSP_FRAME_SRC,
  CSP_FONT_SRC,
  CSP_CONNECT_SRC,
  CSP_REPORT_URI,
  CSP_FRAME_ANCESTORS,
  // CSP_LAST_DIRECTIVE_VALUE always needs to be the last element in the enum
  // because we use it to calculate the size for the char* array.
  CSP_LAST_DIRECTIVE_VALUE
};

static const char* CSPStrDirectives[] = {
  "default-src",    // CSP_DEFAULT_SRC = 0
  "script-src",     // CSP_SCRIPT_SRC
  "object-src",     // CSP_OBJECT_SRC
  "style-src",      // CSP_STYLE_SRC
  "img-src",        // CSP_IMG_SRC
  "media-src",      // CSP_MEDIA_SRC
  "frame-src",      // CSP_FRAME_SRC
  "font-src",       // CSP_FONT_SRC
  "connect-src",    // CSP_CONNECT_SRC
  "report-uri",     // CSP_REPORT_URI
  "frame-ancestors" // CSP_FRAME_ANCESTORS
};

inline const char* CSP_EnumToDirective(enum CSPDirective aDir)
{
  // Make sure all elements in enum CSPDirective got added to CSPStrDirectives.
  static_assert((sizeof(CSPStrDirectives) / sizeof(CSPStrDirectives[0]) ==
                static_cast<uint32_t>(CSP_LAST_DIRECTIVE_VALUE)),
                "CSP_LAST_DIRECTIVE_VALUE does not match length of CSPStrDirectives");
  return CSPStrDirectives[static_cast<uint32_t>(aDir)];
}

inline CSPDirective CSP_DirectiveToEnum(const nsAString& aDir)
{
  nsString lowerDir = PromiseFlatString(aDir);
  ToLowerCase(lowerDir);

  static_assert(CSP_LAST_DIRECTIVE_VALUE ==
                (sizeof(CSPStrDirectives) / sizeof(CSPStrDirectives[0])),
                "CSP_LAST_DIRECTIVE_VALUE does not match length of CSPStrDirectives");

  for (uint32_t i = 0; i < CSP_LAST_DIRECTIVE_VALUE; i++) {
    if (lowerDir.EqualsASCII(CSPStrDirectives[i])) {
      return static_cast<CSPDirective>(i);
    }
  }
  NS_ASSERTION(false, "Can not convert unknown Directive to Enum");
  return CSP_LAST_DIRECTIVE_VALUE;
}

// Please add any new enum items not only to CSPKeyword, but also add
// a string version for every enum >> using the same index << to
// CSPStrKeywords underneath.
enum CSPKeyword {
  CSP_SELF = 0,
  CSP_UNSAFE_INLINE,
  CSP_UNSAFE_EVAL,
  CSP_NONE,
  CSP_NONCE,
  // CSP_LAST_KEYWORD_VALUE always needs to be the last element in the enum
  // because we use it to calculate the size for the char* array.
  CSP_LAST_KEYWORD_VALUE,
  // Putting CSP_HASH after the delimitor, because CSP_HASH is not a valid
  // keyword (hash uses e.g. sha256, sha512) but we use CSP_HASH internally
  // to identify allowed hashes in ::allows.
  CSP_HASH
 };

static const char* CSPStrKeywords[] = {
  "'self'",          // CSP_SELF = 0
  "'unsafe-inline'", // CSP_UNSAFE_INLINE
  "'unsafe-eval'",   // CSP_UNSAFE_EVAL
  "'none'",          // CSP_NONE
  "'nonce-",         // CSP_NONCE
  // Remember: CSP_HASH is not supposed to be used
};

inline const char* CSP_EnumToKeyword(enum CSPKeyword aKey)
{
  // Make sure all elements in enum CSPKeyword got added to CSPStrKeywords.
  static_assert((sizeof(CSPStrKeywords) / sizeof(CSPStrKeywords[0]) ==
                static_cast<uint32_t>(CSP_LAST_KEYWORD_VALUE)),
                "CSP_LAST_KEYWORD_VALUE does not match length of CSPStrKeywords");
  return CSPStrKeywords[static_cast<uint32_t>(aKey)];
}

inline CSPKeyword CSP_KeywordToEnum(const nsAString& aKey)
{
  nsString lowerKey = PromiseFlatString(aKey);
  ToLowerCase(lowerKey);

  static_assert(CSP_LAST_KEYWORD_VALUE ==
                (sizeof(CSPStrKeywords) / sizeof(CSPStrKeywords[0])),
                 "CSP_LAST_KEYWORD_VALUE does not match length of CSPStrKeywords");

  for (uint32_t i = 0; i < CSP_LAST_KEYWORD_VALUE; i++) {
    if (lowerKey.EqualsASCII(CSPStrKeywords[i])) {
      return static_cast<CSPKeyword>(i);
    }
  }
  NS_ASSERTION(false, "Can not convert unknown Keyword to Enum");
  return CSP_LAST_KEYWORD_VALUE;
}

/* =============== Helpers ================== */

class nsCSPHostSrc;

nsCSPHostSrc* CSP_CreateHostSrcFromURI(nsIURI* aURI);
bool CSP_IsValidDirective(const nsAString& aDir);
bool CSP_IsDirective(const nsAString& aValue, enum CSPDirective aDir);
bool CSP_IsKeyword(const nsAString& aValue, enum CSPKeyword aKey);
bool CSP_IsQuotelessKeyword(const nsAString& aKey);

/* =============== nsCSPSrc ================== */

class nsCSPBaseSrc {
  public:
    nsCSPBaseSrc();
    virtual ~nsCSPBaseSrc();

    virtual bool permits(nsIURI* aUri, const nsAString& aNonce) const;
    virtual bool allows(enum CSPKeyword aKeyword, const nsAString& aHashOrNonce) const;
    virtual void toString(nsAString& outStr) const = 0;
};

/* =============== nsCSPSchemeSrc ============ */

class nsCSPSchemeSrc : public nsCSPBaseSrc {
  public:
    nsCSPSchemeSrc(const nsAString& aScheme);
    virtual ~nsCSPSchemeSrc();

    bool permits(nsIURI* aUri, const nsAString& aNonce) const;
    void toString(nsAString& outStr) const;

  private:
    nsString mScheme;
};

/* =============== nsCSPHostSrc ============== */

class nsCSPHostSrc : public nsCSPBaseSrc {
  public:
    nsCSPHostSrc(const nsAString& aHost);
    virtual ~nsCSPHostSrc();

    bool permits(nsIURI* aUri, const nsAString& aNonce) const;
    void toString(nsAString& outStr) const;

    void setScheme(const nsAString& aScheme);
    void setPort(const nsAString& aPort);
    void appendPath(const nsAString &aPath);
    void setFileAndArguments(const nsAString& aFile);

  private:
    nsString mScheme;
    nsString mHost;
    nsString mPort;
    nsString mPath;
    nsString mFileAndArguments;
};

/* =============== nsCSPKeywordSrc ============ */

class nsCSPKeywordSrc : public nsCSPBaseSrc {
  public:
    nsCSPKeywordSrc(CSPKeyword aKeyword);
    virtual ~nsCSPKeywordSrc();

    bool allows(enum CSPKeyword aKeyword, const nsAString& aHashOrNonce) const;
    void toString(nsAString& outStr) const;

  private:
    CSPKeyword mKeyword;
};

/* =============== nsCSPNonceSource =========== */

class nsCSPNonceSrc : public nsCSPBaseSrc {
  public:
    nsCSPNonceSrc(const nsAString& aNonce);
    virtual ~nsCSPNonceSrc();

    bool permits(nsIURI* aUri, const nsAString& aNonce) const;
    bool allows(enum CSPKeyword aKeyword, const nsAString& aHashOrNonce) const;
    void toString(nsAString& outStr) const;

  private:
    nsString mNonce;
};

/* =============== nsCSPHashSource ============ */

class nsCSPHashSrc : public nsCSPBaseSrc {
  public:
    nsCSPHashSrc(const nsAString& algo, const nsAString& hash);
    virtual ~nsCSPHashSrc();

    bool allows(enum CSPKeyword aKeyword, const nsAString& aHashOrNonce) const;
    void toString(nsAString& outStr) const;

  private:
    nsString mAlgorithm;
    nsString mHash;
};

/* =============== nsCSPReportURI ============ */

class nsCSPReportURI : public nsCSPBaseSrc {
  public:
    nsCSPReportURI(nsIURI *aURI);
    virtual ~nsCSPReportURI();

    void toString(nsAString& outStr) const;

  private:
    nsCOMPtr<nsIURI> mReportURI;
};

/* =============== nsCSPDirective ============= */

class nsCSPDirective {
  public:
    nsCSPDirective();
    nsCSPDirective(enum CSPDirective aDirective);
    virtual ~nsCSPDirective();

    bool permits(nsIURI* aUri, const nsAString& aNonce) const;
    bool allows(enum CSPKeyword aKeyword, const nsAString& aHashOrNonce) const;
    void toString(nsAString& outStr) const;

    inline void addSrcs(const nsTArray<nsCSPBaseSrc*>& aSrcs)
      { mSrcs = aSrcs; }

    bool directiveNameEqualsContentType(nsContentPolicyType aContentType) const;

    inline bool isDefaultDirective() const
     { return mDirective == CSP_DEFAULT_SRC; }

    inline bool equals(enum CSPDirective aDirective) const
      { return (mDirective == aDirective); }

    void getReportURIs(nsTArray<nsString> &outReportURIs) const;

  private:
    CSPDirective            mDirective;
    nsTArray<nsCSPBaseSrc*> mSrcs;
};

/* =============== nsCSPPolicy ================== */

class nsCSPPolicy {
  public:
    nsCSPPolicy();
    virtual ~nsCSPPolicy();

    bool permits(nsContentPolicyType aContentType,
                 nsIURI* aUri,
                 const nsAString& aNonce,
                 nsAString& outViolatedDirective) const;
    bool allows(nsContentPolicyType aContentType,
                enum CSPKeyword aKeyword,
                const nsAString& aHashOrNonce) const;
    bool allows(nsContentPolicyType aContentType,
                enum CSPKeyword aKeyword) const;
    void toString(nsAString& outStr) const;

    inline void addDirective(nsCSPDirective* aDir)
      { mDirectives.AppendElement(aDir); }

    bool directiveExists(enum CSPDirective aDir) const;

    inline void setReportOnlyFlag(bool aFlag)
      { mReportOnly = aFlag; }

    inline bool getReportOnlyFlag() const
      { return mReportOnly; }

    void getReportURIs(nsTArray<nsString> &outReportURIs) const;

    void getDirectiveStringForContentType(nsContentPolicyType aContentType,
                                          nsAString& outDirective) const;

    inline uint32_t getNumDirectives() const
      { return mDirectives.Length(); }

  private:
    nsTArray<nsCSPDirective*> mDirectives;
    bool                      mReportOnly;
};

#endif /* nsCSPUtils_h___ */
