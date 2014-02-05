/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/BasicEvents.h"
#ifdef MOZ_B2G
#include "mozilla/Hal.h"
#endif
#include "mozilla/HalSensor.h"

// Microsoft's API Name hackery sucks
#undef CreateEvent

#include "nsISupports.h"
#include "nsDOMEvent.h"
#include "nsEventListenerManager.h"
#include "nsIDOMEventListener.h"
#include "nsGkAtoms.h"
#include "nsPIDOMWindow.h"
#include "nsIJSEventListener.h"
#include "nsIScriptGlobalObject.h"
#include "nsINameSpaceManager.h"
#include "nsIContent.h"
#include "mozilla/MemoryReporting.h"
#include "nsCOMPtr.h"
#include "nsError.h"
#include "nsIDocument.h"
#include "mozilla/MutationEvent.h"
#include "nsIXPConnect.h"
#include "nsDOMCID.h"
#include "nsContentUtils.h"
#include "nsJSUtils.h"
#include "nsEventDispatcher.h"
#include "nsCOMArray.h"
#include "nsEventListenerService.h"
#include "nsIContentSecurityPolicy.h"
#include "xpcpublic.h"
#include "nsSandboxFlags.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/BindingUtils.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::hal;

#define EVENT_TYPE_EQUALS(ls, type, userType, typeString, allEvents) \
  ((ls->mEventType == type &&                                        \
    (ls->mEventType != NS_USER_DEFINED_EVENT ||                      \
    (mIsMainThreadELM && ls->mTypeAtom == userType) ||               \
    (!mIsMainThreadELM && ls->mTypeString.Equals(typeString)))) ||   \
   (allEvents && ls->mAllEvents))

static const uint32_t kAllMutationBits =
  NS_EVENT_BITS_MUTATION_SUBTREEMODIFIED |
  NS_EVENT_BITS_MUTATION_NODEINSERTED |
  NS_EVENT_BITS_MUTATION_NODEREMOVED |
  NS_EVENT_BITS_MUTATION_NODEREMOVEDFROMDOCUMENT |
  NS_EVENT_BITS_MUTATION_NODEINSERTEDINTODOCUMENT |
  NS_EVENT_BITS_MUTATION_ATTRMODIFIED |
  NS_EVENT_BITS_MUTATION_CHARACTERDATAMODIFIED;

static uint32_t
MutationBitForEventType(uint32_t aEventType)
{
  switch (aEventType) {
    case NS_MUTATION_SUBTREEMODIFIED:
      return NS_EVENT_BITS_MUTATION_SUBTREEMODIFIED;
    case NS_MUTATION_NODEINSERTED:
      return NS_EVENT_BITS_MUTATION_NODEINSERTED;
    case NS_MUTATION_NODEREMOVED:
      return NS_EVENT_BITS_MUTATION_NODEREMOVED;
    case NS_MUTATION_NODEREMOVEDFROMDOCUMENT:
      return NS_EVENT_BITS_MUTATION_NODEREMOVEDFROMDOCUMENT;
    case NS_MUTATION_NODEINSERTEDINTODOCUMENT:
      return NS_EVENT_BITS_MUTATION_NODEINSERTEDINTODOCUMENT;
    case NS_MUTATION_ATTRMODIFIED:
      return NS_EVENT_BITS_MUTATION_ATTRMODIFIED;
    case NS_MUTATION_CHARACTERDATAMODIFIED:
      return NS_EVENT_BITS_MUTATION_CHARACTERDATAMODIFIED;
    default:
      break;
  }
  return 0;
}

uint32_t nsEventListenerManager::sMainThreadCreatedCount = 0;

nsEventListenerManager::nsEventListenerManager(EventTarget* aTarget) :
  mMayHavePaintEventListener(false),
  mMayHaveMutationListeners(false),
  mMayHaveCapturingListeners(false),
  mMayHaveSystemGroupListeners(false),
  mMayHaveAudioAvailableEventListener(false),
  mMayHaveTouchEventListener(false),
  mMayHaveMouseEnterLeaveEventListener(false),
  mClearingListeners(false),
  mIsMainThreadELM(NS_IsMainThread()),
  mNoListenerForEvent(0),
  mTarget(aTarget)
{
  NS_ASSERTION(aTarget, "unexpected null pointer");

  if (mIsMainThreadELM) {
    ++sMainThreadCreatedCount;
  }
}

nsEventListenerManager::~nsEventListenerManager() 
{
  // If your code fails this assertion, a possible reason is that
  // a class did not call our Disconnect() manually. Note that
  // this class can have Disconnect called in one of two ways:
  // if it is part of a cycle, then in Unlink() (such a cycle
  // would be with one of the listeners, not mTarget which is weak).
  // If not part of a cycle, then Disconnect must be called manually,
  // typically from the destructor of the owner class (mTarget).
  // XXX azakai: Is there any reason to not just call Disconnect
  //             from right here, if not previously called?
  NS_ASSERTION(!mTarget, "didn't call Disconnect");
  RemoveAllListeners();

}

void
nsEventListenerManager::RemoveAllListeners()
{
  if (mClearingListeners) {
    return;
  }
  mClearingListeners = true;
  mListeners.Clear();
  mClearingListeners = false;
}

void
nsEventListenerManager::Shutdown()
{
  nsDOMEvent::Shutdown();
}

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(nsEventListenerManager, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(nsEventListenerManager, Release)

inline void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                            nsListenerStruct& aField,
                            const char* aName,
                            unsigned aFlags)
{
  if (MOZ_UNLIKELY(aCallback.WantDebugInfo())) {
    nsAutoCString name;
    name.AppendASCII(aName);
    if (aField.mTypeAtom) {
      name.AppendASCII(" event=");
      name.Append(nsAtomCString(aField.mTypeAtom));
      name.AppendASCII(" listenerType=");
      name.AppendInt(aField.mListenerType);
      name.AppendASCII(" ");
    }
    CycleCollectionNoteChild(aCallback, aField.mListener.GetISupports(), name.get(),
                             aFlags);
  } else {
    CycleCollectionNoteChild(aCallback, aField.mListener.GetISupports(), aName,
                             aFlags);
  }
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsEventListenerManager)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsEventListenerManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mListeners)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsEventListenerManager)
  tmp->Disconnect();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END


