/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_loader_LoadedScript_h
#define js_loader_LoadedScript_h

#include "js/AllocPolicy.h"
#include "js/experimental/JSStencil.h"
#include "js/Transcoding.h"

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Maybe.h"
#include "mozilla/MaybeOneOf.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Utf8.h"  // mozilla::Utf8Unit
#include "mozilla/Variant.h"
#include "mozilla/Vector.h"

#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIMemoryReporter.h"

#include "jsapi.h"
#include "ScriptKind.h"
#include "ScriptFetchOptions.h"

class nsIURI;

namespace JS::loader {

class ScriptLoadRequest;

using Utf8Unit = mozilla::Utf8Unit;

void HostAddRefTopLevelScript(const JS::Value& aPrivate);
void HostReleaseTopLevelScript(const JS::Value& aPrivate);

class ClassicScript;
class ModuleScript;
class EventScript;
class LoadContextBase;

// A LoadedScript is a place where the Script is stored once it is loaded. It is
// not unique to a load, and can be shared across loads as long as it is
// properly ref-counted by each load instance.
//
// When the load is not performed, the URI represents the resource to be loaded,
// and it is replaced by the absolute resource location once loaded.
//
// As the LoadedScript can be shared, using the SharedSubResourceCache, it is
// exposed to the memory reporter such that sharing might be accounted for
// properly.
class LoadedScript : public nsIMemoryReporter {
  ScriptKind mKind;
  const mozilla::dom::ReferrerPolicy mReferrerPolicy;
  RefPtr<ScriptFetchOptions> mFetchOptions;
  nsCOMPtr<nsIURI> mURI;
  nsCOMPtr<nsIURI> mBaseURL;

 protected:
  LoadedScript(ScriptKind aKind, mozilla::dom::ReferrerPolicy aReferrerPolicy,
               ScriptFetchOptions* aFetchOptions, nsIURI* aURI);

  LoadedScript(const LoadedScript& aOther);

  template <typename T, typename... Args>
  friend RefPtr<T> mozilla::MakeRefPtr(Args&&... aArgs);

  virtual ~LoadedScript();

 public:
  // When the memory should be reported, register it using RegisterMemoryReport,
  // and make sure to call SizeOfIncludingThis in the enclosing container.
  //
  // Each reported script would be listed under
  // `explicit/js/script/loaded-script/<kind>`.
  void RegisterMemoryReport();
  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS;
  NS_DECL_NSIMEMORYREPORTER;
  NS_DECL_CYCLE_COLLECTION_CLASS(LoadedScript)

  bool IsClassicScript() const { return mKind == ScriptKind::eClassic; }
  bool IsModuleScript() const { return mKind == ScriptKind::eModule; }
  bool IsEventScript() const { return mKind == ScriptKind::eEvent; }

  inline ClassicScript* AsClassicScript();
  inline ModuleScript* AsModuleScript();
  inline EventScript* AsEventScript();

  // Used to propagate Fetch Options to child modules
  ScriptFetchOptions* GetFetchOptions() const { return mFetchOptions; }

  mozilla::dom::ReferrerPolicy ReferrerPolicy() const {
    return mReferrerPolicy;
  }

  nsIURI* GetURI() const { return mURI; }
  void SetBaseURL(nsIURI* aBaseURL) {
    MOZ_ASSERT(!mBaseURL);
    mBaseURL = aBaseURL;
  }
  nsIURI* BaseURL() const { return mBaseURL; }

  void AssociateWithScript(JSScript* aScript);

 public:
  // ===========================================================================
  // Encoding of the content provided by the network, or refined by the JS
  // engine.
  template <typename... Ts>
  using Variant = mozilla::Variant<Ts...>;

  template <typename... Ts>
  using VariantType = mozilla::VariantType<Ts...>;

  // Type of data this instance holds, which is either provided by the nsChannel
  // or retrieved from the cache.
  enum class DataType : uint8_t { eUnknown, eTextSource, eBytecode, eStencil };

  // Use a vector backed by the JS allocator for script text so that contents
  // can be transferred in constant time to the JS engine, not copied in linear
  // time.
  template <typename Unit>
  using ScriptTextBuffer = mozilla::Vector<Unit, 0, js::MallocAllocPolicy>;

