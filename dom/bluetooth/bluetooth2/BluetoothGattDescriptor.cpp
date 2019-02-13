/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothReplyRunnable.h"
#include "BluetoothService.h"
#include "BluetoothUtils.h"
#include "mozilla/dom/BluetoothGattDescriptorBinding.h"
#include "mozilla/dom/bluetooth/BluetoothCommon.h"
#include "mozilla/dom/bluetooth/BluetoothGattCharacteristic.h"
#include "mozilla/dom/bluetooth/BluetoothGattDescriptor.h"
#include "mozilla/dom/bluetooth/BluetoothGattService.h"
#include "mozilla/dom/bluetooth/BluetoothTypes.h"

using namespace mozilla;
using namespace mozilla::dom;

USING_BLUETOOTH_NAMESPACE

NS_IMPL_CYCLE_COLLECTION_CLASS(BluetoothGattDescriptor)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(BluetoothGattDescriptor)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mOwner)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCharacteristic)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER

  /**
   * Unregister the bluetooth signal handler after unlinked.
   *
   * This is needed to avoid ending up with exposing a deleted object to JS or
   * accessing deleted objects while receiving signals from parent process
   * after unlinked. Please see Bug 1138267 for detail informations.
   */
  nsString path;
  GeneratePathFromGattId(tmp->mDescriptorId, path);
  UnregisterBluetoothSignalHandler(path, tmp);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(BluetoothGattDescriptor)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOwner)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCharacteristic)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(BluetoothGattDescriptor)

NS_IMPL_CYCLE_COLLECTING_ADDREF(BluetoothGattDescriptor)
NS_IMPL_CYCLE_COLLECTING_RELEASE(BluetoothGattDescriptor)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(BluetoothGattDescriptor)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

BluetoothGattDescriptor::BluetoothGattDescriptor(
  nsPIDOMWindow* aOwner,
  BluetoothGattCharacteristic* aCharacteristic,
  const BluetoothGattId& aDescriptorId)
  : mOwner(aOwner)
  , mCharacteristic(aCharacteristic)
  , mDescriptorId(aDescriptorId)
{
  MOZ_ASSERT(aOwner);
  MOZ_ASSERT(aCharacteristic);

  UuidToString(mDescriptorId.mUuid, mUuidStr);

  // Generate bluetooth signal path of this descriptor to applications
  nsString path;
  GeneratePathFromGattId(mDescriptorId, path);
  RegisterBluetoothSignalHandler(path, this);
}

BluetoothGattDescriptor::~BluetoothGattDescriptor()
{
  nsString path;
  GeneratePathFromGattId(mDescriptorId, path);
  UnregisterBluetoothSignalHandler(path, this);
}

void
BluetoothGattDescriptor::HandleDescriptorValueUpdated(
  const BluetoothValue& aValue)
{
  MOZ_ASSERT(aValue.type() == BluetoothValue::TArrayOfuint8_t);

  mValue = aValue.get_ArrayOfuint8_t();
}

void
BluetoothGattDescriptor::Notify(const BluetoothSignal& aData)
{
  BT_LOGD("[D] %s", NS_ConvertUTF16toUTF8(aData.name()).get());
  NS_ENSURE_TRUE_VOID(mSignalRegistered);

  BluetoothValue v = aData.value();
  if (aData.name().EqualsLiteral("DescriptorValueUpdated")) {
    HandleDescriptorValueUpdated(v);
  } else {
    BT_WARNING("Not handling GATT Descriptor signal: %s",
               NS_ConvertUTF16toUTF8(aData.name()).get());
  }
}

JSObject*
BluetoothGattDescriptor::WrapObject(JSContext* aContext,
                                    JS::Handle<JSObject*> aGivenProto)
{
  return BluetoothGattDescriptorBinding::Wrap(aContext, this, aGivenProto);
}

void
BluetoothGattDescriptor::GetValue(JSContext* cx,
                                  JS::MutableHandle<JSObject*> aValue) const
{
  aValue.set(mValue.IsEmpty()
             ? nullptr
             : ArrayBuffer::Create(cx, mValue.Length(), mValue.Elements()));
}

class ReadValueTask final : public BluetoothReplyRunnable
{
public:
  ReadValueTask(BluetoothGattDescriptor* aDescriptor, Promise* aPromise)
    : BluetoothReplyRunnable(
        nullptr, aPromise,
        NS_LITERAL_STRING("GattClientReadDescriptorValue"))
    , mDescriptor(aDescriptor)
  {
    MOZ_ASSERT(aDescriptor);
    MOZ_ASSERT(aPromise);
  }

  bool
  ParseSuccessfulReply(JS::MutableHandle<JS::Value> aValue)
  {
    aValue.setUndefined();

    const BluetoothValue& v = mReply->get_BluetoothReplySuccess().value();
    NS_ENSURE_TRUE(v.type() == BluetoothValue::TArrayOfuint8_t, false);

    AutoJSAPI jsapi;
    NS_ENSURE_TRUE(jsapi.Init(mDescriptor->GetParentObject()), false);

    JSContext* cx = jsapi.cx();
    if (!ToJSValue(cx, v.get_ArrayOfuint8_t(), aValue)) {
      JS_ClearPendingException(cx);
      return false;
    }

    return true;
  }

  void
  ReleaseMembers()
  {
    BluetoothReplyRunnable::ReleaseMembers();
    mDescriptor = nullptr;
  }

private:
  nsRefPtr<BluetoothGattDescriptor> mDescriptor;
};

already_AddRefed<Promise>
BluetoothGattDescriptor::ReadValue(ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  BluetoothService* bs = BluetoothService::Get();
  BT_ENSURE_TRUE_REJECT(bs, promise, NS_ERROR_NOT_AVAILABLE);

  nsRefPtr<BluetoothReplyRunnable> result = new ReadValueTask(this, promise);
  bs->GattClientReadDescriptorValueInternal(
    mCharacteristic->Service()->GetAppUuid(),
    mCharacteristic->Service()->GetServiceId(),
    mCharacteristic->GetCharacteristicId(),
    mDescriptorId,
    result);

  return promise.forget();
}

already_AddRefed<Promise>
BluetoothGattDescriptor::WriteValue(
  const RootedTypedArray<ArrayBuffer>& aValue, ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  aValue.ComputeLengthAndData();

  nsTArray<uint8_t> value;
  value.AppendElements(aValue.Data(), aValue.Length());

  BluetoothService* bs = BluetoothService::Get();
  BT_ENSURE_TRUE_REJECT(bs, promise, NS_ERROR_NOT_AVAILABLE);

  nsRefPtr<BluetoothReplyRunnable> result = new BluetoothVoidReplyRunnable(
    nullptr, promise, NS_LITERAL_STRING("GattClientWriteDescriptorValue"));
  bs->GattClientWriteDescriptorValueInternal(
    mCharacteristic->Service()->GetAppUuid(),
    mCharacteristic->Service()->GetServiceId(),
    mCharacteristic->GetCharacteristicId(),
    mDescriptorId,
    value,
    result);

  return promise.forget();
}