nsPIDOMWindow*
nsEventListenerManager::GetInnerWindowForTarget()
{
  nsCOMPtr<nsINode> node = do_QueryInterface(mTarget);
  if (node) {
    // XXX sXBL/XBL2 issue -- do we really want the owner here?  What
    // if that's the XBL document?
    return node->OwnerDoc()->GetInnerWindow();
  }

  nsCOMPtr<nsPIDOMWindow> window = GetTargetAsInnerWindow();
  return window;
}

already_AddRefed<nsPIDOMWindow>
nsEventListenerManager::GetTargetAsInnerWindow() const
{
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(mTarget);
  if (!window) {
    return nullptr;
  }

  NS_ASSERTION(window->IsInnerWindow(), "Target should not be an outer window");
  return window.forget();
}

void
nsEventListenerManager::AddEventListenerInternal(
                          const EventListenerHolder& aListener,
                          uint32_t aType,
                          nsIAtom* aTypeAtom,
                          const nsAString& aTypeString,
                          const EventListenerFlags& aFlags,
                          bool aHandler,
                          bool aAllEvents)
{
  MOZ_ASSERT((NS_IsMainThread() && aType && aTypeAtom) || // Main thread
             (!NS_IsMainThread() && aType && !aTypeString.IsEmpty()) || // non-main-thread
             aAllEvents, "Missing type"); // all-events listener

  if (!aListener || mClearingListeners) {
    return;
  }

  // Since there is no public API to call us with an EventListenerHolder, we
  // know that there's an EventListenerHolder on the stack holding a strong ref
  // to the listener.

  nsListenerStruct* ls;
  uint32_t count = mListeners.Length();
  for (uint32_t i = 0; i < count; i++) {
    ls = &mListeners.ElementAt(i);
    // mListener == aListener is the last one, since it can be a bit slow.
    if (ls->mListenerIsHandler == aHandler &&
        ls->mFlags == aFlags &&
        EVENT_TYPE_EQUALS(ls, aType, aTypeAtom, aTypeString, aAllEvents) &&
        ls->mListener == aListener) {
      return;
    }
  }

  mNoListenerForEvent = NS_EVENT_NULL;
  mNoListenerForEventAtom = nullptr;

  ls = aAllEvents ? mListeners.InsertElementAt(0) : mListeners.AppendElement();
  ls->mListener = aListener;
  MOZ_ASSERT(aType < PR_UINT16_MAX);
  ls->mEventType = aType;
  ls->mTypeString = aTypeString;
  ls->mTypeAtom = aTypeAtom;
  ls->mFlags = aFlags;
  ls->mListenerIsHandler = aHandler;
  ls->mHandlerIsString = false;
  ls->mAllEvents = aAllEvents;

  // Detect the type of event listener.
  nsCOMPtr<nsIXPConnectWrappedJS> wjs;
  if (aFlags.mListenerIsJSListener) {
    MOZ_ASSERT(!aListener.HasWebIDLCallback());
    ls->mListenerType = eJSEventListener;
  } else if (aListener.HasWebIDLCallback()) {
    ls->mListenerType = eWebIDLListener;
  } else if ((wjs = do_QueryInterface(aListener.GetXPCOMCallback()))) {
    ls->mListenerType = eWrappedJSListener;
  } else {
    ls->mListenerType = eNativeListener;
  }


  if (aFlags.mInSystemGroup) {
    mMayHaveSystemGroupListeners = true;
  }
  if (aFlags.mCapture) {
    mMayHaveCapturingListeners = true;
  }

  if (aType == NS_AFTERPAINT) {
    mMayHavePaintEventListener = true;
    nsPIDOMWindow* window = GetInnerWindowForTarget();
    if (window) {
      window->SetHasPaintEventListeners();
    }
  } else if (aType == NS_MOZAUDIOAVAILABLE) {
    mMayHaveAudioAvailableEventListener = true;
    nsPIDOMWindow* window = GetInnerWindowForTarget();
    if (window) {
      window->SetHasAudioAvailableEventListeners();
    }
  } else if (aType >= NS_MUTATION_START && aType <= NS_MUTATION_END) {
    // For mutation listeners, we need to update the global bit on the DOM window.
    // Otherwise we won't actually fire the mutation event.
    mMayHaveMutationListeners = true;
    // Go from our target to the nearest enclosing DOM window.
    nsPIDOMWindow* window = GetInnerWindowForTarget();
    if (window) {
      nsCOMPtr<nsIDocument> doc = window->GetExtantDoc();
      if (doc) {
        doc->WarnOnceAbout(nsIDocument::eMutationEvent);
      }
      // If aType is NS_MUTATION_SUBTREEMODIFIED, we need to listen all
      // mutations. nsContentUtils::HasMutationListeners relies on this.
      window->SetMutationListeners((aType == NS_MUTATION_SUBTREEMODIFIED) ?
                                   kAllMutationBits :
                                   MutationBitForEventType(aType));
    }
  } else if (aTypeAtom == nsGkAtoms::ondeviceorientation) {
    EnableDevice(NS_DEVICE_ORIENTATION);
  } else if (aTypeAtom == nsGkAtoms::ondeviceproximity || aTypeAtom == nsGkAtoms::onuserproximity) {
    EnableDevice(NS_DEVICE_PROXIMITY);
  } else if (aTypeAtom == nsGkAtoms::ondevicelight) {
    EnableDevice(NS_DEVICE_LIGHT);
  } else if (aTypeAtom == nsGkAtoms::ondevicemotion) {
    EnableDevice(NS_DEVICE_MOTION);
#ifdef MOZ_B2G
  } else if (aTypeAtom == nsGkAtoms::onmoztimechange) {
    nsCOMPtr<nsPIDOMWindow> window = GetTargetAsInnerWindow();
    if (window) {
      window->EnableTimeChangeNotifications();
    }
  } else if (aTypeAtom == nsGkAtoms::onmoznetworkupload) {
    nsCOMPtr<nsPIDOMWindow> window = GetTargetAsInnerWindow();
    if (window) {
      window->EnableNetworkEvent(NS_NETWORK_UPLOAD_EVENT);
    }
  } else if (aTypeAtom == nsGkAtoms::onmoznetworkdownload) {
    nsCOMPtr<nsPIDOMWindow> window = GetTargetAsInnerWindow();
    if (window) {
      window->EnableNetworkEvent(NS_NETWORK_DOWNLOAD_EVENT);
    }
#endif // MOZ_B2G
  } else if (aTypeAtom == nsGkAtoms::ontouchstart ||
             aTypeAtom == nsGkAtoms::ontouchend ||
             aTypeAtom == nsGkAtoms::ontouchmove ||
             aTypeAtom == nsGkAtoms::ontouchenter ||
             aTypeAtom == nsGkAtoms::ontouchleave ||
             aTypeAtom == nsGkAtoms::ontouchcancel) {
    mMayHaveTouchEventListener = true;
    nsPIDOMWindow* window = GetInnerWindowForTarget();
    // we don't want touchevent listeners added by scrollbars to flip this flag
    // so we ignore listeners created with system event flag
    if (window && !aFlags.mInSystemGroup) {
      window->SetHasTouchEventListeners();
    }
  } else if (aTypeAtom == nsGkAtoms::onmouseenter ||
             aTypeAtom == nsGkAtoms::onmouseleave) {
    mMayHaveMouseEnterLeaveEventListener = true;
    nsPIDOMWindow* window = GetInnerWindowForTarget();
    if (window) {
#ifdef DEBUG
      nsCOMPtr<nsIDocument> d = window->GetExtantDoc();
      NS_WARN_IF_FALSE(!nsContentUtils::IsChromeDoc(d),
                       "Please do not use mouseenter/leave events in chrome. "
                       "They are slower than mouseover/out!");
#endif
      window->SetHasMouseEnterLeaveEventListeners();
    }
#ifdef MOZ_GAMEPAD
  } else if (aType >= NS_GAMEPAD_START &&
             aType <= NS_GAMEPAD_END) {
    nsPIDOMWindow* window = GetInnerWindowForTarget();
    if (window) {
      window->SetHasGamepadEventListener();
    }
#endif
  }
  if (aTypeAtom && mTarget) {
    mTarget->EventListenerAdded(aTypeAtom);
  }
}

