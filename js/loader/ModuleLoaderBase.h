/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_loader_ModuleLoaderBase_h
#define js_loader_ModuleLoaderBase_h

#include "LoadedScript.h"
#include "ScriptLoadRequest.h"

#include "ImportMap.h"
#include "js/ColumnNumber.h"  // JS::ColumnNumberOneOrigin
#include "js/TypeDecls.h"     // JS::MutableHandle, JS::Handle, JS::Root
#include "js/Modules.h"
#include "nsRefPtrHashtable.h"
#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "nsILoadInfo.h"    // nsSecurityFlags
#include "nsINode.h"        // nsIURI
#include "nsThreadUtils.h"  // GetMainThreadSerialEventTarget
#include "nsURIHashKey.h"
#include "mozilla/Attributes.h"  // MOZ_RAII
#include "mozilla/CORSMode.h"
#include "mozilla/MaybeOneOf.h"
#include "mozilla/UniquePtr.h"
#include "ResolveResult.h"

class nsIURI;

namespace mozilla {

class LazyLogModule;
union Utf8Unit;

}  // namespace mozilla

namespace JS {

class CompileOptions;

template <typename UnitT>
class SourceText;

namespace loader {

class ModuleLoaderBase;
class ModuleLoadRequest;
class ModuleScript;

/*
 * [DOMDOC] Shared Classic/Module Script Methods
 *
 * The ScriptLoaderInterface defines the shared methods needed by both
 * ScriptLoaders (loading classic scripts) and ModuleLoaders (loading module
 * scripts). These include:
 *
 *     * Error Logging
 *     * Generating the compile options
 *     * Optional: Bytecode Encoding
 *
 * ScriptLoaderInterface does not provide any implementations.
 * It enables the ModuleLoaderBase to reference back to the behavior implemented
 * by a given ScriptLoader.
 *
 * Not all methods will be used by all ModuleLoaders. For example, Bytecode
 * Encoding does not apply to workers, as we only work with source text there.
 * Fully virtual methods are implemented by all.
 *
 */

class ScriptLoaderInterface : public nsISupports {
 public:
  // Alias common classes.
  using ScriptFetchOptions = JS::loader::ScriptFetchOptions;
  using ScriptKind = JS::loader::ScriptKind;
  using ScriptLoadRequest = JS::loader::ScriptLoadRequest;
  using ScriptLoadRequestList = JS::loader::ScriptLoadRequestList;
  using ModuleLoadRequest = JS::loader::ModuleLoadRequest;

  virtual ~ScriptLoaderInterface() = default;

  // In some environments, we will need to default to a base URI
  virtual nsIURI* GetBaseURI() const = 0;

  virtual void ReportErrorToConsole(ScriptLoadRequest* aRequest,
                                    nsresult aResult) const = 0;

  virtual void ReportWarningToConsole(
      ScriptLoadRequest* aRequest, const char* aMessageName,
      const nsTArray<nsString>& aParams = nsTArray<nsString>()) const = 0;

  // Similar to Report*ToConsole(), only non-null in dom/script/ScriptLoader
  // as we currently only load importmaps there.
  virtual nsIConsoleReportCollector* GetConsoleReportCollector() const {
    return nullptr;
  }

  // Fill in CompileOptions, as well as produce the introducer script for
  // subsequent calls to UpdateDebuggerMetadata
  virtual nsresult FillCompileOptionsForRequest(
      JSContext* cx, ScriptLoadRequest* aRequest, JS::CompileOptions* aOptions,
      JS::MutableHandle<JSScript*> aIntroductionScript) = 0;

  virtual void MaybePrepareModuleForBytecodeEncodingBeforeExecute(
      JSContext* aCx, ModuleLoadRequest* aRequest) {}

  virtual nsresult MaybePrepareModuleForBytecodeEncodingAfterExecute(
      ModuleLoadRequest* aRequest, nsresult aRv) {
    return NS_OK;
  }

