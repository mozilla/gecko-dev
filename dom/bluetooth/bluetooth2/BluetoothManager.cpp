/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"
#include "BluetoothReplyRunnable.h"
#include "BluetoothService.h"
#include "BluetoothUtils.h"

#include "mozilla/dom/bluetooth/BluetoothAdapter.h"
#include "mozilla/dom/bluetooth/BluetoothManager.h"
#include "mozilla/dom/bluetooth/BluetoothTypes.h"
#include "mozilla/dom/BluetoothManager2Binding.h"
#include "mozilla/Services.h"
#include "nsContentUtils.h"
#include "nsDOMClassInfo.h"
#include "nsIPermissionManager.h"
#include "nsThreadUtils.h"

using namespace mozilla;

USING_BLUETOOTH_NAMESPACE

NS_IMPL_CYCLE_COLLECTION_CLASS(BluetoothManager)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(BluetoothManager,
                                                DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mAdapters)

  /**
   * Unregister the bluetooth signal handler after unlinked.
   *
   * This is needed to avoid ending up with exposing a deleted object to JS or
   * accessing deleted objects while receiving signals from parent process
   * after unlinked. Please see Bug 1138267 for detail informations.
   */
  UnregisterBluetoothSignalHandler(NS_LITERAL_STRING(KEY_MANAGER), tmp);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(BluetoothManager,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mAdapters)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(BluetoothManager)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(BluetoothManager, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(BluetoothManager, DOMEventTargetHelper)

class GetAdaptersTask : public BluetoothReplyRunnable
{
 public:
  GetAdaptersTask(BluetoothManager* aManager)
    : BluetoothReplyRunnable(nullptr)
    , mManager(aManager)
  { }

  bool
  ParseSuccessfulReply(JS::MutableHandle<JS::Value> aValue)
  {
    /**
     * Unwrap BluetoothReply.BluetoothReplySuccess.BluetoothValue =
     *   BluetoothNamedValue[]
     *     |
     *     |__ BluetoothNamedValue =
     *     |     {"Adapter", BluetoothValue = BluetoothNamedValue[]}
     *     |
     *     |__ BluetoothNamedValue =
     *     |     {"Adapter", BluetoothValue = BluetoothNamedValue[]}
     *     ...
     */

    // Extract the array of all adapters' properties
    const BluetoothValue& adaptersProperties =
      mReply->get_BluetoothReplySuccess().value();
    NS_ENSURE_TRUE(adaptersProperties.type() ==
                   BluetoothValue::TArrayOfBluetoothNamedValue, false);

    const InfallibleTArray<BluetoothNamedValue>& adaptersPropertiesArray =
      adaptersProperties.get_ArrayOfBluetoothNamedValue();
    BT_API2_LOGR("GetAdaptersTask: len[%d]", adaptersPropertiesArray.Length());

    // Append a BluetoothAdapter into adapters array for each properties array
    uint32_t numAdapters = adaptersPropertiesArray.Length();
    for (uint32_t i = 0; i < numAdapters; i++) {
      MOZ_ASSERT(adaptersPropertiesArray[i].name().EqualsLiteral("Adapter"));

      const BluetoothValue& properties = adaptersPropertiesArray[i].value();
      mManager->AppendAdapter(properties);
    }

    aValue.setUndefined();
    return true;
  }

  virtual void
  ReleaseMembers() override
  {
    BluetoothReplyRunnable::ReleaseMembers();
    mManager = nullptr;
  }

private:
  nsRefPtr<BluetoothManager> mManager;
};

BluetoothManager::BluetoothManager(nsPIDOMWindow *aWindow)
  : DOMEventTargetHelper(aWindow)
  , mDefaultAdapterIndex(-1)
{
  MOZ_ASSERT(aWindow);

  RegisterBluetoothSignalHandler(NS_LITERAL_STRING(KEY_MANAGER), this);
  BT_API2_LOGR("aWindow %p", aWindow);

  // Query adapters list from bluetooth backend
  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  nsRefPtr<BluetoothReplyRunnable> result = new GetAdaptersTask(this);
  NS_ENSURE_SUCCESS_VOID(bs->GetAdaptersInternal(result));
}

BluetoothManager::~BluetoothManager()
{
  UnregisterBluetoothSignalHandler(NS_LITERAL_STRING(KEY_MANAGER), this);
}

void
BluetoothManager::DisconnectFromOwner()
{
  DOMEventTargetHelper::DisconnectFromOwner();
  UnregisterBluetoothSignalHandler(NS_LITERAL_STRING(KEY_MANAGER), this);
}

BluetoothAdapter*
BluetoothManager::GetDefaultAdapter()
{
  BT_API2_LOGR("mDefaultAdapterIndex: %d", mDefaultAdapterIndex);

  return DefaultAdapterExists() ? mAdapters[mDefaultAdapterIndex] : nullptr;
}

void
BluetoothManager::AppendAdapter(const BluetoothValue& aValue)
{
  MOZ_ASSERT(aValue.type() == BluetoothValue::TArrayOfBluetoothNamedValue);

  // Create a new BluetoothAdapter and append it to adapters array
  const InfallibleTArray<BluetoothNamedValue>& values =
    aValue.get_ArrayOfBluetoothNamedValue();
  nsRefPtr<BluetoothAdapter> adapter =
    BluetoothAdapter::Create(GetOwner(), values);

  mAdapters.AppendElement(adapter);

  // Set this adapter as default adapter if no adapter exists
  if (!DefaultAdapterExists()) {
    MOZ_ASSERT(mAdapters.Length() == 1);
    ReselectDefaultAdapter();
  }
}

void
BluetoothManager::GetAdapters(nsTArray<nsRefPtr<BluetoothAdapter> >& aAdapters)
{
  aAdapters = mAdapters;
}

// static
already_AddRefed<BluetoothManager>
BluetoothManager::Create(nsPIDOMWindow* aWindow)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWindow);

  nsRefPtr<BluetoothManager> manager = new BluetoothManager(aWindow);
  return manager.forget();
}

