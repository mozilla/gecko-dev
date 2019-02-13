/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


//
// Part of the reason these routines are all in once place is so that as new
// data flavors are added that are known to be one-byte or two-byte strings, or even
// raw binary data, then we just have to go to one place to change how the data
// moves into/out of the primitives and native line endings.
//
// If you add new flavors that have special consideration (binary data or one-byte
// char* strings), please update all the helper classes in this file.
//
// For now, this is the assumption that we are making:
//  - text/plain is always a char*
//  - anything else is a char16_t*
//


#include "nsPrimitiveHelpers.h"
#include "nsCOMPtr.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsITransferable.h"
#include "nsIComponentManager.h"
#include "nsLinebreakConverter.h"
#include "nsReadableUtils.h"

#include "nsIServiceManager.h"
// unicode conversion
#include "nsIPlatformCharset.h"
#include "nsIUnicodeDecoder.h"
#include "nsISaveAsCharset.h"
#include "nsAutoPtr.h"
#include "mozilla/Likely.h"
#include "mozilla/dom/EncodingUtils.h"

using mozilla::dom::EncodingUtils;


//
// CreatePrimitiveForData
//
// Given some data and the flavor it corresponds to, creates the appropriate
// nsISupports* wrapper for passing across IDL boundaries. Right now, everything
// creates a two-byte |nsISupportsString|, except for "text/plain" and native
// platform HTML (CF_HTML on win32)
//
void
nsPrimitiveHelpers :: CreatePrimitiveForData ( const char* aFlavor, const void* aDataBuff,
                                                 uint32_t aDataLen, nsISupports** aPrimitive )
{
  if ( !aPrimitive )
    return;

  if ( strcmp(aFlavor,kTextMime) == 0 || strcmp(aFlavor,kNativeHTMLMime) == 0 ) {
    nsCOMPtr<nsISupportsCString> primitive =
        do_CreateInstance(NS_SUPPORTS_CSTRING_CONTRACTID);
    if ( primitive ) {
      const char * start = reinterpret_cast<const char*>(aDataBuff);
      primitive->SetData(Substring(start, start + aDataLen));
      NS_ADDREF(*aPrimitive = primitive);
    }
  }
  else {
    nsCOMPtr<nsISupportsString> primitive =
        do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID);
    if (primitive ) {
      if (aDataLen % 2) {
        nsAutoArrayPtr<char> buffer(new char[aDataLen + 1]);
        if (!MOZ_LIKELY(buffer))
          return;

        memcpy(buffer, aDataBuff, aDataLen);
        buffer[aDataLen] = 0;
        const char16_t* start = reinterpret_cast<const char16_t*>(buffer.get());
        // recall that length takes length as characters, not bytes
        primitive->SetData(Substring(start, start + (aDataLen + 1) / 2));
      } else {
        const char16_t* start = reinterpret_cast<const char16_t*>(aDataBuff);
        // recall that length takes length as characters, not bytes
        primitive->SetData(Substring(start, start + (aDataLen / 2)));
      }
      NS_ADDREF(*aPrimitive = primitive);
    }
  }

} // CreatePrimitiveForData


//
// CreateDataFromPrimitive
//
// Given a nsISupports* primitive and the flavor it represents, creates a new data
// buffer with the data in it. This data will be null terminated, but the length
// parameter does not reflect that.
//
void
nsPrimitiveHelpers :: CreateDataFromPrimitive ( const char* aFlavor, nsISupports* aPrimitive,
                                                   void** aDataBuff, uint32_t aDataLen )
{
  if ( !aDataBuff )
    return;

  *aDataBuff = nullptr;

  if ( strcmp(aFlavor,kTextMime) == 0 ) {
    nsCOMPtr<nsISupportsCString> plainText ( do_QueryInterface(aPrimitive) );
    if ( plainText ) {
      nsAutoCString data;
      plainText->GetData ( data );
      *aDataBuff = ToNewCString(data);
    }
  }
  else {
    nsCOMPtr<nsISupportsString> doubleByteText ( do_QueryInterface(aPrimitive) );
    if ( doubleByteText ) {
      nsAutoString data;
      doubleByteText->GetData ( data );
      *aDataBuff = ToNewUnicode(data);
    }
  }

}


//
// ConvertUnicodeToPlatformPlainText
//
// Given a unicode buffer (flavor text/unicode), this converts it to plain text using
// the appropriate platform charset encoding. |inUnicodeLen| is the length of the input
// string, not the # of bytes in the buffer. The |outPlainTextData| is null terminated,
// but its length parameter, |outPlainTextLen|, does not reflect that.
//
nsresult
nsPrimitiveHelpers :: ConvertUnicodeToPlatformPlainText ( char16_t* inUnicode, int32_t inUnicodeLen,
                                                            char** outPlainTextData, int32_t* outPlainTextLen )
{
  if ( !outPlainTextData || !outPlainTextLen )
    return NS_ERROR_INVALID_ARG;

  // get the charset
  nsresult rv;
  nsCOMPtr <nsIPlatformCharset> platformCharsetService = do_GetService(NS_PLATFORMCHARSET_CONTRACTID, &rv);

  nsAutoCString platformCharset;
  if (NS_SUCCEEDED(rv))
    rv = platformCharsetService->GetCharset(kPlatformCharsetSel_PlainTextInClipboard, platformCharset);
  if (NS_FAILED(rv))
    platformCharset.AssignLiteral("ISO-8859-1");

  // use transliterate to convert things like smart quotes to normal quotes for plain text

  nsCOMPtr<nsISaveAsCharset> converter = do_CreateInstance("@mozilla.org/intl/saveascharset;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = converter->Init(platformCharset.get(),
                  nsISaveAsCharset::attr_EntityAfterCharsetConv +
                  nsISaveAsCharset::attr_FallbackQuestionMark,
                  0);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = converter->Convert(inUnicode, outPlainTextData);
  *outPlainTextLen = *outPlainTextData ? strlen(*outPlainTextData) : 0;

  NS_ASSERTION ( NS_SUCCEEDED(rv), "Error converting unicode to plain text" );

  return rv;
} // ConvertUnicodeToPlatformPlainText