  using MaybeSourceText =
      mozilla::MaybeOneOf<JS::SourceText<char16_t>, JS::SourceText<Utf8Unit>>;

  bool IsUnknownDataType() const { return mDataType == DataType::eUnknown; }
  bool IsTextSource() const { return mDataType == DataType::eTextSource; }
  bool IsSource() const { return IsTextSource(); }
  bool IsBytecode() const { return mDataType == DataType::eBytecode; }
  bool IsStencil() const { return mDataType == DataType::eStencil; }

  void SetUnknownDataType() {
    mDataType = DataType::eUnknown;
    mScriptData.reset();
  }

  void SetTextSource(LoadContextBase* maybeLoadContext) {
    MOZ_ASSERT(IsUnknownDataType());
    mDataType = DataType::eTextSource;
    mScriptData.emplace(VariantType<ScriptTextBuffer<Utf8Unit>>());
  }

  void SetBytecode() {
    MOZ_ASSERT(IsUnknownDataType());
    mDataType = DataType::eBytecode;
  }

  void SetStencil(already_AddRefed<JS::Stencil> aStencil) {
    SetUnknownDataType();
    mDataType = DataType::eStencil;
    mStencil = aStencil;
  }

  bool IsUTF16Text() const {
    return mScriptData->is<ScriptTextBuffer<char16_t>>();
  }
  bool IsUTF8Text() const {
    return mScriptData->is<ScriptTextBuffer<Utf8Unit>>();
  }

  template <typename Unit>
  const ScriptTextBuffer<Unit>& ScriptText() const {
    MOZ_ASSERT(IsTextSource());
    return mScriptData->as<ScriptTextBuffer<Unit>>();
  }
  template <typename Unit>
  ScriptTextBuffer<Unit>& ScriptText() {
    MOZ_ASSERT(IsTextSource());
    return mScriptData->as<ScriptTextBuffer<Unit>>();
  }

  size_t ScriptTextLength() const {
    MOZ_ASSERT(IsTextSource());
    return IsUTF16Text() ? ScriptText<char16_t>().length()
                         : ScriptText<Utf8Unit>().length();
  }

  // Get source text.  On success |aMaybeSource| will contain either UTF-8 or
  // UTF-16 source; on failure it will remain in its initial state.
  nsresult GetScriptSource(JSContext* aCx, MaybeSourceText* aMaybeSource,
                           LoadContextBase* aMaybeLoadContext);

  void ClearScriptSource() {
    if (IsTextSource()) {
      ClearScriptText();
    }
  }

  void ClearScriptText() {
    MOZ_ASSERT(IsTextSource());
    return IsUTF16Text() ? ScriptText<char16_t>().clearAndFree()
                         : ScriptText<Utf8Unit>().clearAndFree();
  }

  size_t ReceivedScriptTextLength() const { return mReceivedScriptTextLength; }

  void SetReceivedScriptTextLength(size_t aLength) {
    mReceivedScriptTextLength = aLength;
  }

  bool CanHaveBytecode() const {
    return IsBytecode() || IsSource() || IsStencil();
  }

  JS::TranscodeBuffer& SRIAndBytecode() {
    // Note: SRIAndBytecode might be called even if the IsSource() returns true,
    // as we want to be able to save the bytecode content when we are loading
    // from source.
    MOZ_ASSERT(CanHaveBytecode());
    return mScriptBytecode;
  }
  JS::TranscodeRange Bytecode() const {
    MOZ_ASSERT(IsBytecode());
    const auto& bytecode = mScriptBytecode;
    auto offset = mBytecodeOffset;
    return JS::TranscodeRange(bytecode.begin() + offset,
                              bytecode.length() - offset);
  }

  size_t GetSRILength() const {
    MOZ_ASSERT(CanHaveBytecode());
    return mBytecodeOffset;
  }
  void SetSRILength(size_t sriLength) {
    MOZ_ASSERT(CanHaveBytecode());
    mBytecodeOffset = JS::AlignTranscodingBytecodeOffset(sriLength);
  }

  void DropBytecode() {
    MOZ_ASSERT(CanHaveBytecode());
    mScriptBytecode.clearAndFree();
  }