  virtual void MaybeTriggerBytecodeEncoding() {}
};

class ModuleMapKey : public PLDHashEntryHdr {
 public:
  using KeyType = const ModuleMapKey&;
  using KeyTypePointer = const ModuleMapKey*;

  ModuleMapKey(const nsIURI* aUri, const ModuleType aModuleType)
      : mUri(const_cast<nsIURI*>(aUri)), mModuleType(aModuleType) {
    MOZ_COUNT_CTOR(ModuleMapKey);
    MOZ_ASSERT(aUri);
  }
  explicit ModuleMapKey(KeyTypePointer aOther)
      : mUri(std::move(aOther->mUri)), mModuleType(aOther->mModuleType) {
    MOZ_COUNT_CTOR(ModuleMapKey);
    MOZ_ASSERT(mUri);
  }
  ModuleMapKey(ModuleMapKey&& aOther)
      : mUri(std::move(aOther.mUri)), mModuleType(aOther.mModuleType) {
    MOZ_COUNT_CTOR(ModuleMapKey);
    MOZ_ASSERT(mUri);
  }
  MOZ_COUNTED_DTOR(ModuleMapKey)

  bool KeyEquals(KeyTypePointer aKey) const {
    if (mModuleType != aKey->mModuleType) {
      return false;
    }

    bool eq;
    if (NS_SUCCEEDED(mUri->Equals(aKey->mUri, &eq))) {
      return eq;
    }

    return false;
  }

  static KeyTypePointer KeyToPointer(KeyType key) { return &key; }

  static PLDHashNumber HashKey(KeyTypePointer aKey) {
    MOZ_ASSERT(aKey->mUri);
    nsAutoCString spec;
    // This is based on `nsURIHashKey`, and it ignores GetSpec() failures, so
    // do the same here.
    (void)aKey->mUri->GetSpec(spec);
    return mozilla::HashGeneric(mozilla::HashString(spec), aKey->mModuleType);
  }

  enum { ALLOW_MEMMOVE = true };

  const nsCOMPtr<nsIURI> mUri;
  const ModuleType mModuleType;
};

/*
 * [DOMDOC] Module Loading
 *
 * ModuleLoaderBase provides support for loading module graphs as defined in the
 * EcmaScript specification. A derived module loader class must be created for a
 * specific use case (for example loading HTML module scripts). The derived
 * class provides operations such as fetching of source code and scheduling of
 * module execution.
 *
 * Module loading works in terms of 'requests' which hold data about modules as
 * they move through the loading process. There may be more than one load
 * request active for a single module URI, but the module is only loaded
 * once. This is achieved by tracking all fetching and fetched modules in the
 * module map.
 *
 * The module map is made up of two parts. A module that has been requested but
 * has not finished fetching is represented by an entry in the mFetchingModules
 * map. A module which has been fetched and compiled is represented by a
 * ModuleScript in the mFetchedModules map.
 *
 * Module loading typically works as follows:
 *
 * 1.  The client ensures there is an instance of the derived module loader
 *     class for its global or creates one if necessary.
 *
 * 2.  The client creates a ModuleLoadRequest object for the module to load and
 *     calls the loader's StartModuleLoad() method. This is a top-level request,
 *     i.e. not an import.
 *
 * 3.  The module loader calls the virtual method CanStartLoad() to check
 *     whether the request should be loaded.
 *
 * 4.  If the module is not already present in the module map, the loader calls
 *     the virtual method StartFetch() to set up an asynchronous operation to
 *     fetch the module source.
 *
 * 5.  When the fetch operation is complete, the derived loader calls
 *     OnFetchComplete() passing an error code to indicate success or failure.
 *
 * 6.  On success, the loader attempts to create a module script by calling the
 *     virtual CompileFetchedModule() method.
 *
 * 7.  If compilation is successful, the loader creates load requests for any
 *     imported modules if present. If so, the process repeats from step 3.
 *
 * 8.  When a load request is completed, the virtual OnModuleLoadComplete()
 *     method is called. This is called for the top-level request and import
 *     requests.
 *
 * 9.  The client calls InstantiateModuleGraph() for the top-level request. This
 *     links the loaded module graph.
 *
 * 10. The client calls EvaluateModule() to execute the top-level module.
 */
class ModuleLoaderBase : public nsISupports {
 public:
  // Alias common classes.
  using LoadedScript = JS::loader::LoadedScript;
  using ScriptFetchOptions = JS::loader::ScriptFetchOptions;
  using ScriptLoadRequest = JS::loader::ScriptLoadRequest;
  using ModuleLoadRequest = JS::loader::ModuleLoadRequest;

