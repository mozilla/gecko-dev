/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

enum DataCallType {
  "mms",
  "supl",
  "ims",
  "dun",
  "fota"
};

enum DataCallState {
  "unknown",
  "connecting",
  "connected",
  "disconnecting",
  "disconnected",
  "unavailable"
};

[NavigatorProperty="dataCallManager",
 Pref="dom.datacall.enabled",
 CheckPermissions="datacall",
 AvailableIn="CertifiedApps",
 JSImplementation="@mozilla.org/datacallmanager;1"]
interface DataCallManager {
  /**
   * Request data call for a certain data call type.
   *
   * @param type
   *        The desired data call type, one of the DataCallType values.
   * @param serviceId [optional]
   *        Default value is the user setting service id for data call.
   *
   * @return If success, promise is resolved with the new created DataCall
             object. Otherwise, rejected with an error message.
   */
  Promise<DataCall> requestDataCall(DataCallType type,
                                    optional unsigned long serviceId);

  /**
   * Request the current state of a certain data call type.
   *
   * @param type
   *        The desired data call type, one of the DataCallType values.
   * @param serviceId [optional]
   *        Default value is the user setting service id for data call.
   *
   * @return If success, promise is resolved with a DataCallState. Otherwise,
   *         rejected with an error message.
   */
  Promise<DataCallState> getDataCallState(DataCallType type,
                                          optional unsigned long serviceId);

};

[Pref="dom.datacall.enabled",
 CheckPermissions="datacall",
 AvailableIn="CertifiedApps",
 JSImplementation="@mozilla.org/datacall;1"]
interface DataCall : EventTarget {
  /**
   * Current data call state.
   *
   * Note: if state becomes 'disconnected', state may (or may not) become
   *       'connected' later. Consumer can choose to wait or release, however
   *       all host routes must be re-added when state becomes 'connected'.
   *       If state becomes 'unavailable', all functions in this DataCall
   *       becomes invalid as well. Please release and request it again if
   *       needed.
   */
  readonly attribute DataCallState state;

  // Data call type.
  readonly attribute DataCallType type;

  // Network interface name.
  readonly attribute DOMString name;

  // List of ip addresses with prefix length.
  [Cached, Pure]
  readonly attribute sequence<DOMString> addresses;

  // List of gateway addresses.
  [Cached, Pure]
  readonly attribute sequence<DOMString> gateways;

  // List of dns addresses.
  [Cached, Pure]
  readonly attribute sequence<DOMString> dnses;

  /**
   * Add a host route to ensure traffic to the host is delivered via this data
   * call.
   *
   * @param host
   *        The host to add route for.
   *
   * @return If success, promise is resolved. Otherwise, rejected with an error
   *         message.
   */
  Promise<void> addHostRoute(DOMString host);

  /**
   * Remove host route to stop routing traffic to a host via this data call.
   *
   * @param host
   *        The host to remove route for.
   *
   * @return If success, promise is resolved. Otherwise, rejected with an error
   *         message.
   */
  Promise<void> removeHostRoute(DOMString host);

  /**
   * Release this data call. Once the DataCall is released, state becomes
   * 'unavailable' and all functions of the DataCall becomes invalid as well.
   *
   * @return If success, promise is resolved. Otherwise, rejected with an error
   *         message.
   */
  Promise<void> releaseDataCall();

  // Fired when attribute state changed.
  attribute EventHandler onstatechanged;
};
