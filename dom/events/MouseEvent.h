/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MouseEvent_h_
#define mozilla_dom_MouseEvent_h_

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/UIEvent.h"
#include "mozilla/dom/MouseEventBinding.h"
#include "mozilla/EventForwards.h"

namespace mozilla::dom {

class MouseEvent : public UIEvent {
 public:
  MouseEvent(EventTarget* aOwner, nsPresContext* aPresContext,
             WidgetMouseEventBase* aEvent);

  NS_INLINE_DECL_REFCOUNTING_INHERITED(MouseEvent, UIEvent)

  virtual JSObject* WrapObjectInternal(
      JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override {
    return MouseEvent_Binding::Wrap(aCx, this, aGivenProto);
  }

  virtual MouseEvent* AsMouseEvent() override { return this; }

  MOZ_CAN_RUN_SCRIPT_BOUNDARY void DuplicatePrivateData() override;

  // Web IDL binding methods
  virtual uint32_t Which(CallerType aCallerType) override {
    return Button() + 1;
  }

  already_AddRefed<nsIScreen> GetScreen();

  /**
   * Return screenX and screenY values for this event in CSS pixels.
   * If current setting allows to expose fractional coordinates for the event,
   * this returns the fractional values as-is.  Otherwise, this returns
   * integer values with rounding the computed values.  Note that if this
   * event is untrusted one and should not expose fractional values, the
   * initialized values are floored before computing the values as defined by
   * Pointer Events spec.
   */
  CSSDoublePoint ScreenPoint(CallerType) const;
  double ScreenX(CallerType aCallerType) const {
    return ScreenPoint(aCallerType).x;
  }
  double ScreenY(CallerType aCallerType) const {
    return ScreenPoint(aCallerType).y;
  }
  LayoutDeviceIntPoint ScreenPointLayoutDevicePix() const;
  DesktopIntPoint ScreenPointDesktopPix() const;

  /**
   * Return pageX and pageY values for this event in CSS pixels which are
   * client point + scroll position of the root scrollable frame.
   * If current setting allows to expose fractional coordinates for the event,
   * this returns the fractional values as-is.  Otherwise, this returns
   * integer values with rounding the computed values.  Note that if this
   * event is untrusted one and should not expose fractional values, the
   * initialized values are floored before computing the values as defined by
   * Pointer Events spec.
   */
  CSSDoublePoint PagePoint() const;
  double PageX() const { return PagePoint().x; }
  double PageY() const { return PagePoint().y; }

  /**
   * Return clientX and clientY values for this event in CSS pixels.
   * If current setting allows to expose fractional coordinates for the event,
   * this returns the fractional values as-is.  Otherwise, this returns
   * integer values with rounding the computed values.  Note that if this
   * event is untrusted one and should not expose fractional values, the
   * initialized values are floored before computing the values as defined by
   * Pointer Events spec.
   */
  CSSDoublePoint ClientPoint() const;
  double ClientX() const { return ClientPoint().x; }
  double ClientY() const { return ClientPoint().y; }

  /**
   * Return offsetX and offsetY values for this event in CSS pixels which are
   * offset in the target element.
   * If current setting allows to expose fractional coordinates for the event,
   * this returns the fractional values as-is.  Otherwise, this returns
   * integer values with rounding the computed values.  Note that if this
   * event is untrusted one and should not expose fractional values, the
   * initialized values are floored before computing the values as defined by
   * Pointer Events spec.
   *
   * Note that this may flush the pending layout.
   */
  MOZ_CAN_RUN_SCRIPT_BOUNDARY CSSDoublePoint OffsetPoint() const;
  double OffsetX() const { return OffsetPoint().x; }
  double OffsetY() const { return OffsetPoint().y; }

  bool CtrlKey();
  bool ShiftKey();
  bool AltKey();
  bool MetaKey();
  int16_t Button();
  uint16_t Buttons();
  already_AddRefed<EventTarget> GetRelatedTarget();
  void InitMouseEvent(const nsAString& aType, bool aCanBubble, bool aCancelable,
                      nsGlobalWindowInner* aView, int32_t aDetail,
                      int32_t aScreenX, int32_t aScreenY, int32_t aClientX,
                      int32_t aClientY, bool aCtrlKey, bool aAltKey,
                      bool aShiftKey, bool aMetaKey, uint16_t aButton,
                      EventTarget* aRelatedTarget) {
    InitMouseEventInternal(aType, aCanBubble, aCancelable, aView, aDetail,
                           aScreenX, aScreenY, aClientX, aClientY, aCtrlKey,
                           aAltKey, aShiftKey, aMetaKey, aButton,
                           aRelatedTarget);
  }

  void InitializeExtraMouseEventDictionaryMembers(const MouseEventInit& aParam);

  bool GetModifierState(const nsAString& aKeyArg) {
    return GetModifierStateInternal(aKeyArg);
  }
  static already_AddRefed<MouseEvent> Constructor(const GlobalObject& aGlobal,
                                                  const nsAString& aType,
                                                  const MouseEventInit& aParam);
  int32_t MovementX() { return GetMovementPoint().x; }
  int32_t MovementY() { return GetMovementPoint().y; }
  float MozPressure(CallerType) const;
  uint16_t InputSource(CallerType) const;
  void InitNSMouseEvent(const nsAString& aType, bool aCanBubble,
                        bool aCancelable, nsGlobalWindowInner* aView,
                        int32_t aDetail, int32_t aScreenX, int32_t aScreenY,
                        int32_t aClientX, int32_t aClientY, bool aCtrlKey,
                        bool aAltKey, bool aShiftKey, bool aMetaKey,
                        uint16_t aButton, EventTarget* aRelatedTarget,
                        float aPressure, uint16_t aInputSource);
  void PreventClickEvent();
  bool ClickEventPrevented();

 protected:
  ~MouseEvent() = default;

  nsIntPoint GetMovementPoint() const;

  void InitMouseEventInternal(const nsAString& aType, bool aCanBubble,
                              bool aCancelable, nsGlobalWindowInner* aView,
                              int32_t aDetail, double aScreenX, double aScreenY,
                              double aClientX, double aClientY, bool aCtrlKey,
                              bool aAltKey, bool aShiftKey, bool aMetaKey,
                              uint16_t aButton, EventTarget* aRelatedTarget);

  void InitMouseEventInternal(const nsAString& aType, bool aCanBubble,
                              bool aCancelable, nsGlobalWindowInner* aView,
                              int32_t aDetail, double aScreenX, double aScreenY,
                              double aClientX, double aClientY, int16_t aButton,
                              EventTarget* aRelatedTarget,
                              const nsAString& aModifiersList);

  // mWidgetRelativePoint  stores the reference point of the event within the
  // double coordinates.  If this is a trusted event, the values are copied from
  // mEvent->mRefPoint whose type is LayoutDeviceIntPoint.  Therefore, the
  // values are always integer.  On the other hand, if this is an untrusted
  // event, this may store fractional values if and only if the event should
  // expose fractional coordinates.  Otherwise, this is floored values for the
  // backward compatibility.
  LayoutDeviceDoublePoint mWidgetRelativePoint;

  // If this is a trusted event and after dispatching this, mDefaultClientPoint
  // stores the clientX and clientY values at duplicating the data.
  // If this is an untrusted event, mDefaultClientPoint stores the clientX and
  // clientY inputs.  If this event should expose fractional coordinates, the
  // values are set as-is.  Otherwise, this stores floored input values for
  // the backward compatibility.
  CSSDoublePoint mDefaultClientPoint;

  // If this is a trusted event and after dispatching this, mPagePoint stores
  // the pageX and pageY values at duplicating the data.
  // If this is an untrusted event, mPagePoint stores the pageX and pageY
  // inputs. If this event should expose fractional coordinates, the values are
  // set as-is.  Otherwise, this stores floored input values for the backward
  // compatibility.
  CSSDoublePoint mPagePoint;

  nsIntPoint mMovementPoint;
  bool mUseFractionalCoords = false;
};

}  // namespace mozilla::dom

already_AddRefed<mozilla::dom::MouseEvent> NS_NewDOMMouseEvent(
    mozilla::dom::EventTarget* aOwner, nsPresContext* aPresContext,
    mozilla::WidgetMouseEvent* aEvent);

#endif  // mozilla_dom_MouseEvent_h_
