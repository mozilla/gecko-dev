/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This is the telemetry module to report metrics for tools.
 *
 * Comprehensive documentation is in docs/frontend/telemetry.md
 */

"use strict";

const {
  getNthPathExcluding,
} = require("resource://devtools/shared/platform/stack.js");
const { TelemetryEnvironment } = ChromeUtils.importESModule(
  "resource://gre/modules/TelemetryEnvironment.sys.mjs"
);
const WeakMapMap = require("resource://devtools/client/shared/WeakMapMap.js");

// Object to be shared among all instances.
const PENDING_EVENT_PROPERTIES = new WeakMapMap();
const PENDING_EVENTS = new WeakMapMap();

/**
 * Instantiate a new Telemetry helper class.
 *
 * @param {Object} options [optional]
 * @param {Boolean} options.useSessionId [optional]
 *        If true, this instance will automatically generate a unique "sessionId"
 *        and use it to aggregate all records against this unique session.
 *        This helps aggregate all data coming from a single toolbox instance for ex.
 */
class Telemetry {
  constructor({ useSessionId = false } = {}) {
    // Note that native telemetry APIs expect a string
    this.sessionId = String(
      useSessionId ? parseInt(this.msSinceProcessStart(), 10) : -1
    );

    // Bind pretty much all functions so that callers do not need to.
    this.msSystemNow = this.msSystemNow.bind(this);
    this.getKeyedHistogramById = this.getKeyedHistogramById.bind(this);
    this.recordEvent = this.recordEvent.bind(this);
    this.preparePendingEvent = this.preparePendingEvent.bind(this);
    this.addEventProperty = this.addEventProperty.bind(this);
    this.addEventProperties = this.addEventProperties.bind(this);
    this.toolOpened = this.toolOpened.bind(this);
    this.toolClosed = this.toolClosed.bind(this);
  }

  get osNameAndVersion() {
    const osInfo = TelemetryEnvironment.currentEnvironment.system.os;

    if (!osInfo) {
      return "Unknown OS";
    }

    let osVersion = `${osInfo.name} ${osInfo.version}`;

    if (osInfo.windowsBuildNumber) {
      osVersion += `.${osInfo.windowsBuildNumber}`;
    }

    return osVersion;
  }

  /**
   * Time since the system wide epoch. This is not a monotonic timer but
   * can be used across process boundaries.
   */
  msSystemNow() {
    return Services.telemetry.msSystemNow();
  }

  /**
   * The number of milliseconds since process start using monotonic
   * timestamps (unaffected by system clock changes).
   */
  msSinceProcessStart() {
    return Services.telemetry.msSinceProcessStart();
  }

  /**
   * Get a keyed histogram.
   *
   * @param  {String} histogramId
   *         Histogram in which the data is to be stored.
   */
  getKeyedHistogramById(histogramId) {
    let histogram = null;

    if (histogramId) {
      try {
        histogram = Services.telemetry.getKeyedHistogramById(histogramId);
      } catch (e) {
        dump(
          `Warning: An attempt was made to write to the ${histogramId} ` +
            `histogram, which is not defined in Histograms.json\n` +
            `CALLER: ${getCaller()}`
        );
      }
    }
    return (
      histogram || {
        add: () => {},
      }
    );
  }

