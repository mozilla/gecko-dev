/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* vim: set ts=2 sw=2 et tw=78:
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef InkCollector_h__
#define InkCollector_h__

#include <msinkaut.h>
#include "mozilla/StaticPtr.h"

#define MOZ_WM_PEN_LEAVES_HOVER_OF_DIGITIZER  WM_USER + 0x83

class InkCollectorEvent : public _IInkCollectorEvents
{
public:
  // IUnknown
  HRESULT __stdcall QueryInterface(REFIID aRiid, void **aObject);
  virtual ULONG STDMETHODCALLTYPE AddRef() { return ++mRefCount; }
  virtual ULONG STDMETHODCALLTYPE Release()
  {
    MOZ_ASSERT(mRefCount);
    if (!--mRefCount) {
      delete this;
      return 0;
    }
    return mRefCount;
  }

protected:
  // IDispatch
  STDMETHOD(GetTypeInfoCount)(UINT* aInfo) { return E_NOTIMPL; }
  STDMETHOD(GetTypeInfo)(UINT aInfo, LCID aId, ITypeInfo** aTInfo) { return E_NOTIMPL; }
  STDMETHOD(GetIDsOfNames)(REFIID aRiid, LPOLESTR* aStrNames, UINT aNames,
                           LCID aId, DISPID* aDispId) { return E_NOTIMPL; }
  STDMETHOD(Invoke)(DISPID aDispIdMember, REFIID aRiid,
                    LCID aId, WORD wFlags,
                    DISPPARAMS* aDispParams, VARIANT* aVarResult,
                    EXCEPINFO* aExcepInfo, UINT* aArgErr);

  // InkCollectorEvent
  void CursorOutOfRange(IInkCursor* aCursor) const;
  bool IsHardProximityTablet(IInkTablet* aTablet) const;

private:
  uint32_t  mRefCount = 0;
};

class InkCollector
{
public:
  ~InkCollector();
  void Shutdown();

  HWND GetTarget();
  void SetTarget(HWND aTargetWindow);
  void ClearTarget();

  static StaticAutoPtr<InkCollector> sInkCollector;

protected:
  void Initialize();
  void OnInitialize();
  void Enable(bool aNewState);

private:
  nsRefPtr<IUnknown>          mMarshaller;
  nsRefPtr<IInkCollector>     mInkCollector;
  nsRefPtr<IConnectionPoint>  mConnectionPoint;
  nsRefPtr<InkCollectorEvent> mInkCollectorEvent;

  HWND                        mTargetWindow     = 0;
  DWORD                       mCookie           = 0;
  bool                        mComInitialized   = false;
  bool                        mEnabled          = false;
};

#endif // InkCollector_h__
