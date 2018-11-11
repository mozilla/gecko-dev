/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FILE_DESCRIPTOR_SHUFFLE_H_
#define BASE_FILE_DESCRIPTOR_SHUFFLE_H_

#include "mozilla/Attributes.h"

// This code exists to perform the shuffling of file descriptors which is
// commonly needed when forking subprocesses. The naive approve is very simple,
// just call dup2 to setup the desired descriptors, but wrong. It's tough to
// handle the edge cases (like mapping 0 -> 1, 1 -> 0) correctly.
//
// In order to unittest this code, it's broken into the abstract action (an
// injective multimap) and the concrete code for dealing with file descriptors.
// Users should use the code like this:
//   base::InjectiveMultimap file_descriptor_map;
//   file_descriptor_map.push_back(base::InjectionArc(devnull, 0, true));
//   file_descriptor_map.push_back(base::InjectionArc(devnull, 2, true));
//   file_descriptor_map.push_back(base::InjectionArc(pipe[1], 1, true));
//   base::ShuffleFileDescriptors(file_descriptor_map);
//
// and trust the the Right Thing will get done.

#include <vector>

namespace base {

// A Delegate which performs the actions required to perform an injective
// multimapping in place.
class InjectionDelegate {
 public:
  // Duplicate |fd|, an element of the domain, and write a fresh element of the
  // domain into |result|. Returns true iff successful.
  virtual bool Duplicate(int* result, int fd) = 0;
  // Destructively move |src| to |dest|, overwriting |dest|. Returns true iff
  // successful.
  virtual bool Move(int src, int dest) = 0;
  // Delete an element of the domain.
  virtual void Close(int fd) = 0;
};

// An implementation of the InjectionDelegate interface using the file
// descriptor table of the current process as the domain.
class FileDescriptorTableInjection : public InjectionDelegate {
  virtual bool Duplicate(int* result, int fd) override;
  virtual bool Move(int src, int dest) override;
  virtual void Close(int fd) override;
};

// A single arc of the directed graph which describes an injective multimapping.
struct InjectionArc {
  InjectionArc(int in_source, int in_dest, bool in_close)
      : source(in_source),
        dest(in_dest),
        close(in_close) {
  }

  int source;
  int dest;
  bool close;  // if true, delete the source element after performing the
               // mapping.
};

typedef std::vector<InjectionArc> InjectiveMultimap;

bool PerformInjectiveMultimap(const InjectiveMultimap& map,
                              InjectionDelegate* delegate);
bool PerformInjectiveMultimapDestructive(InjectiveMultimap* map,
                                         InjectionDelegate* delegate);

// This function will not call malloc but will mutate |map|
static inline bool ShuffleFileDescriptors(InjectiveMultimap *map) {
  FileDescriptorTableInjection delegate;
  return PerformInjectiveMultimapDestructive(map, &delegate);
}

}  // namespace base

#endif  // !BASE_FILE_DESCRIPTOR_SHUFFLE_H_
