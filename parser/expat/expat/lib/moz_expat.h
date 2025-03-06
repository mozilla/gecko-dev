/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_EXPAT_H_
#define MOZ_EXPAT_H_

#include "expat_config.h"
#include "expat.h"
#include "mozilla/Types.h"

MOZ_BEGIN_EXTERN_C

void
MOZ_XML_SetXmlDeclHandler(XML_Parser parser,
                          XML_XmlDeclHandler xmldecl);

XML_Parser
MOZ_XML_ParserCreate_MM(const XML_Char *encoding,
                        const XML_Memory_Handling_Suite *memsuite,
                        const XML_Char *namespaceSeparator);

void
MOZ_XML_SetElementHandler(XML_Parser parser,
                          XML_StartElementHandler start,
                          XML_EndElementHandler end);

void
MOZ_XML_SetCharacterDataHandler(XML_Parser parser,
                                XML_CharacterDataHandler handler);

void
MOZ_XML_SetProcessingInstructionHandler(XML_Parser parser,
                                        XML_ProcessingInstructionHandler handler);

void
MOZ_XML_SetCommentHandler(XML_Parser parser,
                          XML_CommentHandler handler);

void
MOZ_XML_SetCdataSectionHandler(XML_Parser parser,
                               XML_StartCdataSectionHandler start,
                               XML_EndCdataSectionHandler end);

void
MOZ_XML_SetDefaultHandlerExpand(XML_Parser parser,
                                XML_DefaultHandler handler);

void
MOZ_XML_SetDoctypeDeclHandler(XML_Parser parser,
                              XML_StartDoctypeDeclHandler start,
                              XML_EndDoctypeDeclHandler end);

void
MOZ_XML_SetExternalEntityRefHandler(XML_Parser parser,
                                    XML_ExternalEntityRefHandler handler);

void
MOZ_XML_SetReturnNSTriplet(XML_Parser parser, int do_nst);

enum XML_Status
MOZ_XML_SetBase(XML_Parser parser, const XML_Char *base);

const XML_Char *
MOZ_XML_GetBase(XML_Parser parser);

int
MOZ_XML_GetSpecifiedAttributeCount(XML_Parser parser);

enum XML_Status
MOZ_XML_Parse(XML_Parser parser, const char *s, int len, int isFinal);

enum XML_Status
MOZ_XML_StopParser(XML_Parser parser, int resumable);

enum XML_Status
MOZ_XML_ResumeParser(XML_Parser parser);

XML_Parser
MOZ_XML_ExternalEntityParserCreate(XML_Parser parser,
                                   const XML_Char *context,
                                   const XML_Char *encoding);

int
MOZ_XML_SetParamEntityParsing(XML_Parser parser,
                              enum XML_ParamEntityParsing parsing);

int
MOZ_XML_SetHashSalt(XML_Parser parser, unsigned long hash_salt);

enum XML_Error
MOZ_XML_GetErrorCode(XML_Parser parser);

XML_Size MOZ_XML_GetCurrentLineNumber(XML_Parser parser);

XML_Size MOZ_XML_GetCurrentColumnNumber(XML_Parser parser);

XML_Index MOZ_XML_GetCurrentByteIndex(XML_Parser parser);

void
MOZ_XML_ParserFree(XML_Parser parser);

XML_Bool MOZ_XML_SetReparseDeferralEnabled(XML_Parser parser, int enabled);

// Mozilla-only API: Report opening tag of mismatched closing tag.
const XML_Char*
MOZ_XML_GetMismatchedTag(XML_Parser parser);

// Mozilla-only API: Report whether the parser is currently expanding an entity.
XML_Bool
MOZ_XML_ProcessingEntityValue(XML_Parser parser);

MOZ_END_EXTERN_C

#endif /* MOZ_EXPAT_H_ */