 private:
  /*
   * Represents an ongoing load operation for a URI initiated for one request
   * and which may have other requests waiting for it to complete.
   *
   * These are tracked in the mFetchingModules map.
   */
  class LoadingRequest final : public nsISupports {
    ~LoadingRequest() = default;

   public:
    NS_DECL_CYCLE_COLLECTING_ISUPPORTS_FINAL
    NS_DECL_CYCLE_COLLECTION_CLASS(LoadingRequest)

    // The request that initiated the load and which is currently fetching or
    // being compiled.
    RefPtr<ModuleLoadRequest> mRequest;

    // A list of any other requests for the same URI that are waiting for the
    // initial load to complete. These will be resumed by ResumeWaitingRequests
    // when that happens.
    nsTArray<RefPtr<ModuleLoadRequest>> mWaiting;
  };

  // Module map
  nsRefPtrHashtable<ModuleMapKey, LoadingRequest> mFetchingModules;
  nsRefPtrHashtable<ModuleMapKey, ModuleScript> mFetchedModules;

  // List of dynamic imports that are currently being loaded.
  ScriptLoadRequestList mDynamicImportRequests;

  nsCOMPtr<nsIGlobalObject> mGlobalObject;

  // If non-null, this module loader is overridden by the module loader pointed
  // by mOverriddenBy.
  // See ModuleLoaderBase::GetCurrentModuleLoader for more details.
  RefPtr<ModuleLoaderBase> mOverriddenBy;

  // https://html.spec.whatwg.org/multipage/webappapis.html#import-maps-allowed
  //
  // Each Window has an import maps allowed boolean, initially true.
  bool mImportMapsAllowed = true;

 protected:
  RefPtr<ScriptLoaderInterface> mLoader;

  mozilla::UniquePtr<ImportMap> mImportMap;

  virtual ~ModuleLoaderBase();

#ifdef DEBUG
  const ScriptLoadRequestList& DynamicImportRequests() const {
    return mDynamicImportRequests;
  }
#endif

 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(ModuleLoaderBase)
  explicit ModuleLoaderBase(ScriptLoaderInterface* aLoader,
                            nsIGlobalObject* aGlobalObject);

  // Called to break cycles during shutdown to prevent memory leaks.
  void Shutdown();

  virtual nsIURI* GetBaseURI() const { return mLoader->GetBaseURI(); };

  using MaybeSourceText =
      mozilla::MaybeOneOf<JS::SourceText<char16_t>, JS::SourceText<Utf8Unit>>;

  // Methods that must be implemented by an extending class. These are called
  // internally by ModuleLoaderBase.

 private:
  // Create a module load request for a static module import.
  virtual already_AddRefed<ModuleLoadRequest> CreateStaticImport(
      nsIURI* aURI, JS::ModuleType aModuleType, ModuleLoadRequest* aParent,
      const mozilla::dom::SRIMetadata& aSriMetadata) = 0;

  // Called by HostImportModuleDynamically hook.
  virtual already_AddRefed<ModuleLoadRequest> CreateDynamicImport(
      JSContext* aCx, nsIURI* aURI, JS::ModuleType aModuleType,
      LoadedScript* aMaybeActiveScript, JS::Handle<JSString*> aSpecifier,
      JS::Handle<JSObject*> aPromise) = 0;

