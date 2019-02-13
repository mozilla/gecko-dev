/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=80:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_jsipc_JavaScriptChild_h_
#define mozilla_jsipc_JavaScriptChild_h_

#include "JavaScriptBase.h"
#include "mozilla/jsipc/PJavaScriptChild.h"

namespace mozilla {
namespace jsipc {

class JavaScriptChild : public JavaScriptBase<PJavaScriptChild>
{
  public:
    explicit JavaScriptChild(JSRuntime* rt);
    virtual ~JavaScriptChild();

    bool init();
    void updateWeakPointers();

    void drop(JSObject* obj);

  protected:
    virtual bool isParent() override { return false; }
    virtual JSObject* scopeForTargetObjects() override;

  private:
    bool fail(JSContext* cx, ReturnStatus* rs);
    bool ok(ReturnStatus* rs);
};

} // mozilla
} // jsipc

#endif