bool
nsEventListenerManager::IsDeviceType(uint32_t aType)
{
  switch (aType) {
    case NS_DEVICE_ORIENTATION:
    case NS_DEVICE_MOTION:
    case NS_DEVICE_LIGHT:
    case NS_DEVICE_PROXIMITY:
    case NS_USER_PROXIMITY:
      return true;
    default:
      break;
  }
  return false;
}

void
nsEventListenerManager::EnableDevice(uint32_t aType)
{
  nsCOMPtr<nsPIDOMWindow> window = GetTargetAsInnerWindow();
  if (!window) {
    return;
  }

  switch (aType) {
    case NS_DEVICE_ORIENTATION:
      window->EnableDeviceSensor(SENSOR_ORIENTATION);
      break;
    case NS_DEVICE_PROXIMITY:
    case NS_USER_PROXIMITY:
      window->EnableDeviceSensor(SENSOR_PROXIMITY);
      break;
    case NS_DEVICE_LIGHT:
      window->EnableDeviceSensor(SENSOR_LIGHT);
      break;
    case NS_DEVICE_MOTION:
      window->EnableDeviceSensor(SENSOR_ACCELERATION);
      window->EnableDeviceSensor(SENSOR_LINEAR_ACCELERATION);
      window->EnableDeviceSensor(SENSOR_GYROSCOPE);
      break;
    default:
      NS_WARNING("Enabling an unknown device sensor.");
      break;
  }
}

void
nsEventListenerManager::DisableDevice(uint32_t aType)
{
  nsCOMPtr<nsPIDOMWindow> window = GetTargetAsInnerWindow();
  if (!window) {
    return;
  }

  switch (aType) {
    case NS_DEVICE_ORIENTATION:
      window->DisableDeviceSensor(SENSOR_ORIENTATION);
      break;
    case NS_DEVICE_MOTION:
      window->DisableDeviceSensor(SENSOR_ACCELERATION);
      window->DisableDeviceSensor(SENSOR_LINEAR_ACCELERATION);
      window->DisableDeviceSensor(SENSOR_GYROSCOPE);
      break;
    case NS_DEVICE_PROXIMITY:
    case NS_USER_PROXIMITY:
      window->DisableDeviceSensor(SENSOR_PROXIMITY);
      break;
    case NS_DEVICE_LIGHT:
      window->DisableDeviceSensor(SENSOR_LIGHT);
      break;
    default:
      NS_WARNING("Disabling an unknown device sensor.");
      break;
  }
}

void
nsEventListenerManager::RemoveEventListenerInternal(
                          const EventListenerHolder& aListener, 
                          uint32_t aType,
                          nsIAtom* aUserType,
                          const nsAString& aTypeString,
                          const EventListenerFlags& aFlags,
                          bool aAllEvents)
{
  if (!aListener || !aType || mClearingListeners) {
    return;
  }

  nsListenerStruct* ls;

  uint32_t count = mListeners.Length();
  uint32_t typeCount = 0;
  bool deviceType = IsDeviceType(aType);
#ifdef MOZ_B2G
  bool timeChangeEvent = (aType == NS_MOZ_TIME_CHANGE_EVENT);
  bool networkEvent = (aType == NS_NETWORK_UPLOAD_EVENT ||
                       aType == NS_NETWORK_DOWNLOAD_EVENT);
#endif // MOZ_B2G

  for (uint32_t i = 0; i < count; ++i) {
    ls = &mListeners.ElementAt(i);
    if (EVENT_TYPE_EQUALS(ls, aType, aUserType, aTypeString, aAllEvents)) {
      ++typeCount;
      if (ls->mListener == aListener &&
          ls->mFlags.EqualsIgnoringTrustness(aFlags)) {
        nsRefPtr<nsEventListenerManager> kungFuDeathGrip = this;
        mListeners.RemoveElementAt(i);
        --count;
        mNoListenerForEvent = NS_EVENT_NULL;
        mNoListenerForEventAtom = nullptr;
        if (mTarget && aUserType) {
          mTarget->EventListenerRemoved(aUserType);
        }

        if (!deviceType
#ifdef MOZ_B2G
            && !timeChangeEvent && !networkEvent
#endif // MOZ_B2G
            ) {
          return;
        }
        --typeCount;
      }
    }
  }

  if (!aAllEvents && deviceType && typeCount == 0) {
    DisableDevice(aType);
#ifdef MOZ_B2G
  } else if (timeChangeEvent && typeCount == 0) {
    nsCOMPtr<nsPIDOMWindow> window = GetTargetAsInnerWindow();
    if (window) {
      window->DisableTimeChangeNotifications();
    }
  } else if (!aAllEvents && networkEvent && typeCount == 0) {
    nsCOMPtr<nsPIDOMWindow> window = GetTargetAsInnerWindow();
    if (window) {
      window->DisableNetworkEvent(aType);
    }
#endif // MOZ_B2G
  }
}