  /**
   * Telemetry events often need to make use of a number of properties from
   * completely different codepaths. To make this possible we create a
   * "pending event" along with an array of property names that we need to wait
   * for before sending the event.
   *
   * As each property is received via addEventProperty() we check if all
   * properties have been received. Once they have all been received we send the
   * telemetry event.
   *
   * @param {Object} obj
   *        The telemetry event or ping is associated with this object, meaning
   *        that multiple events or pings for the same histogram may be run
   *        concurrently, as long as they are associated with different objects.
   * @param {String} method
   *        The telemetry event method (describes the type of event that
   *        occurred e.g. "open")
   * @param {String} object
   *        The telemetry event object name (the name of the object the event
   *        occurred on) e.g. "tools" or "setting"
   * @param {String|null} value
   *        The telemetry event value (a user defined value, providing context
   *        for the event) e.g. "console"
   * @param {Array} expected
   *        An array of the properties needed before sending the telemetry
   *        event e.g.
   *        [
   *          "host",
   *          "width"
   *        ]
   */
  preparePendingEvent(obj, method, object, value, expected = []) {
    const sig = `${method},${object},${value}`;

    if (expected.length === 0) {
      throw new Error(
        `preparePendingEvent() was called without any expected ` +
          `properties.\n` +
          `CALLER: ${getCaller()}`
      );
    }

    const data = {
      extra: {},
      expected: new Set(expected),
    };

    PENDING_EVENTS.set(obj, sig, data);

    const props = PENDING_EVENT_PROPERTIES.get(obj, sig);
    if (props) {
      for (const [name, val] of Object.entries(props)) {
        this.addEventProperty(obj, method, object, value, name, val);
      }
      PENDING_EVENT_PROPERTIES.delete(obj, sig);
    }
  }

  /**
   * Adds an expected property for either a current or future pending event.
   * This means that if preparePendingEvent() is called before or after sending
   * the event properties they will automatically added to the event.
   *
   * @param {Object} obj
   *        The telemetry event or ping is associated with this object, meaning
   *        that multiple events or pings for the same histogram may be run
   *        concurrently, as long as they are associated with different objects.
   * @param {String} method
   *        The telemetry event method (describes the type of event that
   *        occurred e.g. "open")
   * @param {String} object
   *        The telemetry event object name (the name of the object the event
   *        occurred on) e.g. "tools" or "setting"
   * @param {String|null} value
   *        The telemetry event value (a user defined value, providing context
   *        for the event) e.g. "console"
   * @param {String} pendingPropName
   *        The pending property name
   * @param {String} pendingPropValue
   *        The pending property value
   */
  addEventProperty(
    obj,
    method,
    object,
    value,
    pendingPropName,
    pendingPropValue
  ) {
    const sig = `${method},${object},${value}`;
    const events = PENDING_EVENTS.get(obj, sig);

    // If the pending event has not been created add the property to the pending
    // list.
    if (!events) {
      const props = PENDING_EVENT_PROPERTIES.get(obj, sig);

      if (props) {
        props[pendingPropName] = pendingPropValue;
      } else {
        PENDING_EVENT_PROPERTIES.set(obj, sig, {
          [pendingPropName]: pendingPropValue,
        });
      }
      return;
    }

    const { expected, extra } = events;

    if (expected.has(pendingPropName)) {
      extra[pendingPropName] = pendingPropValue;

      if (expected.size === Object.keys(extra).length) {
        this._sendPendingEvent(obj, method, object, value);
      }
    } else {
      // The property was not expected, warn and bail.
      throw new Error(
        `An attempt was made to add the unexpected property ` +
          `"${pendingPropName}" to a telemetry event with the ` +
          `signature "${sig}"\n` +
          `CALLER: ${getCaller()}`
      );
    }
  }

  /**
   * Adds expected properties for either a current or future pending event.
   * This means that if preparePendingEvent() is called before or after sending
   * the event properties they will automatically added to the event.
   *
   * @param {Object} obj
   *        The telemetry event or ping is associated with this object, meaning
   *        that multiple events or pings for the same histogram may be run
   *        concurrently, as long as they are associated with different objects.
   * @param {String} method
   *        The telemetry event method (describes the type of event that
   *        occurred e.g. "open")
   * @param {String} object
   *        The telemetry event object name (the name of the object the event
   *        occurred on) e.g. "tools" or "setting"
   * @param {String|null} value
   *        The telemetry event value (a user defined value, providing context
   *        for the event) e.g. "console"
   * @param {String} pendingObject
   *        An object containing key, value pairs that should be added to the
   *        event as properties.
   */
  addEventProperties(obj, method, object, value, pendingObject) {
    for (const [key, val] of Object.entries(pendingObject)) {
      this.addEventProperty(obj, method, object, value, key, val);
    }
  }