void
BluetoothManager::HandleAdapterAdded(const BluetoothValue& aValue)
{
  MOZ_ASSERT(aValue.type() == BluetoothValue::TArrayOfBluetoothNamedValue);
  BT_API2_LOGR();

  AppendAdapter(aValue);

  // Notify application of added adapter
  BluetoothAdapterEventInit init;
  init.mAdapter = mAdapters.LastElement();
  DispatchAdapterEvent(NS_LITERAL_STRING("adapteradded"), init);
}

void
BluetoothManager::HandleAdapterRemoved(const BluetoothValue& aValue)
{
  MOZ_ASSERT(aValue.type() == BluetoothValue::TnsString);
  MOZ_ASSERT(DefaultAdapterExists());

  // Remove the adapter of given address from adapters array
  nsString addressToRemove = aValue.get_nsString();

  uint32_t i;
  for (i = 0; i < mAdapters.Length(); i++) {
    nsString address;
    mAdapters[i]->GetAddress(address);
    if (address.Equals(addressToRemove)) {
      mAdapters.RemoveElementAt(i);
      break;
    }
  }

  // Notify application of removed adapter
  BluetoothAdapterEventInit init;
  init.mAddress = addressToRemove;
  DispatchAdapterEvent(NS_LITERAL_STRING("adapterremoved"), init);

  // Reselect default adapter if it's removed
  if (mDefaultAdapterIndex == (int)i) {
    ReselectDefaultAdapter();
  }
}

void
BluetoothManager::ReselectDefaultAdapter()
{
  // Select the first of existing/remaining adapters as default adapter
  mDefaultAdapterIndex = mAdapters.IsEmpty() ? -1 : 0;
  BT_API2_LOGR("mAdapters length: %d => NEW mDefaultAdapterIndex: %d",
               mAdapters.Length(), mDefaultAdapterIndex);

  // Notify application of default adapter change
  DispatchAttributeEvent();
}

void
BluetoothManager::DispatchAdapterEvent(const nsAString& aType,
                                       const BluetoothAdapterEventInit& aInit)
{
  BT_API2_LOGR("aType (%s)", NS_ConvertUTF16toUTF8(aType).get());

  nsRefPtr<BluetoothAdapterEvent> event =
    BluetoothAdapterEvent::Constructor(this, aType, aInit);
  DispatchTrustedEvent(event);
}

void
BluetoothManager::DispatchAttributeEvent()
{
  MOZ_ASSERT(NS_IsMainThread());
  BT_API2_LOGR();

  Sequence<nsString> types;
  BT_APPEND_ENUM_STRING_FALLIBLE(types,
                                 BluetoothManagerAttribute,
                                 BluetoothManagerAttribute::DefaultAdapter);

  // Notify application of default adapter change
  BluetoothAttributeEventInit init;
  init.mAttrs = types;
  nsRefPtr<BluetoothAttributeEvent> event =
    BluetoothAttributeEvent::Constructor(
      this, NS_LITERAL_STRING(ATTRIBUTE_CHANGED_ID), init);

  DispatchTrustedEvent(event);
}

void
BluetoothManager::Notify(const BluetoothSignal& aData)
{
  BT_LOGD("[M] %s", NS_ConvertUTF16toUTF8(aData.name()).get());
  NS_ENSURE_TRUE_VOID(mSignalRegistered);

  if (aData.name().EqualsLiteral("AdapterAdded")) {
    HandleAdapterAdded(aData.value());
  } else if (aData.name().EqualsLiteral("AdapterRemoved")) {
    HandleAdapterRemoved(aData.value());
  } else {
    BT_WARNING("Not handling manager signal: %s",
               NS_ConvertUTF16toUTF8(aData.name()).get());
  }
}

JSObject*
BluetoothManager::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return BluetoothManagerBinding::Wrap(aCx, this, aGivenProto);
}
