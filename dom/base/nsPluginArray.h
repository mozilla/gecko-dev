/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPluginArray_h___
#define nsPluginArray_h___

#include "nsTArray.h"
#include "nsWeakReference.h"
#include "nsIObserver.h"
#include "nsWrapperCache.h"
#include "nsPluginTags.h"
#include "nsPIDOMWindow.h"

class nsPluginElement;
class nsMimeType;

class nsPluginArray final : public nsIObserver,
                            public nsSupportsWeakReference,
                            public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(nsPluginArray,
                                                         nsIObserver)

  // nsIObserver
  NS_DECL_NSIOBSERVER

  explicit nsPluginArray(nsPIDOMWindow* aWindow);
  nsPIDOMWindow* GetParentObject() const;
  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  // nsPluginArray registers itself as an observer with a weak reference.
  // This can't be done in the constructor, because at that point its
  // refcount is 0 (and it gets destroyed upon registration). So, Init()
  // must be called after construction.
  void Init();
  void Invalidate();

  void GetMimeTypes(nsTArray<nsRefPtr<nsMimeType>>& aMimeTypes);

  // PluginArray WebIDL methods

  nsPluginElement* Item(uint32_t aIndex);
  nsPluginElement* NamedItem(const nsAString& aName);
  void Refresh(bool aReloadDocuments);
  nsPluginElement* IndexedGetter(uint32_t aIndex, bool &aFound);
  nsPluginElement* NamedGetter(const nsAString& aName, bool &aFound);
  bool NameIsEnumerable(const nsAString& aName);
  uint32_t Length();
  void GetSupportedNames(unsigned, nsTArray<nsString>& aRetval);

private:
  virtual ~nsPluginArray();

  bool AllowPlugins() const;
  void EnsurePlugins();

  nsCOMPtr<nsPIDOMWindow> mWindow;
  nsTArray<nsRefPtr<nsPluginElement> > mPlugins;
};

class nsPluginElement final : public nsISupports,
                              public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(nsPluginElement)

  nsPluginElement(nsPIDOMWindow* aWindow, nsPluginTag* aPluginTag);

  nsPIDOMWindow* GetParentObject() const;
  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  nsPluginTag* PluginTag() const
  {
    return mPluginTag;
  }

  // Plugin WebIDL methods

  void GetDescription(nsString& retval) const;
  void GetFilename(nsString& retval) const;
  void GetVersion(nsString& retval) const;
  void GetName(nsString& retval) const;
  nsMimeType* Item(uint32_t index);
  nsMimeType* NamedItem(const nsAString& name);
  nsMimeType* IndexedGetter(uint32_t index, bool &found);
  nsMimeType* NamedGetter(const nsAString& name, bool &found);
  bool NameIsEnumerable(const nsAString& aName);
  uint32_t Length();
  void GetSupportedNames(unsigned, nsTArray<nsString>& retval);

  nsTArray<nsRefPtr<nsMimeType> >& MimeTypes();

protected:
  ~nsPluginElement();

  void EnsurePluginMimeTypes();

  nsCOMPtr<nsPIDOMWindow> mWindow;
  nsRefPtr<nsPluginTag> mPluginTag;
  nsTArray<nsRefPtr<nsMimeType> > mMimeTypes;
};

#endif /* nsPluginArray_h___ */
