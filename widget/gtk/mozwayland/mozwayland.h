/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Wayland compatibility header, it makes Firefox build with
   wayland-1.2 and Gtk+ 3.10.
*/

#ifndef __MozWayland_h_
#define __MozWayland_h_

#include "mozilla/Types.h"
#include <gtk/gtk.h>
#include <gdk/gdkwayland.h>

#ifdef __cplusplus
extern "C" {
#endif

MOZ_EXPORT struct wl_display* wl_display_connect(const char* name);
MOZ_EXPORT int wl_display_roundtrip_queue(struct wl_display* display,
                                          struct wl_event_queue* queue);
MOZ_EXPORT uint32_t wl_proxy_get_version(struct wl_proxy* proxy);
MOZ_EXPORT void wl_proxy_marshal(struct wl_proxy* p, uint32_t opcode, ...);
MOZ_EXPORT struct wl_proxy* wl_proxy_marshal_constructor(
    struct wl_proxy* proxy, uint32_t opcode,
    const struct wl_interface* interface, ...);
MOZ_EXPORT struct wl_proxy* wl_proxy_marshal_constructor_versioned(
    struct wl_proxy* proxy, uint32_t opcode,
    const struct wl_interface* interface, uint32_t version, ...);
MOZ_EXPORT struct wl_proxy* wl_proxy_marshal_flags(
    struct wl_proxy* proxy, uint32_t opcode,
    const struct wl_interface* interface, uint32_t version, uint32_t flags,
    ...);
MOZ_EXPORT void wl_proxy_destroy(struct wl_proxy* proxy);
MOZ_EXPORT void* wl_proxy_create_wrapper(void* proxy);
MOZ_EXPORT void wl_proxy_wrapper_destroy(void* proxy_wrapper);

#ifndef WL_MARSHAL_FLAG_DESTROY
#  define WL_MARSHAL_FLAG_DESTROY (1 << 0)
#endif

#ifndef WL_SURFACE_DAMAGE_BUFFER_SINCE_VERSION
#  define WL_SURFACE_DAMAGE_BUFFER_SINCE_VERSION 4
#endif

/* We need implement some missing functions from wayland-client-protocol.h
 */
#ifndef WL_DATA_DEVICE_MANAGER_DND_ACTION_ENUM
enum wl_data_device_manager_dnd_action {
  WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE = 0,
  WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY = 1,
  WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE = 2,
  WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK = 4
};
#endif

#ifndef WL_DATA_OFFER_SET_ACTIONS
#  define WL_DATA_OFFER_SET_ACTIONS 4

struct moz_wl_data_offer_listener {
  void (*offer)(void* data, struct wl_data_offer* wl_data_offer,
                const char* mime_type);
  void (*source_actions)(void* data, struct wl_data_offer* wl_data_offer,
                         uint32_t source_actions);
  void (*action)(void* data, struct wl_data_offer* wl_data_offer,
                 uint32_t dnd_action);
};

static inline void wl_data_offer_set_actions(
    struct wl_data_offer* wl_data_offer, uint32_t dnd_actions,
    uint32_t preferred_action) {
  wl_proxy_marshal((struct wl_proxy*)wl_data_offer, WL_DATA_OFFER_SET_ACTIONS,
                   dnd_actions, preferred_action);
}
#else
typedef struct wl_data_offer_listener moz_wl_data_offer_listener;
#endif

#ifndef WL_SUBCOMPOSITOR_GET_SUBSURFACE
#  define WL_SUBCOMPOSITOR_GET_SUBSURFACE 1
struct wl_subcompositor;

// Emulate what mozilla header wrapper does - make the
// wl_subcompositor_interface always visible.
#  pragma GCC visibility push(default)
extern const struct wl_interface wl_subsurface_interface;
extern const struct wl_interface wl_subcompositor_interface;
#  pragma GCC visibility pop

#  define WL_SUBSURFACE_DESTROY 0
#  define WL_SUBSURFACE_SET_POSITION 1
#  define WL_SUBSURFACE_PLACE_ABOVE 2
#  define WL_SUBSURFACE_PLACE_BELOW 3
#  define WL_SUBSURFACE_SET_SYNC 4
#  define WL_SUBSURFACE_SET_DESYNC 5

static inline struct wl_subsurface* wl_subcompositor_get_subsurface(
    struct wl_subcompositor* wl_subcompositor, struct wl_surface* surface,
    struct wl_surface* parent) {
  struct wl_proxy* id;

  id = wl_proxy_marshal_constructor(
      (struct wl_proxy*)wl_subcompositor, WL_SUBCOMPOSITOR_GET_SUBSURFACE,
      &wl_subsurface_interface, NULL, surface, parent);

  return (struct wl_subsurface*)id;
}

static inline void wl_subsurface_set_position(
    struct wl_subsurface* wl_subsurface, int32_t x, int32_t y) {
  wl_proxy_marshal((struct wl_proxy*)wl_subsurface, WL_SUBSURFACE_SET_POSITION,
                   x, y);
}

static inline void wl_subsurface_set_desync(
    struct wl_subsurface* wl_subsurface) {
  wl_proxy_marshal((struct wl_proxy*)wl_subsurface, WL_SUBSURFACE_SET_DESYNC);
}

static inline void wl_subsurface_destroy(struct wl_subsurface* wl_subsurface) {
  wl_proxy_marshal((struct wl_proxy*)wl_subsurface, WL_SUBSURFACE_DESTROY);

  wl_proxy_destroy((struct wl_proxy*)wl_subsurface);
}
#endif

#ifndef WL_SURFACE_DAMAGE_BUFFER
#  define WL_SURFACE_DAMAGE_BUFFER 9

static inline void wl_surface_damage_buffer(struct wl_surface* wl_surface,
                                            int32_t x, int32_t y, int32_t width,
                                            int32_t height) {
  wl_proxy_marshal((struct wl_proxy*)wl_surface, WL_SURFACE_DAMAGE_BUFFER, x, y,
                   width, height);
}
#endif

#ifndef WL_POINTER_RELEASE_SINCE_VERSION
#  define WL_POINTER_RELEASE_SINCE_VERSION 3
#endif

#ifndef WL_POINTER_AXIS_ENUM
#  define WL_POINTER_AXIS_ENUM
/**
 * @ingroup iface_wl_pointer
 * axis types
 *
 * Describes the axis types of scroll events.
 */
enum wl_pointer_axis {
  /**
   * vertical axis
   */
  WL_POINTER_AXIS_VERTICAL_SCROLL = 0,
  /**
   * horizontal axis
   */
  WL_POINTER_AXIS_HORIZONTAL_SCROLL = 1,
};
#endif /* WL_POINTER_AXIS_ENUM */

#ifndef WL_POINTER_AXIS_SOURCE_ENUM
#  define WL_POINTER_AXIS_SOURCE_ENUM
/**
 * @ingroup iface_wl_pointer
 * axis source types
 *
 * Describes the source types for axis events. This indicates to the
 * client how an axis event was physically generated; a client may
 * adjust the user interface accordingly. For example, scroll events
 * from a "finger" source may be in a smooth coordinate space with
 * kinetic scrolling whereas a "wheel" source may be in discrete steps
 * of a number of lines.
 *
 * The "continuous" axis source is a device generating events in a
 * continuous coordinate space, but using something other than a
 * finger. One example for this source is button-based scrolling where
 * the vertical motion of a device is converted to scroll events while
 * a button is held down.
 *
 * The "wheel tilt" axis source indicates that the actual device is a
 * wheel but the scroll event is not caused by a rotation but a
 * (usually sideways) tilt of the wheel.
 */
enum wl_pointer_axis_source {
  /**
   * a physical wheel rotation
   */
  WL_POINTER_AXIS_SOURCE_WHEEL = 0,
  /**
   * finger on a touch surface
   */
  WL_POINTER_AXIS_SOURCE_FINGER = 1,
  /**
   * continuous coordinate space
   */
  WL_POINTER_AXIS_SOURCE_CONTINUOUS = 2,
  /**
   * a physical wheel tilt
   * @since 6
   */
  WL_POINTER_AXIS_SOURCE_WHEEL_TILT = 3,
};
/**
 * @ingroup iface_wl_pointer
 */
#  define WL_POINTER_AXIS_SOURCE_WHEEL_TILT_SINCE_VERSION 6
#endif /* WL_POINTER_AXIS_SOURCE_ENUM */

#ifndef WL_POINTER_AXIS_RELATIVE_DIRECTION_ENUM
#  define WL_POINTER_AXIS_RELATIVE_DIRECTION_ENUM
/**
 * @ingroup iface_wl_pointer
 * axis relative direction
 *
 * This specifies the direction of the physical motion that caused a
 * wl_pointer.axis event, relative to the wl_pointer.axis direction.
 */
enum wl_pointer_axis_relative_direction {
  /**
   * physical motion matches axis direction
   */
  WL_POINTER_AXIS_RELATIVE_DIRECTION_IDENTICAL = 0,
  /**
   * physical motion is the inverse of the axis direction
   */
  WL_POINTER_AXIS_RELATIVE_DIRECTION_INVERTED = 1,
};
#endif /* WL_POINTER_AXIS_RELATIVE_DIRECTION_ENUM */

/**
 * @ingroup iface_wl_pointer
 * @struct wl_pointer_listener
 */
struct moz_wl_pointer_listener {
  /**
   * enter event
   *
   * Notification that this seat's pointer is focused on a certain
   * surface.
   *
   * When a seat's focus enters a surface, the pointer image is
   * undefined and a client should respond to this event by setting
   * an appropriate pointer image with the set_cursor request.
   * @param serial serial number of the enter event
   * @param surface surface entered by the pointer
   * @param surface_x surface-local x coordinate
   * @param surface_y surface-local y coordinate
   */
  void (*enter)(void* data, struct wl_pointer* wl_pointer, uint32_t serial,
                struct wl_surface* surface, wl_fixed_t surface_x,
                wl_fixed_t surface_y);
  /**
   * leave event
   *
   * Notification that this seat's pointer is no longer focused on
   * a certain surface.
   *
   * The leave notification is sent before the enter notification for
   * the new focus.
   * @param serial serial number of the leave event
   * @param surface surface left by the pointer
   */
  void (*leave)(void* data, struct wl_pointer* wl_pointer, uint32_t serial,
                struct wl_surface* surface);
  /**
   * pointer motion event
   *
   * Notification of pointer location change. The arguments
   * surface_x and surface_y are the location relative to the focused
   * surface.
   * @param time timestamp with millisecond granularity
   * @param surface_x surface-local x coordinate
   * @param surface_y surface-local y coordinate
   */
  void (*motion)(void* data, struct wl_pointer* wl_pointer, uint32_t time,
                 wl_fixed_t surface_x, wl_fixed_t surface_y);
  /**
   * pointer button event
   *
   * Mouse button click and release notifications.
   *
   * The location of the click is given by the last motion or enter
   * event. The time argument is a timestamp with millisecond
   * granularity, with an undefined base.
   *
   * The button is a button code as defined in the Linux kernel's
   * linux/input-event-codes.h header file, e.g. BTN_LEFT.
   *
   * Any 16-bit button code value is reserved for future additions to
   * the kernel's event code list. All other button codes above
   * 0xFFFF are currently undefined but may be used in future
   * versions of this protocol.
   * @param serial serial number of the button event
   * @param time timestamp with millisecond granularity
   * @param button button that produced the event
   * @param state physical state of the button
   */
  void (*button)(void* data, struct wl_pointer* wl_pointer, uint32_t serial,
                 uint32_t time, uint32_t button, uint32_t state);
  /**
   * axis event
   *
   * Scroll and other axis notifications.
   *
   * For scroll events (vertical and horizontal scroll axes), the
   * value parameter is the length of a vector along the specified
   * axis in a coordinate space identical to those of motion events,
   * representing a relative movement along the specified axis.
   *
   * For devices that support movements non-parallel to axes multiple
   * axis events will be emitted.
   *
   * When applicable, for example for touch pads, the server can
   * choose to emit scroll events where the motion vector is
   * equivalent to a motion event vector.
   *
   * When applicable, a client can transform its content relative to
   * the scroll distance.
   * @param time timestamp with millisecond granularity
   * @param axis axis type
   * @param value length of vector in surface-local coordinate space
   */
  void (*axis)(void* data, struct wl_pointer* wl_pointer, uint32_t time,
               uint32_t axis, wl_fixed_t value);
  /**
   * end of a pointer event sequence
   *
   * Indicates the end of a set of events that logically belong
   * together. A client is expected to accumulate the data in all
   * events within the frame before proceeding.
   *
   * All wl_pointer events before a wl_pointer.frame event belong
   * logically together. For example, in a diagonal scroll motion the
   * compositor will send an optional wl_pointer.axis_source event,
   * two wl_pointer.axis events (horizontal and vertical) and finally
   * a wl_pointer.frame event. The client may use this information to
   * calculate a diagonal vector for scrolling.
   *
   * When multiple wl_pointer.axis events occur within the same
   * frame, the motion vector is the combined motion of all events.
   * When a wl_pointer.axis and a wl_pointer.axis_stop event occur
   * within the same frame, this indicates that axis movement in one
   * axis has stopped but continues in the other axis. When multiple
   * wl_pointer.axis_stop events occur within the same frame, this
   * indicates that these axes stopped in the same instance.
   *
   * A wl_pointer.frame event is sent for every logical event group,
   * even if the group only contains a single wl_pointer event.
   * Specifically, a client may get a sequence: motion, frame,
   * button, frame, axis, frame, axis_stop, frame.
   *
   * The wl_pointer.enter and wl_pointer.leave events are logical
   * events generated by the compositor and not the hardware. These
   * events are also grouped by a wl_pointer.frame. When a pointer
   * moves from one surface to another, a compositor should group the
   * wl_pointer.leave event within the same wl_pointer.frame.
   * However, a client must not rely on wl_pointer.leave and
   * wl_pointer.enter being in the same wl_pointer.frame.
   * Compositor-specific policies may require the wl_pointer.leave
   * and wl_pointer.enter event being split across multiple
   * wl_pointer.frame groups.
   * @since 5
   */
  void (*frame)(void* data, struct wl_pointer* wl_pointer);
  /**
   * axis source event
   *
   * Source information for scroll and other axes.
   *
   * This event does not occur on its own. It is sent before a
   * wl_pointer.frame event and carries the source information for
   * all events within that frame.
   *
   * The source specifies how this event was generated. If the source
   * is wl_pointer.axis_source.finger, a wl_pointer.axis_stop event
   * will be sent when the user lifts the finger off the device.
   *
   * If the source is wl_pointer.axis_source.wheel,
   * wl_pointer.axis_source.wheel_tilt or
   * wl_pointer.axis_source.continuous, a wl_pointer.axis_stop event
   * may or may not be sent. Whether a compositor sends an axis_stop
   * event for these sources is hardware-specific and
   * implementation-dependent; clients must not rely on receiving an
   * axis_stop event for these scroll sources and should treat scroll
   * sequences from these scroll sources as unterminated by default.
   *
   * This event is optional. If the source is unknown for a
   * particular axis event sequence, no event is sent. Only one
   * wl_pointer.axis_source event is permitted per frame.
   *
   * The order of wl_pointer.axis_discrete and wl_pointer.axis_source
   * is not guaranteed.
   * @param axis_source source of the axis event
   * @since 5
   */
  void (*axis_source)(void* data, struct wl_pointer* wl_pointer,
                      uint32_t axis_source);
  /**
   * axis stop event
   *
   * Stop notification for scroll and other axes.
   *
   * For some wl_pointer.axis_source types, a wl_pointer.axis_stop
   * event is sent to notify a client that the axis sequence has
   * terminated. This enables the client to implement kinetic
   * scrolling. See the wl_pointer.axis_source documentation for
   * information on when this event may be generated.
   *
   * Any wl_pointer.axis events with the same axis_source after this
   * event should be considered as the start of a new axis motion.
   *
   * The timestamp is to be interpreted identical to the timestamp in
   * the wl_pointer.axis event. The timestamp value may be the same
   * as a preceding wl_pointer.axis event.
   * @param time timestamp with millisecond granularity
   * @param axis the axis stopped with this event
   * @since 5
   */
  void (*axis_stop)(void* data, struct wl_pointer* wl_pointer, uint32_t time,
                    uint32_t axis);
  /**
   * axis click event
   *
   * Discrete step information for scroll and other axes.
   *
   * This event carries the axis value of the wl_pointer.axis event
   * in discrete steps (e.g. mouse wheel clicks).
   *
   * This event is deprecated with wl_pointer version 8 - this event
   * is not sent to clients supporting version 8 or later.
   *
   * This event does not occur on its own, it is coupled with a
   * wl_pointer.axis event that represents this axis value on a
   * continuous scale. The protocol guarantees that each
   * axis_discrete event is always followed by exactly one axis event
   * with the same axis number within the same wl_pointer.frame. Note
   * that the protocol allows for other events to occur between the
   * axis_discrete and its coupled axis event, including other
   * axis_discrete or axis events. A wl_pointer.frame must not
   * contain more than one axis_discrete event per axis type.
   *
   * This event is optional; continuous scrolling devices like
   * two-finger scrolling on touchpads do not have discrete steps and
   * do not generate this event.
   *
   * The discrete value carries the directional information. e.g. a
   * value of -2 is two steps towards the negative direction of this
   * axis.
   *
   * The axis number is identical to the axis number in the
   * associated axis event.
   *
   * The order of wl_pointer.axis_discrete and wl_pointer.axis_source
   * is not guaranteed.
   * @param axis axis type
   * @param discrete number of steps
   * @since 5
   * @deprecated Deprecated since version 8
   */
  void (*axis_discrete)(void* data, struct wl_pointer* wl_pointer,
                        uint32_t axis, int32_t discrete);
  /**
   * axis high-resolution scroll event
   *
   * Discrete high-resolution scroll information.
   *
   * This event carries high-resolution wheel scroll information,
   * with each multiple of 120 representing one logical scroll step
   * (a wheel detent). For example, an axis_value120 of 30 is one
   * quarter of a logical scroll step in the positive direction, a
   * value120 of -240 are two logical scroll steps in the negative
   * direction within the same hardware event. Clients that rely on
   * discrete scrolling should accumulate the value120 to multiples
   * of 120 before processing the event.
   *
   * The value120 must not be zero.
   *
   * This event replaces the wl_pointer.axis_discrete event in
   * clients supporting wl_pointer version 8 or later.
   *
   * Where a wl_pointer.axis_source event occurs in the same
   * wl_pointer.frame, the axis source applies to this event.
   *
   * The order of wl_pointer.axis_value120 and wl_pointer.axis_source
   * is not guaranteed.
   * @param axis axis type
   * @param value120 scroll distance as fraction of 120
   * @since 8
   */
  void (*axis_value120)(void* data, struct wl_pointer* wl_pointer,
                        uint32_t axis, int32_t value120);
  /**
   * axis relative physical direction event
   *
   * Relative directional information of the entity causing the
   * axis motion.
   *
   * For a wl_pointer.axis event, the
   * wl_pointer.axis_relative_direction event specifies the movement
   * direction of the entity causing the wl_pointer.axis event. For
   * example: - if a user's fingers on a touchpad move down and this
   * causes a wl_pointer.axis vertical_scroll down event, the
   * physical direction is 'identical' - if a user's fingers on a
   * touchpad move down and this causes a wl_pointer.axis
   * vertical_scroll up scroll up event ('natural scrolling'), the
   * physical direction is 'inverted'.
   *
   * A client may use this information to adjust scroll motion of
   * components. Specifically, enabling natural scrolling causes the
   * content to change direction compared to traditional scrolling.
   * Some widgets like volume control sliders should usually match
   * the physical direction regardless of whether natural scrolling
   * is active. This event enables clients to match the scroll
   * direction of a widget to the physical direction.
   *
   * This event does not occur on its own, it is coupled with a
   * wl_pointer.axis event that represents this axis value. The
   * protocol guarantees that each axis_relative_direction event is
   * always followed by exactly one axis event with the same axis
   * number within the same wl_pointer.frame. Note that the protocol
   * allows for other events to occur between the
   * axis_relative_direction and its coupled axis event.
   *
   * The axis number is identical to the axis number in the
   * associated axis event.
   *
   * The order of wl_pointer.axis_relative_direction,
   * wl_pointer.axis_discrete and wl_pointer.axis_source is not
   * guaranteed.
   * @param axis axis type
   * @param direction physical direction relative to axis motion
   * @since 9
   */
  void (*axis_relative_direction)(void* data, struct wl_pointer* wl_pointer,
                                  uint32_t axis, uint32_t direction);
};

#ifdef __cplusplus
}
#endif

#endif /* __MozWayland_h_ */