  virtual bool IsDynamicImportSupported() { return true; }

  // Called when dynamic import started successfully.
  virtual void OnDynamicImportStarted(ModuleLoadRequest* aRequest) {}

  // Check whether we can load a module. May return false with |aRvOut| set to
  // NS_OK to abort load without returning an error.
  virtual bool CanStartLoad(ModuleLoadRequest* aRequest, nsresult* aRvOut) = 0;

  // Start the process of fetching module source (or bytecode). This is only
  // called if CanStartLoad returned true.
  virtual nsresult StartFetch(ModuleLoadRequest* aRequest) = 0;

  // Create a JS module for a fetched module request. This might compile source
  // text or decode cached bytecode.
  virtual nsresult CompileFetchedModule(
      JSContext* aCx, JS::Handle<JSObject*> aGlobal,
      JS::CompileOptions& aOptions, ModuleLoadRequest* aRequest,
      JS::MutableHandle<JSObject*> aModuleOut) = 0;

  // Called when a module script has been loaded, including imports.
  virtual void OnModuleLoadComplete(ModuleLoadRequest* aRequest) = 0;

  virtual bool IsModuleEvaluationAborted(ModuleLoadRequest* aRequest) {
    return false;
  }

  // Get the error message when resolving failed. The default is to call
  // nsContentUtils::FormatLoalizedString. But currently
  // nsContentUtils::FormatLoalizedString cannot be called on a worklet thread,
  // see bug 1808301. So WorkletModuleLoader will override this function to
  // get the error message.
  virtual nsresult GetResolveFailureMessage(ResolveError aError,
                                            const nsAString& aSpecifier,
                                            nsAString& aResult);

  // Public API methods.

 public:
  ScriptLoaderInterface* GetScriptLoaderInterface() const { return mLoader; }

  nsIGlobalObject* GetGlobalObject() const { return mGlobalObject; }

  bool HasFetchingModules() const;

  bool HasPendingDynamicImports() const;
  void CancelDynamicImport(ModuleLoadRequest* aRequest, nsresult aResult);
#ifdef DEBUG
  bool HasDynamicImport(const ModuleLoadRequest* aRequest) const;
#endif

  // Start a load for a module script URI. Returns immediately if the module is
  // already being loaded.
  nsresult StartModuleLoad(ModuleLoadRequest* aRequest);
  nsresult RestartModuleLoad(ModuleLoadRequest* aRequest);

  // Notify the module loader when a fetch started by StartFetch() completes.
  nsresult OnFetchComplete(ModuleLoadRequest* aRequest, nsresult aRv);

  // Link the module and all its imports. This must occur prior to evaluation.
  bool InstantiateModuleGraph(ModuleLoadRequest* aRequest);

  // Executes the module.
  // Implements https://html.spec.whatwg.org/#run-a-module-script
  nsresult EvaluateModule(ModuleLoadRequest* aRequest);

  // Evaluate a module in the given context. Does not push an entry to the
  // execution stack.
  nsresult EvaluateModuleInContext(JSContext* aCx, ModuleLoadRequest* aRequest,
                                   JS::ModuleErrorBehaviour errorBehaviour);

  nsresult StartDynamicImport(ModuleLoadRequest* aRequest);
  void ProcessDynamicImport(ModuleLoadRequest* aRequest);
  void CancelAndClearDynamicImports();

  // Process <script type="importmap">
  mozilla::UniquePtr<ImportMap> ParseImportMap(ScriptLoadRequest* aRequest);

  // Implements
  // https://html.spec.whatwg.org/multipage/webappapis.html#register-an-import-map
  void RegisterImportMap(mozilla::UniquePtr<ImportMap> aImportMap);

  bool HasImportMapRegistered() const { return bool(mImportMap); }

  // Getter for mImportMapsAllowed.
  bool IsImportMapAllowed() const { return mImportMapsAllowed; }
  // https://html.spec.whatwg.org/multipage/webappapis.html#disallow-further-import-maps
  void DisallowImportMaps() { mImportMapsAllowed = false; }

