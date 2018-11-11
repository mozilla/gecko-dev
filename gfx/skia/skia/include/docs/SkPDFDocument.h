// Copyright 2018 Google LLC.
// Use of this source code is governed by a BSD-style license that can be found in the LICENSE file.
#ifndef SkPDFDocument_DEFINED
#define SkPDFDocument_DEFINED

#include "SkDocument.h"

#include "SkScalar.h"
#include "SkString.h"
#include "SkTime.h"

namespace SkPDF {

/** Table 333 in PDF 32000-1:2008
*/
enum class DocumentStructureType {
    kDocument,
    kPart,
    kArt,         // Article
    kSect,        // Section
    kDiv,
    kBlockQuote,
    kCaption,
    kTOC,         // Table of Contents
    kTOCI,        // Table of Contents Item
    kIndex,
    kNonStruct,
    kPrivate,
    kH,           // Heading
    kH1,          // Heading level 1
    kH2,
    kH3,
    kH4,
    kH5,
    kH6,          // Heading level 6
    kP,           // Paragraph
    kL,           // List
    kLI,          // List item
    kLbl,         // List item label
    kLBody,       // List item body
    kTable,
    kTR,
    kTH,
    kTD,
    kTHead,
    kTBody,
    kTFoot,
    kSpan,
    kQuote,
    kNote,
    kReference,
    kBibEntry,
    kCode,
    kLink,
    kAnnot,
    kRuby,
    kWarichu,
    kFigure,
    kFormula,
    kForm,        // Form control (not like an HTML FORM element)
};

/**
 *  A node in a PDF structure tree, giving a semantic representation
 *  of the content.  Each node ID is associated with content
 *  by passing the SkCanvas and node ID to SkPDF::SetNodeId() when drawing.
 */
struct StructureElementNode {
    const StructureElementNode* fChildren = nullptr;
    size_t fChildCount;
    int fNodeId;
    DocumentStructureType fType;
};

/** Optional metadata to be passed into the PDF factory function.
*/
struct Metadata {
    /** The document's title.
    */
    SkString fTitle;

    /** The name of the person who created the document.
    */
    SkString fAuthor;

    /** The subject of the document.
    */
    SkString fSubject;

    /** Keywords associated with the document.  Commas may be used to delineate
        keywords within the string.
    */
    SkString fKeywords;

    /** If the document was converted to PDF from another format,
        the name of the conforming product that created the
        original document from which it was converted.
    */
    SkString fCreator;

    /** The product that is converting this document to PDF.
        Leave fProducer empty to get the default, correct value.
    */
    SkString fProducer;

    /** The date and time the document was created.
        The zero default value represents an unknown/unset time.
    */
    SkTime::DateTime fCreation = {0, 0, 0, 0, 0, 0, 0, 0};

    /** The date and time the document was most recently modified.
        The zero default value represents an unknown/unset time.
    */
    SkTime::DateTime fModified = {0, 0, 0, 0, 0, 0, 0, 0};

    /** The DPI (pixels-per-inch) at which features without native PDF support
        will be rasterized (e.g. draw image with perspective, draw text with
        perspective, ...)  A larger DPI would create a PDF that reflects the
        original intent with better fidelity, but it can make for larger PDF
        files too, which would use more memory while rendering, and it would be
        slower to be processed or sent online or to printer.
    */
    SkScalar fRasterDPI = SK_ScalarDefaultRasterDPI;

    /** If true, include XMP metadata, a document UUID, and sRGB output intent
        information.  This adds length to the document and makes it
        non-reproducable, but are necessary features for PDF/A-2b conformance
    */
    bool fPDFA = false;

    /** Encoding quality controls the trade-off between size and quality. By
        default this is set to 101 percent, which corresponds to lossless
        encoding. If this value is set to a value <= 100, and the image is
        opaque, it will be encoded (using JPEG) with that quality setting.
    */
    int fEncodingQuality = 101;

    /**
     *  An optional tree of structured document tags that provide
     *  a semantic representation of the content. The caller
     *  should retain ownership.
     */
    const StructureElementNode* fStructureElementTreeRoot = nullptr;
};

/** Associate a node ID with subsequent drawing commands in an
    SkCanvas.  The same node ID can appear in a StructureElementNode
    in order to associate a document's structure element tree with
    its content.

    A node ID of zero indicates no node ID.

    @param canvas  The canvas used to draw to the PDF.
    @param nodeId  The node ID for subsequent drawing commands.
*/
SK_API void SetNodeId(SkCanvas* dst, int nodeID);

/** Create a PDF-backed document, writing the results into a SkWStream.

    PDF pages are sized in point units. 1 pt == 1/72 inch == 127/360 mm.

    @param stream A PDF document will be written to this stream.  The document may write
           to the stream at anytime during its lifetime, until either close() is
           called or the document is deleted.
    @param metadata a PDFmetadata object.  Any fields may be left empty.

    @returns NULL if there is an error, otherwise a newly created PDF-backed SkDocument.
*/
SK_API sk_sp<SkDocument> MakeDocument(SkWStream* stream, const Metadata& metadata);

static inline sk_sp<SkDocument> MakeDocument(SkWStream* stream) {
    return MakeDocument(stream, Metadata());
}

}  // namespace SkPDF
#endif  // SkPDFDocument_DEFINED
