
/*
 * Copyright 2010 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#ifndef SkPDFDocument_DEFINED
#define SkPDFDocument_DEFINED

#include "SkAdvancedTypefaceMetrics.h"
#include "SkRefCnt.h"
#include "SkTDArray.h"
#include "SkTemplates.h"

class SkPDFCatalog;
class SkPDFDevice;
class SkPDFDict;
class SkPDFPage;
class SkPDFObject;
class SkWStream;
template <typename T> class SkTSet;

/** \class SkPDFDocument

    A SkPDFDocument assembles pages together and generates the final PDF file.
*/
class SkPDFDocument {
public:
    enum Flags {
        kNoCompression_Flags = 0x01,  //!< DEPRECATED.
        kFavorSpeedOverSize_Flags = 0x01,  //!< Don't compress the stream, but
                                           // if it is already compressed return
                                           // the compressed stream.
        kNoLinks_Flags       = 0x02,  //!< do not honor link annotations.

        kDraftMode_Flags     = 0x01,
    };
    /** Create a PDF document.
     */
    explicit SK_API SkPDFDocument(Flags flags = (Flags)0);
    SK_API ~SkPDFDocument();

    /** Output the PDF to the passed stream.  It is an error to call this (it
     *  will return false and not modify stream) if no pages have been added
     *  or there are pages missing (i.e. page 1 and 3 have been added, but not
     *  page 2).
     *
     *  @param stream    The writable output stream to send the PDF to.
     */
    SK_API bool emitPDF(SkWStream* stream);

    /** Sets the specific page to the passed PDF device. If the specified
     *  page is already set, this overrides it. Returns true if successful.
     *  Will fail if the document has already been emitted.
     *
     *  @param pageNumber The position to add the passed device (1 based).
     *  @param pdfDevice  The page to add to this document.
     */
    SK_API bool setPage(int pageNumber, SkPDFDevice* pdfDevice);

    /** Append the passed pdf device to the document as a new page.  Returns
     *  true if successful.  Will fail if the document has already been emitted.
     *
     *  @param pdfDevice The page to add to this document.
     */
    SK_API bool appendPage(SkPDFDevice* pdfDevice);

    /** Get the count of unique font types used in the document.
     * DEPRECATED.
     */
    SK_API void getCountOfFontTypes(
        int counts[SkAdvancedTypefaceMetrics::kOther_Font + 2]) const;

    /** Get the count of unique font types used in the document.
     */
    SK_API void getCountOfFontTypes(
        int counts[SkAdvancedTypefaceMetrics::kOther_Font + 1],
        int* notSubsettableCount,
        int* notEmbedddableCount) const;

private:
    SkAutoTDelete<SkPDFCatalog> fCatalog;
    int64_t fXRefFileOffset;

    SkTDArray<SkPDFPage*> fPages;
    SkTDArray<SkPDFDict*> fPageTree;
    SkPDFDict* fDocCatalog;
    SkTSet<SkPDFObject*>* fFirstPageResources;
    SkTSet<SkPDFObject*>* fOtherPageResources;
    SkTDArray<SkPDFObject*> fSubstitutes;

    SkPDFDict* fTrailerDict;

    /** Output the PDF header to the passed stream.
     *  @param stream    The writable output stream to send the header to.
     */
    void emitHeader(SkWStream* stream);

    /** Get the size of the header.
     */
    size_t headerSize();

    /** Output the PDF footer to the passed stream.
     *  @param stream    The writable output stream to send the footer to.
     *  @param objCount  The number of objects in the PDF.
     */
    void emitFooter(SkWStream* stream, int64_t objCount);
};

#endif