bool
nsEventListenerManager::ListenerCanHandle(nsListenerStruct* aLs,
                                          WidgetEvent* aEvent)
{
  // This is slightly different from EVENT_TYPE_EQUALS in that it returns
  // true even when aEvent->message == NS_USER_DEFINED_EVENT and
  // aLs=>mEventType != NS_USER_DEFINED_EVENT as long as the atoms are the same
  if (aLs->mAllEvents) {
    return true;
  }
  if (aEvent->message == NS_USER_DEFINED_EVENT) {
    if (mIsMainThreadELM) {
      return aLs->mTypeAtom == aEvent->userType;
    }
    return aLs->mTypeString.Equals(aEvent->typeString);
  }
  MOZ_ASSERT(mIsMainThreadELM);
  return aLs->mEventType == aEvent->message;
}

void
nsEventListenerManager::AddEventListenerByType(const EventListenerHolder& aListener, 
                                               const nsAString& aType,
                                               const EventListenerFlags& aFlags)
{
  nsCOMPtr<nsIAtom> atom =
    mIsMainThreadELM ? do_GetAtom(NS_LITERAL_STRING("on") + aType) : nullptr;
  uint32_t type = nsContentUtils::GetEventId(atom);
  AddEventListenerInternal(aListener, type, atom, aType, aFlags);
}

void
nsEventListenerManager::RemoveEventListenerByType(
                          const EventListenerHolder& aListener,
                          const nsAString& aType,
                          const EventListenerFlags& aFlags)
{
  nsCOMPtr<nsIAtom> atom =
    mIsMainThreadELM ? do_GetAtom(NS_LITERAL_STRING("on") + aType) : nullptr;
  uint32_t type = nsContentUtils::GetEventId(atom);
  RemoveEventListenerInternal(aListener, type, atom, aType, aFlags);
}

nsListenerStruct*
nsEventListenerManager::FindEventHandler(uint32_t aEventType,
                                         nsIAtom* aTypeAtom,
                                         const nsAString& aTypeString)
{
  // Run through the listeners for this type and see if a script
  // listener is registered
  nsListenerStruct *ls;
  uint32_t count = mListeners.Length();
  for (uint32_t i = 0; i < count; ++i) {
    ls = &mListeners.ElementAt(i);
    if (ls->mListenerIsHandler &&
        EVENT_TYPE_EQUALS(ls, aEventType, aTypeAtom, aTypeString, false)) {
      return ls;
    }
  }
  return nullptr;
}

nsListenerStruct*
nsEventListenerManager::SetEventHandlerInternal(JS::Handle<JSObject*> aScopeObject,
                                                nsIAtom* aName,
                                                const nsAString& aTypeString,
                                                const nsEventHandler& aHandler,
                                                bool aPermitUntrustedEvents)
{
  MOZ_ASSERT(aScopeObject || aHandler.HasEventHandler(),
             "Must have one or the other!");
  MOZ_ASSERT(aName || !aTypeString.IsEmpty());

  uint32_t eventType = nsContentUtils::GetEventId(aName);
  nsListenerStruct* ls = FindEventHandler(eventType, aName, aTypeString);

  if (!ls) {
    // If we didn't find a script listener or no listeners existed
    // create and add a new one.
    EventListenerFlags flags;
    flags.mListenerIsJSListener = true;

    nsCOMPtr<nsIJSEventListener> scriptListener;
    NS_NewJSEventListener(aScopeObject, mTarget, aName,
                          aHandler, getter_AddRefs(scriptListener));
    EventListenerHolder holder(scriptListener);
    AddEventListenerInternal(holder, eventType, aName, aTypeString, flags,
                             true);

    ls = FindEventHandler(eventType, aName, aTypeString);
  } else {
    nsIJSEventListener* scriptListener = ls->GetJSListener();
    MOZ_ASSERT(scriptListener,
               "How can we have an event handler with no nsIJSEventListener?");

    bool same = scriptListener->GetHandler() == aHandler;
    // Possibly the same listener, but update still the context and scope.
    scriptListener->SetHandler(aHandler, aScopeObject);
    if (mTarget && !same && aName) {
      mTarget->EventListenerRemoved(aName);
      mTarget->EventListenerAdded(aName);
    }
  }

  // Set flag to indicate possible need for compilation later
  ls->mHandlerIsString = !aHandler.HasEventHandler();
  if (aPermitUntrustedEvents) {
    ls->mFlags.mAllowUntrustedEvents = true;
  }

  return ls;
}

