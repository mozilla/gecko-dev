/*
*******************************************************************************
* Copyright (C) 2013, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* dictionarydata.h
*
* created on: 2012may31
* created by: Markus W. Scherer & Maxime Serrano
*/

#include "dictionarydata.h"
#include "unicode/ucharstrie.h"
#include "unicode/bytestrie.h"
#include "unicode/udata.h"
#include "cmemory.h"

#if !UCONFIG_NO_BREAK_ITERATION

U_NAMESPACE_BEGIN

const int32_t  DictionaryData::TRIE_TYPE_BYTES = 0;
const int32_t  DictionaryData::TRIE_TYPE_UCHARS = 1;
const int32_t  DictionaryData::TRIE_TYPE_MASK = 7;
const int32_t  DictionaryData::TRIE_HAS_VALUES = 8;

const int32_t  DictionaryData::TRANSFORM_NONE = 0;
const int32_t  DictionaryData::TRANSFORM_TYPE_OFFSET = 0x1000000;
const int32_t  DictionaryData::TRANSFORM_TYPE_MASK = 0x7f000000;
const int32_t  DictionaryData::TRANSFORM_OFFSET_MASK = 0x1fffff;
    
DictionaryMatcher::~DictionaryMatcher() {
}

UCharsDictionaryMatcher::~UCharsDictionaryMatcher() {
    udata_close(file);
}

int32_t UCharsDictionaryMatcher::getType() const {
    return DictionaryData::TRIE_TYPE_UCHARS;
}

int32_t UCharsDictionaryMatcher::matches(UText *text, int32_t maxLength, int32_t *lengths, int32_t &count, int32_t limit, int32_t *values) const {
    UCharsTrie uct(characters);
    UChar32 c = utext_next32(text);
    if (c < 0) {
        return 0;
    }
    UStringTrieResult result = uct.first(c);
    int32_t numChars = 1;
    count = 0;
    for (;;) {
        if (USTRINGTRIE_HAS_VALUE(result)) {
            if (count < limit) {
                if (values != NULL) {
                    values[count] = uct.getValue();
                }
                lengths[count++] = numChars;
            }
            if (result == USTRINGTRIE_FINAL_VALUE) {
                break;
            }
        }
        else if (result == USTRINGTRIE_NO_MATCH) {
            break;
        }

        // TODO: why do we have a text limit if the UText knows its length?
        if (numChars >= maxLength) {
            break;
        }

        c = utext_next32(text);
        if (c < 0) {
            break;
        }
        ++numChars;
        result = uct.next(c);
    }
    return numChars;
}

BytesDictionaryMatcher::~BytesDictionaryMatcher() {
    udata_close(file);
}

UChar32 BytesDictionaryMatcher::transform(UChar32 c) const {
    if ((transformConstant & DictionaryData::TRANSFORM_TYPE_MASK) == DictionaryData::TRANSFORM_TYPE_OFFSET) {
        if (c == 0x200D) {
            return 0xFF;
        } else if (c == 0x200C) {
            return 0xFE;
        }
        int32_t delta = c - (transformConstant & DictionaryData::TRANSFORM_OFFSET_MASK);
        if (delta < 0 || 0xFD < delta) {
            return U_SENTINEL;
        }
        return (UChar32)delta;
    }
    return c;
}

int32_t BytesDictionaryMatcher::getType() const {
    return DictionaryData::TRIE_TYPE_BYTES;
}

int32_t BytesDictionaryMatcher::matches(UText *text, int32_t maxLength, int32_t *lengths, int32_t &count, int32_t limit, int32_t *values) const {
    BytesTrie bt(characters);
    UChar32 c = utext_next32(text);
    if (c < 0) {
        return 0;
    }
    UStringTrieResult result = bt.first(transform(c));
    int32_t numChars = 1;
    count = 0;
    for (;;) {
        if (USTRINGTRIE_HAS_VALUE(result)) {
            if (count < limit) {
                if (values != NULL) {
                    values[count] = bt.getValue();
            }
                lengths[count++] = numChars;
            }
            if (result == USTRINGTRIE_FINAL_VALUE) {
                break;
            }
        }
        else if (result == USTRINGTRIE_NO_MATCH) {
            break;
        }

        // TODO: why do we have a text limit if the UText knows its length?
        if (numChars >= maxLength) {
            break;
        }

        c = utext_next32(text);
        if (c < 0) {
            break;
        }
        ++numChars;
        result = bt.next(transform(c));
    }
    return numChars;
}