  /**
   * A private method that is not to be used externally. This method is used to
   * prepare a pending telemetry event for sending and then send it via
   * recordEvent().
   *
   * @param {Object} obj
   *        The telemetry event or ping is associated with this object, meaning
   *        that multiple events or pings for the same histogram may be run
   *        concurrently, as long as they are associated with different objects.
   * @param {String} method
   *        The telemetry event method (describes the type of event that
   *        occurred e.g. "open")
   * @param {String} object
   *        The telemetry event object name (the name of the object the event
   *        occurred on) e.g. "tools" or "setting"
   * @param {String|null} value
   *        The telemetry event value (a user defined value, providing context
   *        for the event) e.g. "console"
   */
  _sendPendingEvent(obj, method, object, value) {
    const sig = `${method},${object},${value}`;
    const { extra } = PENDING_EVENTS.get(obj, sig);

    PENDING_EVENTS.delete(obj, sig);
    PENDING_EVENT_PROPERTIES.delete(obj, sig);
    this.recordEvent(method, object, value, extra);
  }

  /**
   * Send a telemetry event.
   *
   * @param {String} method
   *        The telemetry event method (describes the type of event that
   *        occurred e.g. "open")
   * @param {String} object
   *        The telemetry event object name (the name of the object the event
   *        occurred on) e.g. "tools" or "setting"
   * @param {String|null} [value]
   *        Optional telemetry event value (a user defined value, providing
   *        context for the event) e.g. "console"
   * @param {Object} [extra]
   *        Optional telemetry event extra object containing the properties that
   *        will be sent with the event e.g.
   *        {
   *          host: "bottom",
   *          width: "1024"
   *        }
   */
  recordEvent(method, object, value = null, extra = null) {
    // Only string values are allowed so cast all values to strings.
    if (extra) {
      for (let [name, val] of Object.entries(extra)) {
        val = val + "";

        if (val.length > 80) {
          const sig = `${method},${object},${value}`;

          dump(
            `Warning: The property "${name}" was added to a telemetry ` +
              `event with the signature ${sig} but it's value "${val}" is ` +
              `longer than the maximum allowed length of 80 characters.\n` +
              `The property value has been trimmed to 80 characters before ` +
              `sending.\nCALLER: ${getCaller()}`
          );

          val = val.substring(0, 80);
        }

        extra[name] = val;
      }
    }
    // Automatically flag the record with the session ID
    // if the current Telemetry instance relates to a toolbox
    // so that data can be aggregated per toolbox instance.
    // Note that we also aggregate data per about:debugging instance.
    if (!extra) {
      extra = {};
    }
    extra.session_id = this.sessionId;
    if (value !== null) {
      extra.value = value;
    }

    // Using the Glean API directly insteade of doing string manipulations
    // would be better. See bug 1921793.
    const eventName = `${method}_${object}`.replace(/(_[a-z])/g, c =>
      c[1].toUpperCase()
    );
    Glean.devtoolsMain[eventName]?.record(extra);
  }

  /**
   * Sends telemetry pings to indicate that a tool has been opened.
   *
   * @param {String} id
   *        The ID of the tool opened.
   * @param {Object} obj
   *        The telemetry event or ping is associated with this object, meaning
   *        that multiple events or pings for the same histogram may be run
   *        concurrently, as long as they are associated with different objects.
   *
   * NOTE: This method is designed for tools that send multiple probes on open,
   *       one of those probes being a counter and the other a timer. If you
   *       only have one probe you should be using another method.
   */
  toolOpened(id, obj) {
    const charts = getChartsFromToolId(id);

    if (!charts) {
      return;
    }

    if (charts.useTimedEvent) {
      this.preparePendingEvent(obj, "tool_timer", id, null, [
        "os",
        "time_open",
      ]);
      this.addEventProperty(
        obj,
        "tool_timer",
        id,
        null,
        "time_open",
        this.msSystemNow()
      );
    }
    if (charts.gleanTimingDist) {
      if (!obj._timerIDs) {
        obj._timerIDs = new Map();
      }
      if (!obj._timerIDs.has(id)) {
        obj._timerIDs.set(id, charts.gleanTimingDist.start());
      }
    }
    if (charts.gleanCounter) {
      charts.gleanCounter.add(1);
    }
  }