//
// ConvertPlatformPlainTextToUnicode
//
// Given a char buffer (flavor text/plaikn), this converts it to unicode using
// the appropriate platform charset encoding. |outUnicode| is null terminated,
// but its length parameter, |outUnicodeLen|, does not reflect that. |outUnicodeLen| is
// the length of the string in characters, not bytes.
//
nsresult
nsPrimitiveHelpers :: ConvertPlatformPlainTextToUnicode ( const char* inText, int32_t inTextLen,
                                                            char16_t** outUnicode, int32_t* outUnicodeLen )
{
  if ( !outUnicode || !outUnicodeLen )
    return NS_ERROR_INVALID_ARG;

  // Get the appropriate unicode decoder. We're guaranteed that this won't change
  // through the life of the app so we can cache it.
  nsresult rv = NS_OK;
  static nsCOMPtr<nsIUnicodeDecoder> decoder;
  static bool hasConverter = false;
  if ( !hasConverter ) {
    // get the charset
    nsAutoCString platformCharset;
    nsCOMPtr <nsIPlatformCharset> platformCharsetService = do_GetService(NS_PLATFORMCHARSET_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv))
      rv = platformCharsetService->GetCharset(kPlatformCharsetSel_PlainTextInClipboard, platformCharset);
    if (NS_FAILED(rv))
      platformCharset.AssignLiteral("windows-1252");

    decoder = EncodingUtils::DecoderForEncoding(platformCharset);

    hasConverter = true;
  }

  // Estimate out length and allocate the buffer based on a worst-case estimate, then do
  // the conversion.
  rv = decoder->GetMaxLength(inText, inTextLen, outUnicodeLen);   // |outUnicodeLen| is number of chars
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if ( *outUnicodeLen ) {
    *outUnicode = reinterpret_cast<char16_t*>(moz_xmalloc((*outUnicodeLen + 1) * sizeof(char16_t)));
    if ( *outUnicode ) {
      rv = decoder->Convert(inText, &inTextLen, *outUnicode, outUnicodeLen);
      (*outUnicode)[*outUnicodeLen] = '\0';                   // null terminate. Convert() doesn't do it for us
    }
  } // if valid length

  NS_ASSERTION ( NS_SUCCEEDED(rv), "Error converting plain text to unicode" );

  return rv;
} // ConvertPlatformPlainTextToUnicode


//
// ConvertPlatformToDOMLinebreaks
//
// Given some data, convert from the platform linebreaks into the LF expected by the
// DOM. This will attempt to convert the data in place, but the buffer may still need to
// be reallocated regardless (disposing the old buffer is taken care of internally, see
// the note below).
//
// NOTE: this assumes that it can use 'free' to dispose of the old buffer.
//
nsresult
nsLinebreakHelpers :: ConvertPlatformToDOMLinebreaks ( const char* inFlavor, void** ioData,
                                                          int32_t* ioLengthInBytes )
{
  NS_ASSERTION ( ioData && *ioData && ioLengthInBytes, "Bad Params");
  if ( !(ioData && *ioData && ioLengthInBytes) )
    return NS_ERROR_INVALID_ARG;

  nsresult retVal = NS_OK;

  if ( strcmp(inFlavor, "text/plain") == 0 ) {
    char* buffAsChars = reinterpret_cast<char*>(*ioData);
    char* oldBuffer = buffAsChars;
    retVal = nsLinebreakConverter::ConvertLineBreaksInSitu ( &buffAsChars, nsLinebreakConverter::eLinebreakAny,
                                                              nsLinebreakConverter::eLinebreakContent,
                                                              *ioLengthInBytes, ioLengthInBytes );
    if ( NS_SUCCEEDED(retVal) ) {
      if ( buffAsChars != oldBuffer )             // check if buffer was reallocated
        free ( oldBuffer );
      *ioData = buffAsChars;
    }
  }
  else if ( strcmp(inFlavor, "image/jpeg") == 0 ) {
    // I'd assume we don't want to do anything for binary data....
  }
  else {
    char16_t* buffAsUnichar = reinterpret_cast<char16_t*>(*ioData);
    char16_t* oldBuffer = buffAsUnichar;
    int32_t newLengthInChars;
    retVal = nsLinebreakConverter::ConvertUnicharLineBreaksInSitu ( &buffAsUnichar, nsLinebreakConverter::eLinebreakAny,
                                                                     nsLinebreakConverter::eLinebreakContent,
                                                                     *ioLengthInBytes / sizeof(char16_t), &newLengthInChars );
    if ( NS_SUCCEEDED(retVal) ) {
      if ( buffAsUnichar != oldBuffer )           // check if buffer was reallocated
        free ( oldBuffer );
      *ioData = buffAsUnichar;
      *ioLengthInBytes = newLengthInChars * sizeof(char16_t);
    }
  }

  return retVal;

} // ConvertPlatformToDOMLinebreaks