U_NAMESPACE_END

U_NAMESPACE_USE

U_CAPI int32_t U_EXPORT2
udict_swap(const UDataSwapper *ds, const void *inData, int32_t length,
           void *outData, UErrorCode *pErrorCode) {
    const UDataInfo *pInfo;
    int32_t headerSize;
    const uint8_t *inBytes;
    uint8_t *outBytes;
    const int32_t *inIndexes;
    int32_t indexes[DictionaryData::IX_COUNT];
    int32_t i, offset, size;

    headerSize = udata_swapDataHeader(ds, inData, length, outData, pErrorCode);
    if (pErrorCode == NULL || U_FAILURE(*pErrorCode)) return 0;
    pInfo = (const UDataInfo *)((const char *)inData + 4);
    if (!(pInfo->dataFormat[0] == 0x44 && 
          pInfo->dataFormat[1] == 0x69 && 
          pInfo->dataFormat[2] == 0x63 && 
          pInfo->dataFormat[3] == 0x74 && 
          pInfo->formatVersion[0] == 1)) {
        udata_printError(ds, "udict_swap(): data format %02x.%02x.%02x.%02x (format version %02x) is not recognized as dictionary data\n",
                         pInfo->dataFormat[0], pInfo->dataFormat[1], pInfo->dataFormat[2], pInfo->dataFormat[3], pInfo->formatVersion[0]);
        *pErrorCode = U_UNSUPPORTED_ERROR;
        return 0;
    }

    inBytes = (const uint8_t *)inData + headerSize;
    outBytes = (uint8_t *)outData + headerSize;

    inIndexes = (const int32_t *)inBytes;
    if (length >= 0) {
        length -= headerSize;
        if (length < (int32_t)(sizeof(indexes))) {
            udata_printError(ds, "udict_swap(): too few bytes (%d after header) for dictionary data\n", length);
            *pErrorCode = U_INDEX_OUTOFBOUNDS_ERROR;
            return 0;
        }
    }

    for (i = 0; i < DictionaryData::IX_COUNT; i++) {
        indexes[i] = udata_readInt32(ds, inIndexes[i]);
    }

    size = indexes[DictionaryData::IX_TOTAL_SIZE];

    if (length >= 0) {
        if (length < size) {
            udata_printError(ds, "udict_swap(): too few bytes (%d after header) for all of dictionary data\n", length);
            *pErrorCode = U_INDEX_OUTOFBOUNDS_ERROR;
            return 0;
        }

        if (inBytes != outBytes) {
            uprv_memcpy(outBytes, inBytes, size);
        }

        offset = 0;
        ds->swapArray32(ds, inBytes, sizeof(indexes), outBytes, pErrorCode);
        offset = (int32_t)sizeof(indexes);
        int32_t trieType = indexes[DictionaryData::IX_TRIE_TYPE] & DictionaryData::TRIE_TYPE_MASK;
        int32_t nextOffset = indexes[DictionaryData::IX_RESERVED1_OFFSET];

        if (trieType == DictionaryData::TRIE_TYPE_UCHARS) {
            ds->swapArray16(ds, inBytes + offset, nextOffset - offset, outBytes + offset, pErrorCode);
        } else if (trieType == DictionaryData::TRIE_TYPE_BYTES) {
            // nothing to do
        } else {
            udata_printError(ds, "udict_swap(): unknown trie type!\n");
            *pErrorCode = U_UNSUPPORTED_ERROR;
            return 0;
        }

        // these next two sections are empty in the current format,
        // but may be used later.
        offset = nextOffset;
        nextOffset = indexes[DictionaryData::IX_RESERVED2_OFFSET];
        offset = nextOffset;
        nextOffset = indexes[DictionaryData::IX_TOTAL_SIZE];
        offset = nextOffset;
    }
    return headerSize + size;
}
#endif
