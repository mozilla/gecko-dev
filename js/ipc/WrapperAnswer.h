/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=80:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_jsipc_WrapperAnswer_h_
#define mozilla_jsipc_WrapperAnswer_h_

#include "JavaScriptShared.h"

namespace mozilla {
namespace jsipc {

class WrapperAnswer : public virtual JavaScriptShared
{
  public:
    WrapperAnswer(JSRuntime *rt) : JavaScriptShared(rt) {}

    bool AnswerPreventExtensions(const ObjectId &objId, ReturnStatus *rs);
    bool AnswerGetPropertyDescriptor(const ObjectId &objId, const nsString &id,
                                     ReturnStatus *rs,
                                     PPropertyDescriptor *out);
    bool AnswerGetOwnPropertyDescriptor(const ObjectId &objId,
                                        const nsString &id,
                                        ReturnStatus *rs,
                                        PPropertyDescriptor *out);
    bool AnswerDefineProperty(const ObjectId &objId, const nsString &id,
                              const PPropertyDescriptor &flags,
                              ReturnStatus *rs);
    bool AnswerDelete(const ObjectId &objId, const nsString &id,
                      ReturnStatus *rs, bool *success);

    bool AnswerHas(const ObjectId &objId, const nsString &id,
                       ReturnStatus *rs, bool *bp);
    bool AnswerHasOwn(const ObjectId &objId, const nsString &id,
                          ReturnStatus *rs, bool *bp);
    bool AnswerGet(const ObjectId &objId, const ObjectId &receiverId,
                       const nsString &id,
                       ReturnStatus *rs, JSVariant *result);
    bool AnswerSet(const ObjectId &objId, const ObjectId &receiverId,
                   const nsString &id, const bool &strict,
                   const JSVariant &value, ReturnStatus *rs, JSVariant *result);

    bool AnswerIsExtensible(const ObjectId &objId, ReturnStatus *rs,
                            bool *result);
    bool AnswerCall(const ObjectId &objId, const nsTArray<JSParam> &argv,
                    ReturnStatus *rs, JSVariant *result,
                    nsTArray<JSParam> *outparams);
    bool AnswerObjectClassIs(const ObjectId &objId, const uint32_t &classValue,
                             bool *result);
    bool AnswerClassName(const ObjectId &objId, nsString *result);

    bool AnswerGetPropertyNames(const ObjectId &objId, const uint32_t &flags,
                                ReturnStatus *rs, nsTArray<nsString> *names);
    bool AnswerInstanceOf(const ObjectId &objId, const JSIID &iid,
                          ReturnStatus *rs, bool *instanceof);
    bool AnswerDOMInstanceOf(const ObjectId &objId, const int &prototypeID, const int &depth,
                             ReturnStatus *rs, bool *instanceof);

    bool RecvDropObject(const ObjectId &objId);

  private:
    bool fail(JSContext *cx, ReturnStatus *rs);
    bool ok(ReturnStatus *rs);
};

} // mozilla
} // jsipc

#endif
