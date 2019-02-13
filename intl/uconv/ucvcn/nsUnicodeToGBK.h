/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

 /**
 * A character set converter from Unicode to GBK.
 * 
 *
 * @created         08/Sept/1999
 * @author  Yueheng Xu, Yueheng.Xu@intel.com
 */

#ifndef nsUnicodeToGBK_h___
#define nsUnicodeToGBK_h___

#include "nsUCSupport.h"
#include "nsCOMPtr.h"
#include "nsIUnicodeEncoder.h"
#include "nsGBKConvUtil.h"
//----------------------------------------------------------------------
// Class nsUnicodeToGBK [declaration]

class nsUnicodeToGBK: public nsEncoderSupport
{
public:

  /**
   * Class constructor.
   */
  explicit nsUnicodeToGBK(uint32_t aMaxLengthFactor = 2);
  virtual ~nsUnicodeToGBK() {}

protected:

  //--------------------------------------------------------------------
  // Subclassing of nsEncoderSupport class [declaration]
  NS_IMETHOD ConvertNoBuffNoErr(const char16_t * aSrc,
                                int32_t * aSrcLength,
                                char * aDest,
                                int32_t * aDestLength);

  virtual void CreateExtensionEncoder();
  virtual void Create4BytesEncoder();

  nsCOMPtr<nsIUnicodeEncoder> mExtensionEncoder;
  nsCOMPtr<nsIUnicodeEncoder> m4BytesEncoder;
protected:
  char16_t mSurrogateHigh;
  nsGBKConvUtil mUtil;
  nsresult TryExtensionEncoder(char16_t aChar, char* aDest, int32_t* aOutLen);
  nsresult Try4BytesEncoder(char16_t aChar, char* aDest, int32_t* aOutLen);
  virtual nsresult EncodeSurrogate(char16_t aSurrogateHigh,
                                   char16_t aSurrogateLow, char* aDest,
                                   int32_t aDestLength, int32_t aBufferLength);
};

class nsUnicodeToGB18030: public nsUnicodeToGBK
{
public:
  nsUnicodeToGB18030() : nsUnicodeToGBK(4) {}
  virtual ~nsUnicodeToGB18030() {}
protected:
  virtual void CreateExtensionEncoder();
  virtual void Create4BytesEncoder();
  virtual nsresult EncodeSurrogate(char16_t aSurrogateHigh,
                                   char16_t aSurrogateLow, char* aDest,
                                   int32_t aDestLength, int32_t aBufferLength);
};

#endif /* nsUnicodeToGBK_h___ */