  JS::Stencil* GetStencil() const {
    MOZ_ASSERT(IsStencil());
    return mStencil;
  }

 public:
  // Fields.

  // Determine whether the mScriptData or mScriptBytecode is used.
  DataType mDataType;

  // Holds script source data for non-inline scripts.
  mozilla::Maybe<
      Variant<ScriptTextBuffer<char16_t>, ScriptTextBuffer<Utf8Unit>>>
      mScriptData;

  // The length of script source text, set when reading completes. This is used
  // since mScriptData is cleared when the source is passed to the JS engine.
  size_t mReceivedScriptTextLength;

  // Holds the SRI serialized hash and the script bytecode for non-inline
  // scripts. The data is laid out according to ScriptBytecodeDataLayout
  // or, if compression is enabled, ScriptBytecodeCompressedDataLayout.
  JS::TranscodeBuffer mScriptBytecode;
  uint32_t mBytecodeOffset;  // Offset of the bytecode in mScriptBytecode

  RefPtr<JS::Stencil> mStencil;
};

// Provide accessors for any classes `Derived` which is providing the
// `getLoadedScript` function as interface. The accessors are meant to be
// inherited by the `Derived` class.
template <typename Derived>
class LoadedScriptDelegate {
 private:
  // Use a static_cast<Derived> instead of declaring virtual functions. This is
  // meant to avoid relying on virtual table, and improve inlining for non-final
  // classes.
  const LoadedScript* GetLoadedScript() const {
    return static_cast<const Derived*>(this)->getLoadedScript();
  }
  LoadedScript* GetLoadedScript() {
    return static_cast<Derived*>(this)->getLoadedScript();
  }

 public:
  template <typename Unit>
  using ScriptTextBuffer = LoadedScript::ScriptTextBuffer<Unit>;
  using MaybeSourceText = LoadedScript::MaybeSourceText;

  bool IsModuleScript() const { return GetLoadedScript()->IsModuleScript(); }
  bool IsEventScript() const { return GetLoadedScript()->IsEventScript(); }

  bool IsUnknownDataType() const {
    return GetLoadedScript()->IsUnknownDataType();
  }
  bool IsTextSource() const { return GetLoadedScript()->IsTextSource(); }
  bool IsSource() const { return GetLoadedScript()->IsSource(); }
  bool IsBytecode() const { return GetLoadedScript()->IsBytecode(); }
  bool IsStencil() const { return GetLoadedScript()->IsStencil(); }

  void SetUnknownDataType() { GetLoadedScript()->SetUnknownDataType(); }

  void SetTextSource(LoadContextBase* maybeLoadContext) {
    GetLoadedScript()->SetTextSource(maybeLoadContext);
  }

  void SetBytecode() { GetLoadedScript()->SetBytecode(); }

  void SetStencil(already_AddRefed<JS::Stencil> aStencil) {
    GetLoadedScript()->SetStencil(std::move(aStencil));
  }

  bool IsUTF16Text() const { return GetLoadedScript()->IsUTF16Text(); }
  bool IsUTF8Text() const { return GetLoadedScript()->IsUTF8Text(); }

  template <typename Unit>
  const ScriptTextBuffer<Unit>& ScriptText() const {
    const LoadedScript* loader = GetLoadedScript();
    return loader->ScriptText<Unit>();
  }
  template <typename Unit>
  ScriptTextBuffer<Unit>& ScriptText() {
    LoadedScript* loader = GetLoadedScript();
    return loader->ScriptText<Unit>();
  }

  size_t ScriptTextLength() const {
    return GetLoadedScript()->ScriptTextLength();
  }

  size_t ReceivedScriptTextLength() const {
    return GetLoadedScript()->ReceivedScriptTextLength();
  }

  void SetReceivedScriptTextLength(size_t aLength) {
    GetLoadedScript()->SetReceivedScriptTextLength(aLength);
  }

  // Get source text.  On success |aMaybeSource| will contain either UTF-8 or
  // UTF-16 source; on failure it will remain in its initial state.
  nsresult GetScriptSource(JSContext* aCx, MaybeSourceText* aMaybeSource,
                           LoadContextBase* aLoadContext) {
    return GetLoadedScript()->GetScriptSource(aCx, aMaybeSource, aLoadContext);
  }

