/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const MOCKS_ROOT = CHROME_URL_ROOT + "mocks/";

const { RUNTIMES } = require("devtools/client/aboutdebugging-new/src/constants");

/* import-globals-from head-client-wrapper-mock.js */
Services.scriptloader.loadSubScript(MOCKS_ROOT + "head-client-wrapper-mock.js", this);
/* import-globals-from head-runtime-client-factory-mock.js */
Services.scriptloader.loadSubScript(MOCKS_ROOT + "head-runtime-client-factory-mock.js",
  this);
/* import-globals-from head-usb-runtimes-mock.js */
Services.scriptloader.loadSubScript(MOCKS_ROOT + "head-usb-runtimes-mock.js", this);

/**
 * This wrapper around the USB mocks used in about:debugging tests provides helpers to
 * quickly setup mocks for typical USB runtime tests.
 */
class UsbMocks {
  constructor() {
    // Setup the usb-runtimes mock to rely on the internal _runtimes array.
    this.usbRuntimesMock = createUsbRuntimesMock();
    this._runtimes = [];
    this.usbRuntimesMock.getUSBRuntimes = () => {
      return this._runtimes;
    };

    // Prepare a fake observer to be able to emit events from this mock.
    this._observerMock = addObserverMock(this.usbRuntimesMock);

    // Setup the runtime-client-factory mock to rely on the internal _clients map.
    this.runtimeClientFactoryMock = createRuntimeClientFactoryMock();
    this._clients = {};
    this.runtimeClientFactoryMock.createClientForRuntime = runtime => {
      return { client: this._clients[runtime.id] };
    };

    // Add a client for THIS_FIREFOX, since about:debugging will start on the This Firefox
    // page.
    this._thisFirefoxClient = createThisFirefoxClientMock();
    this._clients[RUNTIMES.THIS_FIREFOX] = this._thisFirefoxClient;
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
  }

  emitUpdate() {
    this._observerMock.emit("runtime-list-updated");
  }

  /**
   * Creates a USB runtime for which a client conenction can be established.
   * @param {String} id
   *        The id of the runtime.
   * @param {Object} optional object used to create the fake runtime & device
   *        - deviceName: {String} Device name
   *        - shortName: {String} Short name for the device
   *        - appName: {String} Application name, for instance "Firefox"
   *        - channel: {String} Release channel, for instance "release", "nightly"
   *        - version: {String} Version, for instance "63.0a"
   *        - socketPath: {String} (should only be used for connecting, so not here)
   * @return {Object} Returns the mock client created for this runtime so that methods
   * can be overridden on it.
   */
  createRuntime(id, runtimeInfo = {}) {
    // Add a new runtime to the list of scanned runtimes.
    this._runtimes.push({
      id: id,
      _socketPath: runtimeInfo.socketPath || "test/path",
      deviceName: runtimeInfo.deviceName || "test device name",
      shortName: runtimeInfo.shortName || "testshort",
    });

    // Add a valid client that can be returned for this particular runtime id.
    const mockUsbClient = createClientMock();
    mockUsbClient.getDeviceDescription = () => {
      return {
        brandName: runtimeInfo.appName || "TestBrand",
        channel: runtimeInfo.channel || "release",
        version: runtimeInfo.version || "1.0",
      };
    };
    this._clients[id] = mockUsbClient;

    return mockUsbClient;
  }

  removeRuntime(id) {
    this._runtimes = this._runtimes.filter(runtime => runtime.id !== id);
    delete this._clients[id];
  }
}