nsresult
nsEventListenerManager::SetEventHandler(nsIAtom *aName,
                                        const nsAString& aBody,
                                        uint32_t aLanguage,
                                        bool aDeferCompilation,
                                        bool aPermitUntrustedEvents,
                                        Element* aElement)
{
  NS_PRECONDITION(aLanguage != nsIProgrammingLanguage::UNKNOWN,
                  "Must know the language for the script event listener");

  // |aPermitUntrustedEvents| is set to False for chrome - events
  // *generated* from an unknown source are not allowed.
  // However, for script languages with no 'sandbox', we want to reject
  // such scripts based on the source of their code, not just the source
  // of the event.
  if (aPermitUntrustedEvents && 
      aLanguage != nsIProgrammingLanguage::JAVASCRIPT) {
    NS_WARNING("Discarding non-JS event listener from untrusted source");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDocument> doc;
  nsCOMPtr<nsIScriptGlobalObject> global =
    GetScriptGlobalAndDocument(getter_AddRefs(doc));

  if (!global) {
    // This can happen; for example this document might have been
    // loaded as data.
    return NS_OK;
  }

#ifdef DEBUG
  nsCOMPtr<nsPIDOMWindow> win = do_QueryInterface(global);
  if (win) {
    MOZ_ASSERT(win->IsInnerWindow(), "We should not have an outer window here!");
  }
#endif

  nsresult rv = NS_OK;
  // return early preventing the event listener from being added
  // 'doc' is fetched above
  if (doc) {
    // Don't allow adding an event listener if the document is sandboxed
    // without 'allow-scripts'.
    if (doc->GetSandboxFlags() & SANDBOXED_SCRIPTS) {
      return NS_ERROR_DOM_SECURITY_ERR;
    }

    nsCOMPtr<nsIContentSecurityPolicy> csp;
    rv = doc->NodePrincipal()->GetCsp(getter_AddRefs(csp));
    NS_ENSURE_SUCCESS(rv, rv);

    if (csp) {
      bool inlineOK = true;
      bool reportViolations = false;
      rv = csp->GetAllowsInlineScript(&reportViolations, &inlineOK);
      NS_ENSURE_SUCCESS(rv, rv);

      if (reportViolations) {
        // gather information to log with violation report
        nsIURI* uri = doc->GetDocumentURI();
        nsAutoCString asciiSpec;
        if (uri)
          uri->GetAsciiSpec(asciiSpec);
        nsAutoString scriptSample, attr, tagName(NS_LITERAL_STRING("UNKNOWN"));
        aName->ToString(attr);
        nsCOMPtr<nsIDOMNode> domNode(do_QueryInterface(mTarget));
        if (domNode)
          domNode->GetNodeName(tagName);
        // build a "script sample" based on what we know about this element
        scriptSample.Assign(attr);
        scriptSample.AppendLiteral(" attribute on ");
        scriptSample.Append(tagName);
        scriptSample.AppendLiteral(" element");
        csp->LogViolationDetails(nsIContentSecurityPolicy::VIOLATION_TYPE_INLINE_SCRIPT,
                                 NS_ConvertUTF8toUTF16(asciiSpec),
                                 scriptSample,
                                 0,
                                 EmptyString(),
                                 EmptyString());
      }

      // return early if CSP wants us to block inline scripts
      if (!inlineOK) {
        return NS_OK;
      }
    }
  }

  // This might be the first reference to this language in the global
  // We must init the language before we attempt to fetch its context.
  if (NS_FAILED(global->EnsureScriptEnvironment())) {
    NS_WARNING("Failed to setup script environment for this language");
    // but fall through and let the inevitable failure below handle it.
  }

  nsIScriptContext* context = global->GetScriptContext();
  NS_ENSURE_TRUE(context, NS_ERROR_FAILURE);

  NS_ENSURE_STATE(global->GetGlobalJSObject());

  JSAutoRequest ar(context->GetNativeContext());
  JS::Rooted<JSObject*> scope(context->GetNativeContext(),
                              global->GetGlobalJSObject());

  nsListenerStruct* ls = SetEventHandlerInternal(scope, aName,
                                                 EmptyString(),
                                                 nsEventHandler(),
                                                 aPermitUntrustedEvents);

  if (!aDeferCompilation) {
    return CompileEventHandlerInternal(ls, &aBody, aElement);
  }

  return NS_OK;
}

void
nsEventListenerManager::RemoveEventHandler(nsIAtom* aName,
                                           const nsAString& aTypeString)
{
  if (mClearingListeners) {
    return;
  }

  uint32_t eventType = nsContentUtils::GetEventId(aName);
  nsListenerStruct* ls = FindEventHandler(eventType, aName, aTypeString);

  if (ls) {
    mListeners.RemoveElementAt(uint32_t(ls - &mListeners.ElementAt(0)));
    mNoListenerForEvent = NS_EVENT_NULL;
    mNoListenerForEventAtom = nullptr;
    if (mTarget && aName) {
      mTarget->EventListenerRemoved(aName);
    }
  }
}

nsresult
nsEventListenerManager::CompileEventHandlerInternal(nsListenerStruct *aListenerStruct,
                                                    const nsAString* aBody,
                                                    Element* aElement)
{
  NS_PRECONDITION(aListenerStruct->GetJSListener(),
                  "Why do we not have a JS listener?");
  NS_PRECONDITION(aListenerStruct->mHandlerIsString,
                  "Why are we compiling a non-string JS listener?");

  nsresult result = NS_OK;

  nsIJSEventListener *listener = aListenerStruct->GetJSListener();
  NS_ASSERTION(!listener->GetHandler().HasEventHandler(),
               "What is there to compile?");

  nsCOMPtr<nsIDocument> doc;
  nsCOMPtr<nsIScriptGlobalObject> global =
    GetScriptGlobalAndDocument(getter_AddRefs(doc));
  NS_ENSURE_STATE(global);

  nsIScriptContext* context = global->GetScriptContext();
  NS_ENSURE_STATE(context);

  // Push a context to make sure exceptions are reported in the right place.
  AutoPushJSContext cx(context->GetNativeContext());
  JS::Rooted<JSObject*> handler(cx);

  JS::Rooted<JSObject*> scope(cx, listener->GetEventScope());

  nsIAtom* attrName = aListenerStruct->mTypeAtom;

  if (aListenerStruct->mHandlerIsString) {
    // OK, we didn't find an existing compiled event handler.  Flag us
    // as not a string so we don't keep trying to compile strings
    // which can't be compiled
    aListenerStruct->mHandlerIsString = false;

    // mTarget may not be an Element if it's a window and we're
    // getting an inline event listener forwarded from <html:body> or
    // <html:frameset> or <xul:window> or the like.
    // XXX I don't like that we have to reference content from
    // here. The alternative is to store the event handler string on
    // the nsIJSEventListener itself, and that still doesn't address
    // the arg names issue.
    nsCOMPtr<Element> element = do_QueryInterface(mTarget);
    MOZ_ASSERT(element || aBody, "Where will we get our body?");
    nsAutoString handlerBody;
    const nsAString* body = aBody;
    if (!aBody) {
      if (aListenerStruct->mTypeAtom == nsGkAtoms::onSVGLoad)
        attrName = nsGkAtoms::onload;
      else if (aListenerStruct->mTypeAtom == nsGkAtoms::onSVGUnload)
        attrName = nsGkAtoms::onunload;
      else if (aListenerStruct->mTypeAtom == nsGkAtoms::onSVGAbort)
        attrName = nsGkAtoms::onabort;
      else if (aListenerStruct->mTypeAtom == nsGkAtoms::onSVGError)
        attrName = nsGkAtoms::onerror;
      else if (aListenerStruct->mTypeAtom == nsGkAtoms::onSVGResize)
        attrName = nsGkAtoms::onresize;
      else if (aListenerStruct->mTypeAtom == nsGkAtoms::onSVGScroll)
        attrName = nsGkAtoms::onscroll;
      else if (aListenerStruct->mTypeAtom == nsGkAtoms::onSVGZoom)
        attrName = nsGkAtoms::onzoom;
      else if (aListenerStruct->mTypeAtom == nsGkAtoms::onbeginEvent)
        attrName = nsGkAtoms::onbegin;
      else if (aListenerStruct->mTypeAtom == nsGkAtoms::onrepeatEvent)
        attrName = nsGkAtoms::onrepeat;
      else if (aListenerStruct->mTypeAtom == nsGkAtoms::onendEvent)
        attrName = nsGkAtoms::onend;

      element->GetAttr(kNameSpaceID_None, attrName, handlerBody);
      body = &handlerBody;
      aElement = element;
    }

    uint32_t lineNo = 0;
    nsAutoCString url (NS_LITERAL_CSTRING("-moz-evil:lying-event-listener"));
    MOZ_ASSERT(body);
    MOZ_ASSERT(aElement);
    nsIURI *uri = aElement->OwnerDoc()->GetDocumentURI();
    if (uri) {
      uri->GetSpec(url);
      lineNo = 1;
    }

    uint32_t argCount;
    const char **argNames;
    nsContentUtils::GetEventArgNames(aElement->GetNameSpaceID(),
                                     aListenerStruct->mTypeAtom,
                                     &argCount, &argNames);

    JSAutoCompartment ac(cx, context->GetWindowProxy());
    JS::CompileOptions options(cx);
    options.setFileAndLine(url.get(), lineNo)
           .setVersion(SCRIPTVERSION_DEFAULT);

    JS::Rooted<JS::Value> targetVal(cx);
    // Go ahead and wrap into the current compartment of cx directly.
    JS::Rooted<JSObject*> wrapScope(cx, JS::CurrentGlobalOrNull(cx));
    if (WrapNewBindingObject(cx, wrapScope, aElement, &targetVal)) {
      MOZ_ASSERT(targetVal.isObject());

      nsDependentAtomString str(attrName);
      // Most of our names are short enough that we don't even have to malloc
      // the JS string stuff, so don't worry about playing games with
      // refcounting XPCOM stringbuffers.
      JS::Rooted<JSString*> jsStr(cx, JS_NewUCStringCopyN(cx,
                                                          str.BeginReading(),
                                                          str.Length()));
      NS_ENSURE_TRUE(jsStr, NS_ERROR_OUT_OF_MEMORY);

      options.setElement(&targetVal.toObject())
             .setElementAttributeName(jsStr);
    }

    JS::Rooted<JSObject*> handlerFun(cx);
    result = nsJSUtils::CompileFunction(cx, JS::NullPtr(), options,
                                        nsAtomCString(aListenerStruct->mTypeAtom),
                                        argCount, argNames, *body, handlerFun.address());
    NS_ENSURE_SUCCESS(result, result);
    handler = handlerFun;
    NS_ENSURE_TRUE(handler, NS_ERROR_FAILURE);
  }

  if (handler) {
    nsCOMPtr<nsPIDOMWindow> win = do_QueryInterface(mTarget);
    // Bind it
    JS::Rooted<JSObject*> boundHandler(cx);
    context->BindCompiledEventHandler(mTarget, scope, handler, &boundHandler);
    aListenerStruct = nullptr;
    // Note - We pass null for aIncumbentGlobal below. We could also pass the
    // compilation global, but since the handler is guaranteed to be scripted,
    // there's no need to use an override, since the JS engine will always give
    // us the right answer.
    if (!boundHandler) {
      listener->ForgetHandler();
    } else if (listener->EventName() == nsGkAtoms::onerror && win) {
      nsRefPtr<OnErrorEventHandlerNonNull> handlerCallback =
        new OnErrorEventHandlerNonNull(boundHandler, /* aIncumbentGlobal = */ nullptr);
      listener->SetHandler(handlerCallback);
    } else if (listener->EventName() == nsGkAtoms::onbeforeunload && win) {
      nsRefPtr<OnBeforeUnloadEventHandlerNonNull> handlerCallback =
        new OnBeforeUnloadEventHandlerNonNull(boundHandler, /* aIncumbentGlobal = */ nullptr);
      listener->SetHandler(handlerCallback);
    } else {
      nsRefPtr<EventHandlerNonNull> handlerCallback =
        new EventHandlerNonNull(boundHandler, /* aIncumbentGlobal = */ nullptr);
      listener->SetHandler(handlerCallback);
    }
  }

  return result;
}

nsresult
nsEventListenerManager::HandleEventSubType(nsListenerStruct* aListenerStruct,
                                           nsIDOMEvent* aDOMEvent,
                                           EventTarget* aCurrentTarget)
{
  nsresult result = NS_OK;
  EventListenerHolder listener(aListenerStruct->mListener);  // strong ref

  // If this is a script handler and we haven't yet
  // compiled the event handler itself
  if ((aListenerStruct->mListenerType == eJSEventListener) &&
      aListenerStruct->mHandlerIsString) {
    result = CompileEventHandlerInternal(aListenerStruct, nullptr, nullptr);
    aListenerStruct = nullptr;
  }

  if (NS_SUCCEEDED(result)) {
    if (mIsMainThreadELM) {
      nsContentUtils::EnterMicroTask();
    }
    // nsIDOMEvent::currentTarget is set in nsEventDispatcher.
    if (listener.HasWebIDLCallback()) {
      ErrorResult rv;
      listener.GetWebIDLCallback()->
        HandleEvent(aCurrentTarget, *(aDOMEvent->InternalDOMEvent()), rv);
      result = rv.ErrorCode();
    } else {
      result = listener.GetXPCOMCallback()->HandleEvent(aDOMEvent);
    }
    if (mIsMainThreadELM) {
      nsContentUtils::LeaveMicroTask();
    }
  }

  return result;
}

/**
* Causes a check for event listeners and processing by them if they exist.
* @param an event listener
*/

void
nsEventListenerManager::HandleEventInternal(nsPresContext* aPresContext,
                                            WidgetEvent* aEvent,
                                            nsIDOMEvent** aDOMEvent,
                                            EventTarget* aCurrentTarget,
                                            nsEventStatus* aEventStatus)
{
  //Set the value of the internal PreventDefault flag properly based on aEventStatus
  if (*aEventStatus == nsEventStatus_eConsumeNoDefault) {
    aEvent->mFlags.mDefaultPrevented = true;
  }

  nsAutoTObserverArray<nsListenerStruct, 2>::EndLimitedIterator iter(mListeners);
  Maybe<nsAutoPopupStatePusher> popupStatePusher;
  if (mIsMainThreadELM) {
    popupStatePusher.construct(nsDOMEvent::GetEventPopupControlState(aEvent));
  }

  bool hasListener = false;
  while (iter.HasMore()) {
    if (aEvent->mFlags.mImmediatePropagationStopped) {
      break;
    }
    nsListenerStruct* ls = &iter.GetNext();
    // Check that the phase is same in event and event listener.
    // Handle only trusted events, except when listener permits untrusted events.
    if (ListenerCanHandle(ls, aEvent)) {
      hasListener = true;
      if (ls->IsListening(aEvent) &&
          (aEvent->mFlags.mIsTrusted || ls->mFlags.mAllowUntrustedEvents)) {
        if (!*aDOMEvent) {
          // This is tiny bit slow, but happens only once per event.
          nsCOMPtr<mozilla::dom::EventTarget> et =
            do_QueryInterface(aEvent->originalTarget);
          nsEventDispatcher::CreateEvent(et, aPresContext,
                                         aEvent, EmptyString(), aDOMEvent);
        }
        if (*aDOMEvent) {
          if (!aEvent->currentTarget) {
            aEvent->currentTarget = aCurrentTarget->GetTargetForDOMEvent();
            if (!aEvent->currentTarget) {
              break;
            }
          }

          if (NS_FAILED(HandleEventSubType(ls, *aDOMEvent, aCurrentTarget))) {
            aEvent->mFlags.mExceptionHasBeenRisen = true;
          }
        }
      }
    }
  }

  aEvent->currentTarget = nullptr;

  if (mIsMainThreadELM && !hasListener) {
    mNoListenerForEvent = aEvent->message;
    mNoListenerForEventAtom = aEvent->userType;
  }

  if (aEvent->mFlags.mDefaultPrevented) {
    *aEventStatus = nsEventStatus_eConsumeNoDefault;
  }
}

void
nsEventListenerManager::Disconnect()
{
  mTarget = nullptr;
  RemoveAllListeners();
}

void
nsEventListenerManager::AddEventListener(const nsAString& aType,
                                         const EventListenerHolder& aListener,
                                         bool aUseCapture,
                                         bool aWantsUntrusted)
{
  EventListenerFlags flags;
  flags.mCapture = aUseCapture;
  flags.mAllowUntrustedEvents = aWantsUntrusted;
  return AddEventListenerByType(aListener, aType, flags);
}

void
nsEventListenerManager::RemoveEventListener(const nsAString& aType, 
                                            const EventListenerHolder& aListener, 
                                            bool aUseCapture)
{
  EventListenerFlags flags;
  flags.mCapture = aUseCapture;
  RemoveEventListenerByType(aListener, aType, flags);
}

void
nsEventListenerManager::AddListenerForAllEvents(nsIDOMEventListener* aListener,
                                                bool aUseCapture,
                                                bool aWantsUntrusted,
                                                bool aSystemEventGroup)
{
  EventListenerFlags flags;
  flags.mCapture = aUseCapture;
  flags.mAllowUntrustedEvents = aWantsUntrusted;
  flags.mInSystemGroup = aSystemEventGroup;
  EventListenerHolder holder(aListener);
  AddEventListenerInternal(holder, NS_EVENT_ALL, nullptr, EmptyString(),
                           flags, false, true);
}

void
nsEventListenerManager::RemoveListenerForAllEvents(nsIDOMEventListener* aListener, 
                                                   bool aUseCapture,
                                                   bool aSystemEventGroup)
{
  EventListenerFlags flags;
  flags.mCapture = aUseCapture;
  flags.mInSystemGroup = aSystemEventGroup;
  EventListenerHolder holder(aListener);
  RemoveEventListenerInternal(holder, NS_EVENT_ALL, nullptr, EmptyString(),
                              flags, true);
}

bool
nsEventListenerManager::HasMutationListeners()
{
  if (mMayHaveMutationListeners) {
    uint32_t count = mListeners.Length();
    for (uint32_t i = 0; i < count; ++i) {
      nsListenerStruct* ls = &mListeners.ElementAt(i);
      if (ls->mEventType >= NS_MUTATION_START &&
          ls->mEventType <= NS_MUTATION_END) {
        return true;
      }
    }
  }

  return false;
}

uint32_t
nsEventListenerManager::MutationListenerBits()
{
  uint32_t bits = 0;
  if (mMayHaveMutationListeners) {
    uint32_t count = mListeners.Length();
    for (uint32_t i = 0; i < count; ++i) {
      nsListenerStruct* ls = &mListeners.ElementAt(i);
      if (ls->mEventType >= NS_MUTATION_START &&
          ls->mEventType <= NS_MUTATION_END) {
        if (ls->mEventType == NS_MUTATION_SUBTREEMODIFIED) {
          return kAllMutationBits;
        }
        bits |= MutationBitForEventType(ls->mEventType);
      }
    }
  }
  return bits;
}

bool
nsEventListenerManager::HasListenersFor(const nsAString& aEventName)
{
  nsCOMPtr<nsIAtom> atom = do_GetAtom(NS_LITERAL_STRING("on") + aEventName);
  return HasListenersFor(atom);
}

bool
nsEventListenerManager::HasListenersFor(nsIAtom* aEventNameWithOn)
{
#ifdef DEBUG
  nsAutoString name;
  aEventNameWithOn->ToString(name);
#endif
  NS_ASSERTION(StringBeginsWith(name, NS_LITERAL_STRING("on")),
               "Event name does not start with 'on'");
  uint32_t count = mListeners.Length();
  for (uint32_t i = 0; i < count; ++i) {
    nsListenerStruct* ls = &mListeners.ElementAt(i);
    if (ls->mTypeAtom == aEventNameWithOn) {
      return true;
    }
  }
  return false;
}

bool
nsEventListenerManager::HasListeners()
{
  return !mListeners.IsEmpty();
}

nsresult
nsEventListenerManager::GetListenerInfo(nsCOMArray<nsIEventListenerInfo>* aList)
{
  nsCOMPtr<EventTarget> target = do_QueryInterface(mTarget);
  NS_ENSURE_STATE(target);
  aList->Clear();
  uint32_t count = mListeners.Length();
  for (uint32_t i = 0; i < count; ++i) {
    const nsListenerStruct& ls = mListeners.ElementAt(i);
    // If this is a script handler and we haven't yet
    // compiled the event handler itself go ahead and compile it
    if ((ls.mListenerType == eJSEventListener) && ls.mHandlerIsString) {
      CompileEventHandlerInternal(const_cast<nsListenerStruct*>(&ls), nullptr,
                                  nullptr);
    }
    nsAutoString eventType;
    if (ls.mAllEvents) {
      eventType.SetIsVoid(true);
    } else {
      eventType.Assign(Substring(nsDependentAtomString(ls.mTypeAtom), 2));
    }
    // EventListenerInfo is defined in XPCOM, so we have to go ahead
    // and convert to an XPCOM callback here...
    nsRefPtr<nsEventListenerInfo> info =
      new nsEventListenerInfo(eventType, ls.mListener.ToXPCOMCallback(),
                              ls.mFlags.mCapture,
                              ls.mFlags.mAllowUntrustedEvents,
                              ls.mFlags.mInSystemGroup);
    aList->AppendObject(info);
  }
  return NS_OK;
}

bool
nsEventListenerManager::HasUnloadListeners()
{
  uint32_t count = mListeners.Length();
  for (uint32_t i = 0; i < count; ++i) {
    nsListenerStruct* ls = &mListeners.ElementAt(i);
    if (ls->mEventType == NS_PAGE_UNLOAD ||
        ls->mEventType == NS_BEFORE_PAGE_UNLOAD) {
      return true;
    }
  }
  return false;
}

void
nsEventListenerManager::SetEventHandler(nsIAtom* aEventName,
                                        const nsAString& aTypeString,
                                        EventHandlerNonNull* aHandler)
{
  if (!aHandler) {
    RemoveEventHandler(aEventName, aTypeString);
    return;
  }

  // Untrusted events are always permitted for non-chrome script
  // handlers.
  SetEventHandlerInternal(JS::NullPtr(), aEventName,
                          aTypeString, nsEventHandler(aHandler),
                          !mIsMainThreadELM ||
                          !nsContentUtils::IsCallerChrome());
}

void
nsEventListenerManager::SetEventHandler(OnErrorEventHandlerNonNull* aHandler)
{
  if (mIsMainThreadELM) {
    if (!aHandler) {
      RemoveEventHandler(nsGkAtoms::onerror, EmptyString());
      return;
    }

    // Untrusted events are always permitted for non-chrome script
    // handlers.
    SetEventHandlerInternal(JS::NullPtr(), nsGkAtoms::onerror,
                            EmptyString(), nsEventHandler(aHandler),
                            !nsContentUtils::IsCallerChrome());
  } else {
    if (!aHandler) {
      RemoveEventHandler(nullptr, NS_LITERAL_STRING("error"));
      return;
    }

    // Untrusted events are always permitted.
    SetEventHandlerInternal(JS::NullPtr(), nullptr,
                            NS_LITERAL_STRING("error"),
                            nsEventHandler(aHandler), true);
  }
}

void
nsEventListenerManager::SetEventHandler(OnBeforeUnloadEventHandlerNonNull* aHandler)
{
  if (!aHandler) {
    RemoveEventHandler(nsGkAtoms::onbeforeunload, EmptyString());
    return;
  }

  // Untrusted events are always permitted for non-chrome script
  // handlers.
  SetEventHandlerInternal(JS::NullPtr(), nsGkAtoms::onbeforeunload,
                          EmptyString(), nsEventHandler(aHandler),
                          !mIsMainThreadELM ||
                          !nsContentUtils::IsCallerChrome());
}

const nsEventHandler*
nsEventListenerManager::GetEventHandlerInternal(nsIAtom *aEventName,
                                                const nsAString& aTypeString)
{
  uint32_t eventType = nsContentUtils::GetEventId(aEventName);
  nsListenerStruct* ls = FindEventHandler(eventType, aEventName, aTypeString);

  if (!ls) {
    return nullptr;
  }

  nsIJSEventListener *listener = ls->GetJSListener();
    
  if (ls->mHandlerIsString) {
    CompileEventHandlerInternal(ls, nullptr, nullptr);
  }

  const nsEventHandler& handler = listener->GetHandler();
  if (handler.HasEventHandler()) {
    return &handler;
  }

  return nullptr;
}

size_t
nsEventListenerManager::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf)
  const
{
  size_t n = aMallocSizeOf(this);
  n += mListeners.SizeOfExcludingThis(aMallocSizeOf);
  uint32_t count = mListeners.Length();
  for (uint32_t i = 0; i < count; ++i) {
    nsIJSEventListener* jsl = mListeners.ElementAt(i).GetJSListener();
    if (jsl) {
      n += jsl->SizeOfIncludingThis(aMallocSizeOf);
    }
  }
  return n;
}