  /**
   * Sends telemetry pings to indicate that a tool has been closed.
   *
   * @param {String} id
   *        The ID of the tool opened.
   * @param {Object} obj
   *        The telemetry event or ping is associated with this object, meaning
   *        that multiple events or pings for the same histogram may be run
   *        concurrently, as long as they are associated with different objects.
   *
   * NOTE: This method is designed for tools that send multiple probes on open,
   *       one of those probes being a counter and the other a timer. If you
   *       only have one probe you should be using another method.
   */
  toolClosed(id, obj) {
    const charts = getChartsFromToolId(id);

    if (!charts) {
      return;
    }

    if (charts.useTimedEvent) {
      const sig = `tool_timer,${id},null`;
      const event = PENDING_EVENTS.get(obj, sig);
      const time = this.msSystemNow() - event.extra.time_open;

      this.addEventProperties(obj, "tool_timer", id, null, {
        time_open: time,
        os: this.osNameAndVersion,
      });
    }

    if (charts.gleanTimingDist && obj._timerIDs) {
      const timerID = obj._timerIDs.get(id);
      if (timerID) {
        charts.gleanTimingDist.stopAndAccumulate(timerID);
        obj._timerIDs.delete(id);
      }
    }
  }
}

/**
 * Returns the telemetry charts for a specific tool.
 *
 * @param {String} id
 *        The ID of the tool that has been opened.
 *
 */
// eslint-disable-next-line complexity
function getChartsFromToolId(id) {
  if (!id) {
    return null;
  }

  let useTimedEvent = null;
  let gleanCounter = null;
  let gleanTimingDist = null;

  if (id === "performance") {
    id = "jsprofiler";
  }

  switch (id) {
    case "aboutdebugging":
    case "browserconsole":
    case "dom":
    case "inspector":
    case "jsbrowserdebugger":
    case "jsdebugger":
    case "jsprofiler":
    case "memory":
    case "netmonitor":
    case "options":
    case "responsive":
    case "storage":
    case "styleeditor":
    case "toolbox":
    case "webconsole":
      gleanTimingDist = Glean.devtools[`${id}TimeActive`];
      gleanCounter = Glean.devtools[`${id}OpenedCount`];
      break;
    case "accessibility":
      gleanTimingDist = Glean.devtools.accessibilityTimeActive;
      gleanCounter = Glean.devtoolsAccessibility.openedCount;
      break;
    case "accessibility_picker":
      gleanTimingDist = Glean.devtools.accessibilityPickerTimeActive;
      gleanCounter = Glean.devtoolsAccessibility.pickerUsedCount;
      break;
    case "changesview":
      gleanTimingDist = Glean.devtools.changesviewTimeActive;
      gleanCounter = Glean.devtoolsChangesview.openedCount;
      break;
    case "animationinspector":
    case "compatibilityview":
    case "computedview":
    case "fontinspector":
    case "layoutview":
    case "ruleview":
      useTimedEvent = true;
      gleanTimingDist = Glean.devtools[`${id}TimeActive`];
      gleanCounter = Glean.devtools[`${id}OpenedCount`];
      break;
    case "flexbox_highlighter":
      gleanTimingDist = Glean.devtools.flexboxHighlighterTimeActive;
      break;
    case "grid_highlighter":
      gleanTimingDist = Glean.devtools.gridHighlighterTimeActive;
      break;
    default:
      gleanTimingDist = Glean.devtools.customTimeActive;
      gleanCounter = Glean.devtools.customOpenedCount;
  }

  return {
    useTimedEvent,
    gleanCounter,
    gleanTimingDist,
  };
}

/**
 * Displays the first caller and calling line outside of this file in the
 * event of an error. This is the line that made the call that produced the
 * error.
 */
function getCaller() {
  return getNthPathExcluding(0, "/telemetry.js");
}

module.exports = Telemetry;
