/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "moz_expat.h"
#include "xmlparse.c"

void
MOZ_XML_SetXmlDeclHandler(XML_Parser parser,
                          XML_XmlDeclHandler xmldecl) {
  return XML_SetXmlDeclHandler(parser, xmldecl);
}

XML_Parser
MOZ_XML_ParserCreate_MM(const XML_Char *encoding,
                        const XML_Memory_Handling_Suite *memsuite,
                        const XML_Char *namespaceSeparator) {
  return XML_ParserCreate_MM(encoding, memsuite, namespaceSeparator);
}

void
MOZ_XML_SetElementHandler(XML_Parser parser,
                          XML_StartElementHandler start,
                          XML_EndElementHandler end) {
  XML_SetElementHandler(parser, start, end);
}

void
MOZ_XML_SetCharacterDataHandler(XML_Parser parser,
                                XML_CharacterDataHandler handler) {
  return XML_SetCharacterDataHandler(parser, handler);
}

void
MOZ_XML_SetProcessingInstructionHandler(XML_Parser parser,
                                        XML_ProcessingInstructionHandler handler) {
  return XML_SetProcessingInstructionHandler(parser, handler);
}

void
MOZ_XML_SetCommentHandler(XML_Parser parser,
                          XML_CommentHandler handler) {
  XML_SetCommentHandler(parser, handler);
}

void
MOZ_XML_SetCdataSectionHandler(XML_Parser parser,
                               XML_StartCdataSectionHandler start,
                               XML_EndCdataSectionHandler end) {
  XML_SetCdataSectionHandler(parser, start, end);
}

void
MOZ_XML_SetDefaultHandlerExpand(XML_Parser parser,
                                XML_DefaultHandler handler) {
  XML_SetDefaultHandlerExpand(parser, handler);
}

void
MOZ_XML_SetDoctypeDeclHandler(XML_Parser parser,
                              XML_StartDoctypeDeclHandler start,
                              XML_EndDoctypeDeclHandler end) {
  XML_SetDoctypeDeclHandler(parser, start, end);
}

void
MOZ_XML_SetExternalEntityRefHandler(XML_Parser parser,
                                    XML_ExternalEntityRefHandler handler) {
  XML_SetExternalEntityRefHandler(parser, handler);
}

void
MOZ_XML_SetReturnNSTriplet(XML_Parser parser, int do_nst) {
  XML_SetReturnNSTriplet(parser, do_nst);
}

enum XML_Status
MOZ_XML_SetBase(XML_Parser parser, const XML_Char *base) {
  return XML_SetBase(parser, base);
}

const XML_Char *
MOZ_XML_GetBase(XML_Parser parser) {
  return XML_GetBase(parser);
}

int
MOZ_XML_GetSpecifiedAttributeCount(XML_Parser parser) {
  return XML_GetSpecifiedAttributeCount(parser);
}

enum XML_Status
MOZ_XML_Parse(XML_Parser parser, const char *s, int len, int isFinal) {
  return XML_Parse(parser, s, len, isFinal);
}

enum XML_Status
MOZ_XML_StopParser(XML_Parser parser, int resumable) {
  return XML_StopParser(parser, resumable);
}

enum XML_Status
MOZ_XML_ResumeParser(XML_Parser parser) {
  return XML_ResumeParser(parser);
}

XML_Parser
MOZ_XML_ExternalEntityParserCreate(XML_Parser parser,
                                   const XML_Char *context,
                                   const XML_Char *encoding) {
  return XML_ExternalEntityParserCreate(parser, context, encoding);
}

int
MOZ_XML_SetParamEntityParsing(XML_Parser parser,
                              enum XML_ParamEntityParsing parsing) {
  return XML_SetParamEntityParsing(parser, parsing);
}

int
MOZ_XML_SetHashSalt(XML_Parser parser, unsigned long hash_salt) {
  return XML_SetHashSalt(parser, hash_salt);
}

enum XML_Error
MOZ_XML_GetErrorCode(XML_Parser parser)
{
  return XML_GetErrorCode(parser);
}

XML_Size MOZ_XML_GetCurrentLineNumber(XML_Parser parser) {
  return XML_GetCurrentLineNumber(parser);
}

XML_Size MOZ_XML_GetCurrentColumnNumber(XML_Parser parser) {
  return XML_GetCurrentColumnNumber(parser);
}

XML_Index MOZ_XML_GetCurrentByteIndex(XML_Parser parser) {
  return XML_GetCurrentByteIndex(parser);
}

void
MOZ_XML_ParserFree(XML_Parser parser) {
  return XML_ParserFree(parser);
}

XML_Bool MOZ_XML_SetReparseDeferralEnabled(XML_Parser parser, int enabled) {
  return XML_SetReparseDeferralEnabled(parser, enabled);
}