  void ClearScriptSource() { GetLoadedScript()->ClearScriptSource(); }

  void ClearScriptText() { GetLoadedScript()->ClearScriptText(); }

  JS::TranscodeBuffer& SRIAndBytecode() {
    return GetLoadedScript()->SRIAndBytecode();
  }
  JS::TranscodeRange Bytecode() const { return GetLoadedScript()->Bytecode(); }

  size_t GetSRILength() const { return GetLoadedScript()->GetSRILength(); }
  void SetSRILength(size_t sriLength) {
    GetLoadedScript()->SetSRILength(sriLength);
  }

  void DropBytecode() { GetLoadedScript()->DropBytecode(); }

  JS::Stencil* GetStencil() const { return GetLoadedScript()->GetStencil(); }
};

class ClassicScript final : public LoadedScript {
  ~ClassicScript() = default;

 private:
  // Scripts can be created only by ScriptLoadRequest::NoCacheEntryFound.
  ClassicScript(mozilla::dom::ReferrerPolicy aReferrerPolicy,
                ScriptFetchOptions* aFetchOptions, nsIURI* aURI);

  friend class ScriptLoadRequest;
};

class EventScript final : public LoadedScript {
  ~EventScript() = default;

 public:
  EventScript(mozilla::dom::ReferrerPolicy aReferrerPolicy,
              ScriptFetchOptions* aFetchOptions, nsIURI* aURI);
};

// A single module script. May be used to satisfy multiple load requests.

class ModuleScript final : public LoadedScript {
  // Those fields are used only after instantiated, and they're reset to
  // null and false when stored into the cache as LoadedScript instance.
  JS::Heap<JSObject*> mModuleRecord;
  JS::Heap<JS::Value> mParseError;
  JS::Heap<JS::Value> mErrorToRethrow;
  bool mForPreload = false;
  bool mHadImportMap = false;
  bool mDebuggerDataInitialized = false;

  ~ModuleScript();

 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(ModuleScript,
                                                         LoadedScript)

 private:
  // Scripts can be created only by ScriptLoadRequest::NoCacheEntryFound.
  ModuleScript(mozilla::dom::ReferrerPolicy aReferrerPolicy,
               ScriptFetchOptions* aFetchOptions, nsIURI* aURI);

  explicit ModuleScript(const LoadedScript& other);

  template <typename T, typename... Args>
  friend RefPtr<T> mozilla::MakeRefPtr(Args&&... aArgs);

  friend class ScriptLoadRequest;

 public:
  // Convert between cacheable LoadedScript instance, which is used by
  // mozilla::dom::SharedScriptCache.
  static already_AddRefed<ModuleScript> FromCache(const LoadedScript& aScript);
  already_AddRefed<LoadedScript> ToCache();

  void SetModuleRecord(JS::Handle<JSObject*> aModuleRecord);
  void SetParseError(const JS::Value& aError);
  void SetErrorToRethrow(const JS::Value& aError);
  void SetForPreload(bool aValue);
  void SetHadImportMap(bool aValue);
  void SetDebuggerDataInitialized();

  JSObject* ModuleRecord() const { return mModuleRecord; }

  JS::Value ParseError() const { return mParseError; }
  JS::Value ErrorToRethrow() const { return mErrorToRethrow; }
  bool HasParseError() const { return !mParseError.isUndefined(); }
  bool HasErrorToRethrow() const { return !mErrorToRethrow.isUndefined(); }
  bool ForPreload() const { return mForPreload; }
  bool HadImportMap() const { return mHadImportMap; }
  bool DebuggerDataInitialized() const { return mDebuggerDataInitialized; }

  void Shutdown();

  void UnlinkModuleRecord();

  friend void CheckModuleScriptPrivate(LoadedScript*, const JS::Value&);
};

ClassicScript* LoadedScript::AsClassicScript() {
  MOZ_ASSERT(!IsModuleScript());
  return static_cast<ClassicScript*>(this);
}

ModuleScript* LoadedScript::AsModuleScript() {
  MOZ_ASSERT(IsModuleScript());
  return static_cast<ModuleScript*>(this);
}

}  // namespace JS::loader

#endif  // js_loader_LoadedScript_h