  // Returns whether there has been an entry in the import map
  // for the given aURI.
  bool GetImportMapSRI(nsIURI* aURI, nsIURI* aSourceURI,
                       nsIConsoleReportCollector* aReporter,
                       mozilla::dom::SRIMetadata* aMetadataOut);

  // Returns true if the module for given module key is already fetched.
  bool IsModuleFetched(const ModuleMapKey& key) const;

  nsresult GetFetchedModuleURLs(nsTArray<nsCString>& aURLs);

  // Override the module loader with given loader until ResetOverride is called.
  // While overridden, ModuleLoaderBase::GetCurrentModuleLoader returns aLoader.
  //
  // This is used by mozJSModuleLoader to temporarily override the global's
  // module loader with SyncModuleLoader while importing a module graph
  // synchronously.
  void SetOverride(ModuleLoaderBase* aLoader);

  // Returns true if SetOverride was called.
  bool IsOverridden();

  // Returns true if SetOverride was called with aLoader.
  bool IsOverriddenBy(ModuleLoaderBase* aLoader);

  void ResetOverride();

  // Copy fetched modules to `aDest`.
  // `this` shouldn't have any fetching.
  // `aDest` shouldn't have any fetching or fetched modules.
  //
  // This is used when starting sync module load, to replicate the module cache
  // in the sync module loader pointed by `aDest`.
  void CopyModulesTo(ModuleLoaderBase* aDest);

  // Move all fetched modules to `aDest`.
  // Both `this` and `aDest` shouldn't have any fetching.
  //
  // This is used when finishing sync module load, to reflect the loaded modules
  // to the async module loader pointed by `aDest`.
  void MoveModulesTo(ModuleLoaderBase* aDest);

  // Internal methods.

 private:
  friend class JS::loader::ModuleLoadRequest;

  static ModuleLoaderBase* GetCurrentModuleLoader(JSContext* aCx);
  static LoadedScript* GetLoadedScriptOrNull(
      JSContext* aCx, JS::Handle<JS::Value> aReferencingPrivate);

  static void EnsureModuleHooksInitialized();

  static JSObject* HostResolveImportedModule(
      JSContext* aCx, JS::Handle<JS::Value> aReferencingPrivate,
      JS::Handle<JSObject*> aModuleRequest);
  static bool HostPopulateImportMeta(JSContext* aCx,
                                     JS::Handle<JS::Value> aReferencingPrivate,
                                     JS::Handle<JSObject*> aMetaObject);
  static bool ImportMetaResolve(JSContext* cx, unsigned argc, Value* vp);
  static JSString* ImportMetaResolveImpl(
      JSContext* aCx, JS::Handle<JS::Value> aReferencingPrivate,
      JS::Handle<JSString*> aSpecifier);
  static bool HostImportModuleDynamically(
      JSContext* aCx, JS::Handle<JS::Value> aReferencingPrivate,
      JS::Handle<JSObject*> aModuleRequest, JS::Handle<JSObject*> aPromise);

  ResolveResult ResolveModuleSpecifier(LoadedScript* aScript,
                                       const nsAString& aSpecifier);

  nsresult HandleResolveFailure(JSContext* aCx, LoadedScript* aScript,
                                const nsAString& aSpecifier,
                                ResolveError aError, uint32_t aLineNumber,
                                JS::ColumnNumberOneOrigin aColumnNumber,
                                JS::MutableHandle<JS::Value> aErrorOut);

  enum class RestartRequest { No, Yes };
  nsresult StartOrRestartModuleLoad(ModuleLoadRequest* aRequest,
                                    RestartRequest aRestart);

  bool ModuleMapContainsURL(const ModuleMapKey& key) const;
  bool IsModuleFetching(const ModuleMapKey& key) const;
  void WaitForModuleFetch(ModuleLoadRequest* aRequest);