void
nsEventListenerManager::MarkForCC()
{
  uint32_t count = mListeners.Length();
  for (uint32_t i = 0; i < count; ++i) {
    const nsListenerStruct& ls = mListeners.ElementAt(i);
    nsIJSEventListener* jsl = ls.GetJSListener();
    if (jsl) {
      if (jsl->GetHandler().HasEventHandler()) {
        JS::ExposeObjectToActiveJS(jsl->GetHandler().Ptr()->Callable());
      }
      if (JSObject* scope = jsl->GetEventScope()) {
        JS::ExposeObjectToActiveJS(scope);
      }
    } else if (ls.mListenerType == eWrappedJSListener) {
      xpc_TryUnmarkWrappedGrayObject(ls.mListener.GetXPCOMCallback());
    } else if (ls.mListenerType == eWebIDLListener) {
      // Callback() unmarks gray
      ls.mListener.GetWebIDLCallback()->Callback();
    }
  }
  if (mRefCnt.IsPurple()) {
    mRefCnt.RemovePurple();
  }
}

already_AddRefed<nsIScriptGlobalObject>
nsEventListenerManager::GetScriptGlobalAndDocument(nsIDocument** aDoc)
{
  nsCOMPtr<nsINode> node(do_QueryInterface(mTarget));
  nsCOMPtr<nsIDocument> doc;
  nsCOMPtr<nsIScriptGlobalObject> global;
  if (node) {
    // Try to get context from doc
    // XXX sXBL/XBL2 issue -- do we really want the owner here?  What
    // if that's the XBL document?
    doc = node->OwnerDoc();
    if (doc->IsLoadedAsData()) {
      return nullptr;
    }

    // We want to allow compiling an event handler even in an unloaded
    // document, so use GetScopeObject here, not GetScriptHandlingObject.
    global = do_QueryInterface(doc->GetScopeObject());
  } else {
    nsCOMPtr<nsPIDOMWindow> win = GetTargetAsInnerWindow();
    if (win) {
      doc = win->GetExtantDoc();
      global = do_QueryInterface(win);
    } else {
      global = do_QueryInterface(mTarget);
    }
  }

  doc.forget(aDoc);
  return global.forget();
}
