/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_INFERENCESESSION_H_
#define DOM_INFERENCESESSION_H_

#include "js/TypeDecls.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/IOUtilsBinding.h"
#include "mozilla/dom/ONNXBinding.h"
#include "nsISupports.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "mozilla/dom/Record.h"
#include "nsIGlobalObject.h"
#include "onnxruntime_c_api.h"

namespace mozilla::dom {
OrtApi* GetOrtAPI();
struct InferenceSessionRunOptions;
class Promise;
class Tensor;

class InferenceSession final : public nsISupports, public nsWrapperCache {
 public:
  explicit InferenceSession(GlobalObject& aGlobal) {
    nsCOMPtr<nsIGlobalObject> global =
        do_QueryInterface(aGlobal.GetAsSupports());
    mGlobal = global;
    mCtx = aGlobal.Context();
  }

  static bool InInferenceProcess(JSContext*, JSObject*);

 protected:
  virtual ~InferenceSession() { Destroy(); }

 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS;
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(InferenceSession);

  static RefPtr<Promise> Create(GlobalObject& aGlobal,
                                const UTF8StringOrUint8Array& aUriOrBuffer,
                                const InferenceSessionSessionOptions& aOptions,
                                ErrorResult& aRv);

  void Init(const RefPtr<Promise>& aPromise,
            const UTF8StringOrUint8Array& aUriOrBuffer,
            const InferenceSessionSessionOptions& aOptions);

  nsIGlobalObject* GetParentObject() const { return mGlobal; };

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // Return a raw pointer here to avoid refcounting, but make sure it's safe
  // (the object should be kept alive by the callee).
  already_AddRefed<Promise> Run(
      const Record<nsCString, OwningNonNull<Tensor>>& feeds,
      const InferenceSessionRunOptions& options, ErrorResult& aRv);


  void Destroy();

  // This implements "release()" in the JS API but needs to be renamed to
  // avoid collliding with our AddRef/Release methods.
  already_AddRefed<Promise> ReleaseSession();

  void StartProfiling();

  void EndProfiling();

  void GetInputNames(nsTArray<nsCString>& aRetVal) const;

  void GetOutputNames(nsTArray<nsCString>& aRetVal) const;

 protected:
  enum class NameDirection { Input, Output };
  void GetNames(nsTArray<nsCString>& aRetVal,
                NameDirection aDirectionInput) const;
  nsCOMPtr<nsIGlobalObject> mGlobal;
  JSContext* mCtx;
  OrtSessionOptions* mOptions = nullptr;
  OrtSession* mSession = nullptr;
};

}  // namespace mozilla::dom

#endif  // DOM_INFERENCESESSION_H_
