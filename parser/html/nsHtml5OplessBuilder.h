/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHtml5OplessBuilder_h
#define nsHtml5OplessBuilder_h

#include "nsHtml5DocumentBuilder.h"
#include "nsTArray.h"

class nsParserBase;

/**
 * This class implements a minimal subclass of nsHtml5DocumentBuilder that
 * works when tree operation queues that are part of the off-the-main-thread
 * parsing machinery are not used and, therefore, nsHtml5TreeOpExecutor is
 * not used.
 *
 * This class is mostly responsible for wrapping tree building in an update
 * batch and resetting various fields in nsContentSink upon finishing.
 */
class nsHtml5OplessBuilder : public nsHtml5DocumentBuilder {
 public:
  nsHtml5OplessBuilder();
  ~nsHtml5OplessBuilder();
  void Start();
  void Finish();
  void SetParser(nsParserBase* aParser);

 private:
  // Speedometer 3.1 first goes to 600 and then
  // right at the end of the run to 1461 and 17232.
  // Let's round 600 up to the next power of two,
  // which results in 1024, but then in order to actually
  // get a power-of-two allocation, we need to take
  // the size of `nsTArrayHeader` into account.
  const size_t kRecyclableLength =
      ((1024 * sizeof(size_t)) - sizeof(nsTArrayHeader)) / sizeof(size_t);
};

#endif  // nsHtml5OplessBuilder_h
