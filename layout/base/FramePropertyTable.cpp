/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FramePropertyTable.h"

#include "mozilla/MemoryReporting.h"

namespace mozilla {

void
FramePropertyTable::SetInternal(
  const nsIFrame* aFrame, UntypedDescriptor aProperty, void* aValue)
{
  NS_ASSERTION(aFrame, "Null frame?");
  NS_ASSERTION(aProperty, "Null property?");

  if (mLastFrame != aFrame || !mLastEntry) {
    mLastFrame = aFrame;
    mLastEntry = mEntries.PutEntry(aFrame);
  }
  Entry* entry = mLastEntry;

  if (!entry->mProp.IsArray()) {
    if (!entry->mProp.mProperty) {
      // Empty entry, so we can just store our property in the empty slot
      entry->mProp.mProperty = aProperty;
      entry->mProp.mValue = aValue;
      return;
    }
    if (entry->mProp.mProperty == aProperty) {
      // Just overwrite the current value
      entry->mProp.DestroyValueFor(aFrame);
      entry->mProp.mValue = aValue;
      return;
    }

    // We need to expand the single current entry to an array
    PropertyValue current = entry->mProp;
    entry->mProp.mProperty = nullptr;
    static_assert(sizeof(nsTArray<PropertyValue>) <= sizeof(void *),
                  "Property array must fit entirely within entry->mProp.mValue");
    new (&entry->mProp.mValue) nsTArray<PropertyValue>(4);
    entry->mProp.ToArray()->AppendElement(current);
  }

  nsTArray<PropertyValue>* array = entry->mProp.ToArray();
  nsTArray<PropertyValue>::index_type index =
    array->IndexOf(aProperty, 0, PropertyComparator());
  if (index != nsTArray<PropertyValue>::NoIndex) {
    PropertyValue* pv = &array->ElementAt(index);
    pv->DestroyValueFor(aFrame);
    pv->mValue = aValue;
    return;
  }

  array->AppendElement(PropertyValue(aProperty, aValue));
}

void*
FramePropertyTable::GetInternal(
  const nsIFrame* aFrame, UntypedDescriptor aProperty, bool* aFoundResult)
{
  NS_ASSERTION(aFrame, "Null frame?");
  NS_ASSERTION(aProperty, "Null property?");

  if (aFoundResult) {
    *aFoundResult = false;
  }

  if (mLastFrame != aFrame) {
    mLastFrame = aFrame;
    mLastEntry = mEntries.GetEntry(mLastFrame);
  }
  Entry* entry = mLastEntry;
  if (!entry)
    return nullptr;

  if (entry->mProp.mProperty == aProperty) {
    if (aFoundResult) {
      *aFoundResult = true;
    }
    return entry->mProp.mValue;
  }
  if (!entry->mProp.IsArray()) {
    // There's just one property and it's not the one we want, bail
    return nullptr;
  }

  nsTArray<PropertyValue>* array = entry->mProp.ToArray();
  nsTArray<PropertyValue>::index_type index =
    array->IndexOf(aProperty, 0, PropertyComparator());
  if (index == nsTArray<PropertyValue>::NoIndex)
    return nullptr;

  if (aFoundResult) {
    *aFoundResult = true;
  }

  return array->ElementAt(index).mValue;
}

void*
FramePropertyTable::RemoveInternal(
  const nsIFrame* aFrame, UntypedDescriptor aProperty, bool* aFoundResult)
{
  NS_ASSERTION(aFrame, "Null frame?");
  NS_ASSERTION(aProperty, "Null property?");

  if (aFoundResult) {
    *aFoundResult = false;
  }

  if (mLastFrame != aFrame) {
    mLastFrame = aFrame;
    mLastEntry = mEntries.GetEntry(aFrame);
  }
  Entry* entry = mLastEntry;
  if (!entry)
    return nullptr;

  if (entry->mProp.mProperty == aProperty) {
    // There's only one entry and it's the one we want
    void* value = entry->mProp.mValue;

    // Here it's ok to use RemoveEntry() -- which may resize mEntries --
    // because we null mLastEntry at the same time.
    mEntries.RemoveEntry(entry);
    mLastEntry = nullptr;
    if (aFoundResult) {
      *aFoundResult = true;
    }
    return value;
  }
  if (!entry->mProp.IsArray()) {
    // There's just one property and it's not the one we want, bail
    return nullptr;
  }

  nsTArray<PropertyValue>* array = entry->mProp.ToArray();
  nsTArray<PropertyValue>::index_type index =
    array->IndexOf(aProperty, 0, PropertyComparator());
  if (index == nsTArray<PropertyValue>::NoIndex) {
    // No such property, bail
    return nullptr;
  }

  if (aFoundResult) {
    *aFoundResult = true;
  }

  void* result = array->ElementAt(index).mValue;

  uint32_t last = array->Length() - 1;
  array->ElementAt(index) = array->ElementAt(last);
  array->RemoveElementAt(last);

  if (last == 1) {
    PropertyValue pv = array->ElementAt(0);
    array->~nsTArray<PropertyValue>();
    entry->mProp = pv;
  }
  
  return result;
}

void
FramePropertyTable::DeleteInternal(
  const nsIFrame* aFrame, UntypedDescriptor aProperty)
{
  NS_ASSERTION(aFrame, "Null frame?");
  NS_ASSERTION(aProperty, "Null property?");

  bool found;
  void* v = RemoveInternal(aFrame, aProperty, &found);
  if (found) {
    PropertyValue pv(aProperty, v);
    pv.DestroyValueFor(aFrame);
  }
}

/* static */ void
FramePropertyTable::DeleteAllForEntry(Entry* aEntry)
{
  if (!aEntry->mProp.IsArray()) {
    aEntry->mProp.DestroyValueFor(aEntry->GetKey());
    return;
  }

  nsTArray<PropertyValue>* array = aEntry->mProp.ToArray();
  for (uint32_t i = 0; i < array->Length(); ++i) {
    array->ElementAt(i).DestroyValueFor(aEntry->GetKey());
  }
  array->~nsTArray<PropertyValue>();
}

void
FramePropertyTable::DeleteAllFor(const nsIFrame* aFrame)
{
  NS_ASSERTION(aFrame, "Null frame?");

  Entry* entry = mEntries.GetEntry(aFrame);
  if (!entry)
    return;

  if (mLastFrame == aFrame) {
    // Flush cache. We assume DeleteAllForEntry will be called before
    // a frame is destroyed.
    mLastFrame = nullptr;
    mLastEntry = nullptr;
  }

  DeleteAllForEntry(entry);

  // mLastEntry points into mEntries, so we use RawRemoveEntry() which will not
  // resize mEntries.
  mEntries.RawRemoveEntry(entry);
}

void
FramePropertyTable::DeleteAll()
{
  mLastFrame = nullptr;
  mLastEntry = nullptr;

  for (auto iter = mEntries.Iter(); !iter.Done(); iter.Next()) {
    DeleteAllForEntry(iter.Get());
  }
  mEntries.Clear();
}

size_t
FramePropertyTable::SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  return mEntries.SizeOfExcludingThis(aMallocSizeOf);
}

} // namespace mozilla