 protected:
  void SetModuleFetchStarted(ModuleLoadRequest* aRequest);

 private:
  ModuleScript* GetFetchedModule(const ModuleMapKey& moduleMapKey) const;

  JS::Value FindFirstParseError(JSContext* aCx, ModuleLoadRequest* aRequest);
  static nsresult InitDebuggerDataForModuleGraph(JSContext* aCx,
                                                 ModuleLoadRequest* aRequest);
  nsresult ResolveRequestedModules(
      ModuleLoadRequest* aRequest,
      nsTArray<ModuleMapKey>* aRequestedModulesOut);

  void SetModuleFetchFinishedAndResumeWaitingRequests(
      ModuleLoadRequest* aRequest, nsresult aResult);
  void ResumeWaitingRequests(LoadingRequest* aLoadingRequest, bool aSuccess);
  void ResumeWaitingRequest(ModuleLoadRequest* aRequest, bool aSuccess);

  void StartFetchingModuleDependencies(ModuleLoadRequest* aRequest);

  void StartFetchingModuleAndDependencies(ModuleLoadRequest* aParent,
                                          const ModuleMapKey& aRequestedModule);

  void InstantiateAndEvaluateDynamicImport(ModuleLoadRequest* aRequest);

  /**
   * Shorthand Wrapper for JSAPI FinishDynamicImport function for the reject
   * case where we do not have `aEvaluationPromise`. As there is no evaluation
   * Promise, JS::FinishDynamicImport will always reject.
   *
   * @param aRequest
   *        The module load request for the dynamic module.
   * @param aResult
   *        The result of running ModuleEvaluate -- If this is successful, then
   *        we can await the associated EvaluationPromise.
   */
  void FinishDynamicImportAndReject(ModuleLoadRequest* aRequest,
                                    nsresult aResult);

  /**
   * Wrapper for JSAPI FinishDynamicImport function. Takes an optional argument
   * `aEvaluationPromise` which, if null, exits early.
   *
   * This is the Top Level Await version, which works with modules which return
   * promises.
   *
   * @param aCX
   *        The JSContext for the module.
   * @param aRequest
   *        The module load request for the dynamic module.
   * @param aResult
   *        The result of running ModuleEvaluate -- If this is successful, then
   *        we can await the associated EvaluationPromise.
   * @param aEvaluationPromise
   *        The evaluation promise returned from evaluating the module. If this
   *        is null, JS::FinishDynamicImport will reject the dynamic import
   *        module promise.
   */
  static void FinishDynamicImport(JSContext* aCx, ModuleLoadRequest* aRequest,
                                  nsresult aResult,
                                  JS::Handle<JSObject*> aEvaluationPromise);

  void RemoveDynamicImport(ModuleLoadRequest* aRequest);

  nsresult CreateModuleScript(ModuleLoadRequest* aRequest);

  bool IsFetchingAndHasWaitingRequest(ModuleLoadRequest* aRequest);

  // The slot stored in ImportMetaResolve function.
  enum { ModulePrivateSlot = 0, SlotCount };

  // The number of args in ImportMetaResolve.
  static const uint32_t ImportMetaResolveNumArgs = 1;
  // The index of the 'specifier' argument in ImportMetaResolve.
  static const uint32_t ImportMetaResolveSpecifierArg = 0;

 public:
  static mozilla::LazyLogModule gCspPRLog;
  static mozilla::LazyLogModule gModuleLoaderBaseLog;
};

// Override the target module loader with given module loader while this
// instance is on the stack.
class MOZ_RAII AutoOverrideModuleLoader {
 public:
  AutoOverrideModuleLoader(ModuleLoaderBase* aTarget,
                           ModuleLoaderBase* aLoader);
  ~AutoOverrideModuleLoader();

 private:
  RefPtr<ModuleLoaderBase> mTarget;
};

}  // namespace loader
}  // namespace JS

#endif  // js_loader_ModuleLoaderBase_h
