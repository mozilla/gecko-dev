/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/Activation-inl.h"

#include "mozilla/Assertions.h"  // MOZ_ASSERT

#include "vm/JSContext.h"  // JSContext, js::TlsContext

using namespace js;

void Activation::registerProfiling() {
  MOZ_ASSERT(isProfiling());
  cx_->profilingActivation_ = this;
}

void Activation::unregisterProfiling() {
  MOZ_ASSERT(isProfiling());
  MOZ_ASSERT(cx_->profilingActivation_ == this);
  cx_->profilingActivation_ = prevProfiling_;
}

ActivationIterator::ActivationIterator(JSContext* cx)
    : activation_(cx->activation_) {
  MOZ_ASSERT(cx == TlsContext.get());
}

ActivationIterator& ActivationIterator::operator++() {
  MOZ_ASSERT(activation_);
  activation_ = activation_->prev();
  return *this;
}
