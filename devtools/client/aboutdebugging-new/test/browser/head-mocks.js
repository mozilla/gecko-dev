/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const MOCKS_ROOT = CHROME_URL_ROOT + "mocks/";

const { RUNTIMES } = require("devtools/client/aboutdebugging-new/src/constants");

/* import-globals-from mocks/head-client-wrapper-mock.js */
Services.scriptloader.loadSubScript(MOCKS_ROOT + "head-client-wrapper-mock.js", this);
/* import-globals-from mocks/head-runtime-client-factory-mock.js */
Services.scriptloader.loadSubScript(MOCKS_ROOT + "head-runtime-client-factory-mock.js",
  this);
/* import-globals-from mocks/head-usb-runtimes-mock.js */
Services.scriptloader.loadSubScript(MOCKS_ROOT + "head-usb-runtimes-mock.js", this);

/**
 * This wrapper around the mocks used in about:debugging tests provides helpers to
 * quickly setup mocks for runtime tests involving USB, network or wifi runtimes that can
 * are difficult to setup in a test environment.
 */
class Mocks {
  constructor() {
    // Setup the usb-runtimes mock to rely on the internal _usbRuntimes array.
    this.usbRuntimesMock = createUsbRuntimesMock();
    this._usbRuntimes = [];
    this.usbRuntimesMock.getUSBRuntimes = () => {
      return this._usbRuntimes;
    };

    // refreshUSBRuntimes normally starts scan, which should ultimately fire the
    // "runtime-list-updated" event.
    this.usbRuntimesMock.refreshUSBRuntimes = () => {
      this.emitUSBUpdate();
    };

    // Prepare a fake observer to be able to emit events from this mock.
    this._observerMock = addObserverMock(this.usbRuntimesMock);

    // Setup the runtime-client-factory mock to rely on the internal _clients map.
    this.runtimeClientFactoryMock = createRuntimeClientFactoryMock();
    this._clients = {
      [RUNTIMES.NETWORK]: {},
      [RUNTIMES.THIS_FIREFOX]: {},
      [RUNTIMES.USB]: {},
    };
    this.runtimeClientFactoryMock.createClientForRuntime = runtime => {
      return this._clients[runtime.type][runtime.id];
    };

    // Add a client for THIS_FIREFOX, since about:debugging will start on the This Firefox
    // page.
    this._thisFirefoxClient = createThisFirefoxClientMock();
    this._clients[RUNTIMES.THIS_FIREFOX][RUNTIMES.THIS_FIREFOX] = this._thisFirefoxClient;

    // Enable mocks and remove them after the test.
    this.enableMocks();
    registerCleanupFunction(() => this.disableMocks());
  }

  get thisFirefoxClient() {
    return this._thisFirefoxClient;
  }

  enableMocks() {
    enableUsbRuntimesMock(this.usbRuntimesMock);
    enableRuntimeClientFactoryMock(this.runtimeClientFactoryMock);
  }

  disableMocks() {
    disableUsbRuntimesMock();
    disableRuntimeClientFactoryMock();

    for (const host of Object.keys(this._clients[RUNTIMES.NETWORK])) {
      this.removeNetworkRuntime(host);
    }
  }

  createNetworkRuntime(host, runtimeInfo) {
    const { addNetworkLocation } =
      require("devtools/client/aboutdebugging-new/src/modules/network-locations");
    addNetworkLocation(host);

    // Add a valid client that can be returned for this particular runtime id.
    const mockNetworkClient = createClientMock();
    mockNetworkClient.getDeviceDescription = () => {
      return {
        name: runtimeInfo.name || "TestBrand",
        channel: runtimeInfo.channel || "release",
        version: runtimeInfo.version || "1.0",
      };
    };
    this._clients[RUNTIMES.NETWORK][host] = mockNetworkClient;

    return mockNetworkClient;
  }

  removeNetworkRuntime(host) {
    const { removeNetworkLocation } =
      require("devtools/client/aboutdebugging-new/src/modules/network-locations");
    removeNetworkLocation(host);

    delete this._clients[RUNTIMES.NETWORK][host];
  }

  emitUSBUpdate() {
    this._observerMock.emit("runtime-list-updated");
  }

  /**
   * Creates a USB runtime for which a client conenction can be established.
   * @param {String} id
   *        The id of the runtime.
   * @param {Object} optional object used to create the fake runtime & device
   *        - deviceName: {String} Device name
   *        - shortName: {String} Short name for the device
   *        - name: {String} Application name, for instance "Firefox"
   *        - channel: {String} Release channel, for instance "release", "nightly"
   *        - version: {String} Version, for instance "63.0a"
   *        - socketPath: {String} (should only be used for connecting, so not here)
   * @return {Object} Returns the mock client created for this runtime so that methods
   * can be overridden on it.
   */
  createUSBRuntime(id, runtimeInfo = {}) {
    // Add a new runtime to the list of scanned runtimes.
    this._usbRuntimes.push({
      id: id,
      _socketPath: runtimeInfo.socketPath || "test/path",
      deviceName: runtimeInfo.deviceName || "test device name",
      shortName: runtimeInfo.shortName || "testshort",
    });

    // Add a valid client that can be returned for this particular runtime id.
    const mockUsbClient = createClientMock();
    mockUsbClient.getDeviceDescription = () => {
      return {
        name: runtimeInfo.name || "TestBrand",
        channel: runtimeInfo.channel || "release",
        version: runtimeInfo.version || "1.0",
      };
    };
    this._clients[RUNTIMES.USB][id] = mockUsbClient;

    return mockUsbClient;
  }

  removeUSBRuntime(id) {
    this._usbRuntimes = this._usbRuntimes.filter(runtime => runtime.id !== id);
    delete this._clients[RUNTIMES.USB][id];
  }
}
