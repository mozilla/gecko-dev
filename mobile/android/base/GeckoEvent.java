/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.nio.ByteBuffer;
import java.util.concurrent.ArrayBlockingQueue;

import org.mozilla.gecko.AppConstants.Versions;
import org.mozilla.gecko.gfx.DisplayPortMetrics;
import org.mozilla.gecko.gfx.ImmutableViewportMetrics;

import android.graphics.Point;
import android.graphics.PointF;
import android.graphics.Rect;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorManager;
import android.location.Address;
import android.location.Location;
import android.os.SystemClock;
import android.util.Log;
import android.util.SparseArray;
import android.view.KeyEvent;
import android.view.MotionEvent;
import org.mozilla.gecko.mozglue.JNITarget;
import org.mozilla.gecko.mozglue.RobocopTarget;

/**
 * We're not allowed to hold on to most events given to us
 * so we save the parts of the events we want to use in GeckoEvent.
 * Fields have different meanings depending on the event type.
 */
@JNITarget
public class GeckoEvent {
    private static final String LOGTAG = "GeckoEvent";

    private static final int EVENT_FACTORY_SIZE = 5;

    // Maybe we're probably better to just make mType non final, and just store GeckoEvents in here...
    private static final SparseArray<ArrayBlockingQueue<GeckoEvent>> mEvents = new SparseArray<ArrayBlockingQueue<GeckoEvent>>();

    public static GeckoEvent get(NativeGeckoEvent type) {
        synchronized (mEvents) {
            ArrayBlockingQueue<GeckoEvent> events = mEvents.get(type.value);
            if (events != null && events.size() > 0) {
                return events.poll();
            }
        }

        return new GeckoEvent(type);
    }

    public void recycle() {
        synchronized (mEvents) {
            ArrayBlockingQueue<GeckoEvent> events = mEvents.get(mType);
            if (events == null) {
                events = new ArrayBlockingQueue<GeckoEvent>(EVENT_FACTORY_SIZE);
                mEvents.put(mType, events);
            }

            events.offer(this);
        }
    }

    // Make sure to keep these values in sync with the enum in
    // AndroidGeckoEvent in widget/android/AndroidJavaWrappers.h
    @JNITarget
    private enum NativeGeckoEvent {
        NATIVE_POKE(0),
        KEY_EVENT(1),
        MOTION_EVENT(2),
        SENSOR_EVENT(3),
        PROCESS_OBJECT(4),
        LOCATION_EVENT(5),
        IME_EVENT(6),
        SIZE_CHANGED(8),
        APP_BACKGROUNDING(9),
        APP_FOREGROUNDING(10),
        LOAD_URI(12),
        NOOP(15),
        BROADCAST(19),
        VIEWPORT(20),
        VISITED(21),
        NETWORK_CHANGED(22),
        THUMBNAIL(25),
        SCREENORIENTATION_CHANGED(27),
        COMPOSITOR_CREATE(28),
        COMPOSITOR_PAUSE(29),
        COMPOSITOR_RESUME(30),
        NATIVE_GESTURE_EVENT(31),
        IME_KEY_EVENT(32),
        CALL_OBSERVER(33),
        REMOVE_OBSERVER(34),
        LOW_MEMORY(35),
        NETWORK_LINK_CHANGE(36),
        TELEMETRY_HISTOGRAM_ADD(37),
        PREFERENCES_OBSERVE(39),
        PREFERENCES_GET(40),
        PREFERENCES_REMOVE_OBSERVERS(41),
        TELEMETRY_UI_SESSION_START(42),
        TELEMETRY_UI_SESSION_STOP(43),
        TELEMETRY_UI_EVENT(44),
        GAMEPAD_ADDREMOVE(45),
        GAMEPAD_DATA(46),
        LONG_PRESS(47),
        ZOOMEDVIEW(48);

        public final int value;

        private NativeGeckoEvent(int value) {
            this.value = value;
        }
    }

    // Encapsulation of common IME actions.
    @JNITarget
    public enum ImeAction {
        IME_SYNCHRONIZE(0),
        IME_REPLACE_TEXT(1),
        IME_SET_SELECTION(2),
        IME_ADD_COMPOSITION_RANGE(3),
        IME_UPDATE_COMPOSITION(4),
        IME_REMOVE_COMPOSITION(5),
        IME_ACKNOWLEDGE_FOCUS(6),
        IME_COMPOSE_TEXT(7);

        public final int value;

        private ImeAction(int value) {
            this.value = value;
        }
    }

    public static final int IME_RANGE_CARETPOSITION = 1;
    public static final int IME_RANGE_RAWINPUT = 2;
    public static final int IME_RANGE_SELECTEDRAWTEXT = 3;
    public static final int IME_RANGE_CONVERTEDTEXT = 4;
    public static final int IME_RANGE_SELECTEDCONVERTEDTEXT = 5;

    public static final int IME_RANGE_LINE_NONE = 0;
    public static final int IME_RANGE_LINE_DOTTED = 1;
    public static final int IME_RANGE_LINE_DASHED = 2;
    public static final int IME_RANGE_LINE_SOLID = 3;
    public static final int IME_RANGE_LINE_DOUBLE = 4;
    public static final int IME_RANGE_LINE_WAVY = 5;

    public static final int IME_RANGE_UNDERLINE = 1;
    public static final int IME_RANGE_FORECOLOR = 2;
    public static final int IME_RANGE_BACKCOLOR = 4;
    public static final int IME_RANGE_LINECOLOR = 8;

    public static final int ACTION_MAGNIFY_START = 11;
    public static final int ACTION_MAGNIFY = 12;
    public static final int ACTION_MAGNIFY_END = 13;

    public static final int ACTION_GAMEPAD_ADDED = 1;
    public static final int ACTION_GAMEPAD_REMOVED = 2;

    public static final int ACTION_GAMEPAD_BUTTON = 1;
    public static final int ACTION_GAMEPAD_AXES = 2;

    public static final int ACTION_OBJECT_LAYER_CLIENT = 1;

    private final int mType;
    private int mAction;
    private boolean mAckNeeded;
    private long mTime;
    private Point[] mPoints;
    private int[] mPointIndicies;
    private int mPointerIndex; // index of the point that has changed
    private float[] mOrientations;
    private float[] mPressures;
    private int[] mToolTypes;
    private Point[] mPointRadii;
    private Rect mRect;
    private double mX;
    private double mY;
    private double mZ;
    private double mW;

    private int mMetaState;
    private int mFlags;
    private int mKeyCode;
    private int mScanCode;
    private int mUnicodeChar;
    private int mBaseUnicodeChar; // mUnicodeChar without meta states applied
    private int mDOMPrintableKeyValue;
    private int mRepeatCount;
    private int mCount;
    private int mStart;
    private int mEnd;
    private String mCharacters;
    private String mCharactersExtra;
    private String mData;
    private int mRangeType;
    private int mRangeStyles;
    private int mRangeLineStyle;
    private boolean mRangeBoldLine;
    private int mRangeForeColor;
    private int mRangeBackColor;
    private int mRangeLineColor;
    private Location mLocation;
    private Address mAddress;

    private int     mConnectionType;
    private boolean mIsWifi;
    private int     mDHCPGateway;

    private int mNativeWindow;

    private short mScreenOrientation;

    private ByteBuffer mBuffer;

    private int mWidth;
    private int mHeight;

    private int mID;
    private int mGamepadButton;
    private boolean mGamepadButtonPressed;
    private float mGamepadButtonValue;
    private float[] mGamepadValues;

    private String[] mPrefNames;

    private Object mObject;

    private GeckoEvent(NativeGeckoEvent event) {
        mType = event.value;
    }

    public static GeckoEvent createAppBackgroundingEvent() {
        return GeckoEvent.get(NativeGeckoEvent.APP_BACKGROUNDING);
    }

    public static GeckoEvent createAppForegroundingEvent() {
        return GeckoEvent.get(NativeGeckoEvent.APP_FOREGROUNDING);
    }

    public static GeckoEvent createNoOpEvent() {
        return GeckoEvent.get(NativeGeckoEvent.NOOP);
    }

    public static GeckoEvent createKeyEvent(KeyEvent k, int action, int metaState) {
        GeckoEvent event = GeckoEvent.get(NativeGeckoEvent.KEY_EVENT);
        event.initKeyEvent(k, action, metaState);
        return event;
    }

    public static GeckoEvent createCompositorCreateEvent(int width, int height) {
        GeckoEvent event = GeckoEvent.get(NativeGeckoEvent.COMPOSITOR_CREATE);
        event.mWidth = width;
        event.mHeight = height;
        return event;
    }

    public static GeckoEvent createCompositorPauseEvent() {
        return GeckoEvent.get(NativeGeckoEvent.COMPOSITOR_PAUSE);
    }

    public static GeckoEvent createCompositorResumeEvent() {
        return GeckoEvent.get(NativeGeckoEvent.COMPOSITOR_RESUME);
    }

    private void initKeyEvent(KeyEvent k, int action, int metaState) {
        // Use a separate action argument so we can override the key's original action,
        // e.g. change ACTION_MULTIPLE to ACTION_DOWN. That way we don't have to allocate
        // a new key event just to change its action field.
        mAction = action;
        mTime = k.getEventTime();
        // Normally we expect k.getMetaState() to reflect the current meta-state; however,
        // some software-generated key events may not have k.getMetaState() set, e.g. key
        // events from Swype. Therefore, it's necessary to combine the key's meta-states
        // with the meta-states that we keep separately in KeyListener
        mMetaState = k.getMetaState() | metaState;
        mFlags = k.getFlags();
        mKeyCode = k.getKeyCode();
        mScanCode = k.getScanCode();
        mUnicodeChar = k.getUnicodeChar(mMetaState);
        // e.g. for Ctrl+A, Android returns 0 for mUnicodeChar,
        // but Gecko expects 'a', so we return that in mBaseUnicodeChar
        mBaseUnicodeChar = k.getUnicodeChar(0);
        mRepeatCount = k.getRepeatCount();
        mCharacters = k.getCharacters();
        if (mUnicodeChar >= ' ') {
            mDOMPrintableKeyValue = mUnicodeChar;
        } else {
            int unmodifiedMetaState =
                mMetaState & ~(KeyEvent.META_ALT_MASK |
                               KeyEvent.META_CTRL_MASK |
                               KeyEvent.META_META_MASK);
            if (unmodifiedMetaState != mMetaState) {
                mDOMPrintableKeyValue = k.getUnicodeChar(unmodifiedMetaState);
            }
        }
    }

    /**
     * This method is a replacement for the the KeyEvent.isGamepadButton method to be
     * compatible with Build.VERSION.SDK_INT < 12. This is an implementation of the
     * same method isGamepadButton available after SDK 12.
     * @param keyCode int with the key code (Android key constant from KeyEvent).
     * @return True if the keycode is a gamepad button, such as {@link #KEYCODE_BUTTON_A}.
     */
    private static boolean isGamepadButton(int keyCode) {
        switch (keyCode) {
            case KeyEvent.KEYCODE_BUTTON_A:
            case KeyEvent.KEYCODE_BUTTON_B:
            case KeyEvent.KEYCODE_BUTTON_C:
            case KeyEvent.KEYCODE_BUTTON_X:
            case KeyEvent.KEYCODE_BUTTON_Y:
            case KeyEvent.KEYCODE_BUTTON_Z:
            case KeyEvent.KEYCODE_BUTTON_L1:
            case KeyEvent.KEYCODE_BUTTON_R1:
            case KeyEvent.KEYCODE_BUTTON_L2:
            case KeyEvent.KEYCODE_BUTTON_R2:
            case KeyEvent.KEYCODE_BUTTON_THUMBL:
            case KeyEvent.KEYCODE_BUTTON_THUMBR:
            case KeyEvent.KEYCODE_BUTTON_START:
            case KeyEvent.KEYCODE_BUTTON_SELECT:
            case KeyEvent.KEYCODE_BUTTON_MODE:
            case KeyEvent.KEYCODE_BUTTON_1:
            case KeyEvent.KEYCODE_BUTTON_2:
            case KeyEvent.KEYCODE_BUTTON_3:
            case KeyEvent.KEYCODE_BUTTON_4:
            case KeyEvent.KEYCODE_BUTTON_5:
            case KeyEvent.KEYCODE_BUTTON_6:
            case KeyEvent.KEYCODE_BUTTON_7:
            case KeyEvent.KEYCODE_BUTTON_8:
            case KeyEvent.KEYCODE_BUTTON_9:
            case KeyEvent.KEYCODE_BUTTON_10:
            case KeyEvent.KEYCODE_BUTTON_11:
            case KeyEvent.KEYCODE_BUTTON_12:
            case KeyEvent.KEYCODE_BUTTON_13:
            case KeyEvent.KEYCODE_BUTTON_14:
            case KeyEvent.KEYCODE_BUTTON_15:
            case KeyEvent.KEYCODE_BUTTON_16:
                return true;
            default:
                return false;
        }
    }

    public static GeckoEvent createNativeGestureEvent(int action, PointF pt, double size) {
        try {
            GeckoEvent event = GeckoEvent.get(NativeGeckoEvent.NATIVE_GESTURE_EVENT);
            event.mAction = action;
            event.mCount = 1;
            event.mPoints = new Point[1];

            PointF geckoPoint = new PointF(pt.x, pt.y);
            geckoPoint = GeckoAppShell.getLayerView().convertViewPointToLayerPoint(geckoPoint);

            if (geckoPoint == null) {
                // This could happen if Gecko isn't ready yet.
                return null;
            }

            event.mPoints[0] = new Point(Math.round(geckoPoint.x), Math.round(geckoPoint.y));

            event.mX = size;
            event.mTime = System.currentTimeMillis();
            return event;
        } catch (Exception e) {
            // This can happen if Gecko isn't ready yet
            return null;
        }
    }

    /**
     * Creates a GeckoEvent that contains the data from the MotionEvent.
     * The keepInViewCoordinates parameter can be set to false to convert from the Java
     * coordinate system (device pixels relative to the LayerView) to a coordinate system
     * relative to gecko's coordinate system (CSS pixels relative to gecko scroll position).
     */
    public static GeckoEvent createMotionEvent(MotionEvent m, boolean keepInViewCoordinates) {
        GeckoEvent event = GeckoEvent.get(NativeGeckoEvent.MOTION_EVENT);
        event.initMotionEvent(m, keepInViewCoordinates);
        return event;
    }

    /**
     * Creates a GeckoEvent that contains the data from the LongPressEvent, to be
     * dispatched in CSS pixels relative to gecko's scroll position.
     */
    public static GeckoEvent createLongPressEvent(MotionEvent m) {
        GeckoEvent event = GeckoEvent.get(NativeGeckoEvent.LONG_PRESS);
        event.initMotionEvent(m, false);
        return event;
    }

    private void initMotionEvent(MotionEvent m, boolean keepInViewCoordinates) {
        mAction = m.getActionMasked();
        mTime = (System.currentTimeMillis() - SystemClock.elapsedRealtime()) + m.getEventTime();
        mMetaState = m.getMetaState();

        switch (mAction) {
            case MotionEvent.ACTION_CANCEL:
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_POINTER_UP:
            case MotionEvent.ACTION_POINTER_DOWN:
            case MotionEvent.ACTION_DOWN:
            case MotionEvent.ACTION_MOVE:
            case MotionEvent.ACTION_HOVER_ENTER:
            case MotionEvent.ACTION_HOVER_MOVE:
            case MotionEvent.ACTION_HOVER_EXIT: {
                mCount = m.getPointerCount();
                mPoints = new Point[mCount];
                mPointIndicies = new int[mCount];
                mOrientations = new float[mCount];
                mPressures = new float[mCount];
                mToolTypes = new int[mCount];
                mPointRadii = new Point[mCount];
                mPointerIndex = m.getActionIndex();
                for (int i = 0; i < mCount; i++) {
                    addMotionPoint(i, i, m, keepInViewCoordinates);
                }
                break;
            }
            default: {
                mCount = 0;
                mPointerIndex = -1;
                mPoints = new Point[mCount];
                mPointIndicies = new int[mCount];
                mOrientations = new float[mCount];
                mPressures = new float[mCount];
                mToolTypes = new int[mCount];
                mPointRadii = new Point[mCount];
            }
        }
    }

    private void addMotionPoint(int index, int eventIndex, MotionEvent event, boolean keepInViewCoordinates) {
        try {
            PointF geckoPoint = new PointF(event.getX(eventIndex), event.getY(eventIndex));
            if (!keepInViewCoordinates) {
                geckoPoint = GeckoAppShell.getLayerView().convertViewPointToLayerPoint(geckoPoint);
            }

            mPoints[index] = new Point(Math.round(geckoPoint.x), Math.round(geckoPoint.y));
            mPointIndicies[index] = event.getPointerId(eventIndex);

            double radians = event.getOrientation(eventIndex);
            mOrientations[index] = (float) Math.toDegrees(radians);
            // w3c touchevents spec does not allow orientations == 90
            // this shifts it to -90, which will be shifted to zero below
            if (mOrientations[index] == 90)
                mOrientations[index] = -90;

            // w3c touchevent radius are given by an orientation between 0 and 90
            // the radius is found by removing the orientation and measuring the x and y
            // radius of the resulting ellipse
            // for android orientations >= 0 and < 90, the major axis should correspond to
            // just reporting the y radius as the major one, and x as minor
            // however, for a radius < 0, we have to shift the orientation by adding 90, and
            // reverse which radius is major and minor
            if (mOrientations[index] < 0) {
                mOrientations[index] += 90;
                mPointRadii[index] = new Point((int)event.getToolMajor(eventIndex)/2,
                                               (int)event.getToolMinor(eventIndex)/2);
            } else {
                mPointRadii[index] = new Point((int)event.getToolMinor(eventIndex)/2,
                                               (int)event.getToolMajor(eventIndex)/2);
            }

            if (!keepInViewCoordinates) {
                // If we are converting to gecko CSS pixels, then we should adjust the
                // radii as well
                float zoom = GeckoAppShell.getLayerView().getViewportMetrics().zoomFactor;
                mPointRadii[index].x /= zoom;
                mPointRadii[index].y /= zoom;
            }
            mPressures[index] = event.getPressure(eventIndex);
            if (Versions.feature14Plus) {
                mToolTypes[index] = event.getToolType(index);
            }
        } catch (Exception ex) {
            Log.e(LOGTAG, "Error creating motion point " + index, ex);
            mPointRadii[index] = new Point(0, 0);
            mPoints[index] = new Point(0, 0);
        }
    }

    private static int HalSensorAccuracyFor(int androidAccuracy) {
        switch (androidAccuracy) {
        case SensorManager.SENSOR_STATUS_UNRELIABLE:
            return GeckoHalDefines.SENSOR_ACCURACY_UNRELIABLE;
        case SensorManager.SENSOR_STATUS_ACCURACY_LOW:
            return GeckoHalDefines.SENSOR_ACCURACY_LOW;
        case SensorManager.SENSOR_STATUS_ACCURACY_MEDIUM:
            return GeckoHalDefines.SENSOR_ACCURACY_MED;
        case SensorManager.SENSOR_STATUS_ACCURACY_HIGH:
            return GeckoHalDefines.SENSOR_ACCURACY_HIGH;
        }
        return GeckoHalDefines.SENSOR_ACCURACY_UNKNOWN;
    }

    public static GeckoEvent createSensorEvent(SensorEvent s) {
        int sensor_type = s.sensor.getType();
        GeckoEvent event = null;

        switch(sensor_type) {

        case Sensor.TYPE_ACCELEROMETER:
            event = GeckoEvent.get(NativeGeckoEvent.SENSOR_EVENT);
            event.mFlags = GeckoHalDefines.SENSOR_ACCELERATION;
            event.mMetaState = HalSensorAccuracyFor(s.accuracy);
            event.mX = s.values[0];
            event.mY = s.values[1];
            event.mZ = s.values[2];
            break;

        case Sensor.TYPE_LINEAR_ACCELERATION:
            event = GeckoEvent.get(NativeGeckoEvent.SENSOR_EVENT);
            event.mFlags = GeckoHalDefines.SENSOR_LINEAR_ACCELERATION;
            event.mMetaState = HalSensorAccuracyFor(s.accuracy);
            event.mX = s.values[0];
            event.mY = s.values[1];
            event.mZ = s.values[2];
            break;

        case Sensor.TYPE_ORIENTATION:
            event = GeckoEvent.get(NativeGeckoEvent.SENSOR_EVENT);
            event.mFlags = GeckoHalDefines.SENSOR_ORIENTATION;
            event.mMetaState = HalSensorAccuracyFor(s.accuracy);
            event.mX = s.values[0];
            event.mY = s.values[1];
            event.mZ = s.values[2];
            break;

        case Sensor.TYPE_GYROSCOPE:
            event = GeckoEvent.get(NativeGeckoEvent.SENSOR_EVENT);
            event.mFlags = GeckoHalDefines.SENSOR_GYROSCOPE;
            event.mMetaState = HalSensorAccuracyFor(s.accuracy);
            event.mX = Math.toDegrees(s.values[0]);
            event.mY = Math.toDegrees(s.values[1]);
            event.mZ = Math.toDegrees(s.values[2]);
            break;

        case Sensor.TYPE_PROXIMITY:
            event = GeckoEvent.get(NativeGeckoEvent.SENSOR_EVENT);
            event.mFlags = GeckoHalDefines.SENSOR_PROXIMITY;
            event.mMetaState = HalSensorAccuracyFor(s.accuracy);
            event.mX = s.values[0];
            event.mY = 0;
            event.mZ = s.sensor.getMaximumRange();
            break;

        case Sensor.TYPE_LIGHT:
            event = GeckoEvent.get(NativeGeckoEvent.SENSOR_EVENT);
            event.mFlags = GeckoHalDefines.SENSOR_LIGHT;
            event.mMetaState = HalSensorAccuracyFor(s.accuracy);
            event.mX = s.values[0];
            break;

        case Sensor.TYPE_ROTATION_VECTOR:
            event = GeckoEvent.get(NativeGeckoEvent.SENSOR_EVENT);
            event.mFlags = GeckoHalDefines.SENSOR_ROTATION_VECTOR;
            event.mMetaState = HalSensorAccuracyFor(s.accuracy);
            event.mX = s.values[0];
            event.mY = s.values[1];
            event.mZ = s.values[2];
            if (s.values.length >= 4) {
                event.mW = s.values[3];
            } else {
                // s.values[3] was optional in API <= 18, so we need to compute it
                // The values form a unit quaternion, so we can compute the angle of
                // rotation purely based on the given 3 values.
                event.mW = 1 - s.values[0]*s.values[0] - s.values[1]*s.values[1] - s.values[2]*s.values[2];
                event.mW = (event.mW > 0.0) ? Math.sqrt(event.mW) : 0.0;
            }
            break;

        // case Sensor.TYPE_GAME_ROTATION_VECTOR: // API >= 18
        case 15:
            event = GeckoEvent.get(NativeGeckoEvent.SENSOR_EVENT);
            event.mFlags = GeckoHalDefines.SENSOR_GAME_ROTATION_VECTOR;
            event.mMetaState = HalSensorAccuracyFor(s.accuracy);
            event.mX = s.values[0];
            event.mY = s.values[1];
            event.mZ = s.values[2];
            event.mW = s.values[3];
            break;
        }
        return event;
    }

    public static GeckoEvent createObjectEvent(final int action, final Object object) {
        GeckoEvent event = GeckoEvent.get(NativeGeckoEvent.PROCESS_OBJECT);
        event.mAction = action;
        event.mObject = object;
        return event;
    }

    public static GeckoEvent createLocationEvent(Location l) {
        GeckoEvent event = GeckoEvent.get(NativeGeckoEvent.LOCATION_EVENT);
        event.mLocation = l;
        return event;
    }

    public static GeckoEvent createIMEEvent(ImeAction action) {
        GeckoEvent event = GeckoEvent.get(NativeGeckoEvent.IME_EVENT);
        event.mAction = action.value;
        return event;
    }

    public static GeckoEvent createIMEKeyEvent(KeyEvent k) {
        GeckoEvent event = GeckoEvent.get(NativeGeckoEvent.IME_KEY_EVENT);
        event.initKeyEvent(k, k.getAction(), 0);
        return event;
    }

    public static GeckoEvent createIMEReplaceEvent(int start, int end, String text) {
        return createIMETextEvent(false, start, end, text);
    }

    public static GeckoEvent createIMEComposeEvent(int start, int end, String text) {
        return createIMETextEvent(true, start, end, text);
    }

    private static GeckoEvent createIMETextEvent(boolean compose, int start, int end, String text) {
        GeckoEvent event = GeckoEvent.get(NativeGeckoEvent.IME_EVENT);
        event.mAction = (compose ? ImeAction.IME_COMPOSE_TEXT : ImeAction.IME_REPLACE_TEXT).value;
        event.mStart = start;
        event.mEnd = end;
        event.mCharacters = text;
        return event;
    }

    public static GeckoEvent createIMESelectEvent(int start, int end) {
        GeckoEvent event = GeckoEvent.get(NativeGeckoEvent.IME_EVENT);
        event.mAction = ImeAction.IME_SET_SELECTION.value;
        event.mStart = start;
        event.mEnd = end;
        return event;
    }

    public static GeckoEvent createIMECompositionEvent(int start, int end) {
        GeckoEvent event = GeckoEvent.get(NativeGeckoEvent.IME_EVENT);
        event.mAction = ImeAction.IME_UPDATE_COMPOSITION.value;
        event.mStart = start;
        event.mEnd = end;
        return event;
    }

    public static GeckoEvent createIMERangeEvent(int start,
                                                 int end, int rangeType,
                                                 int rangeStyles,
                                                 int rangeLineStyle,
                                                 boolean rangeBoldLine,
                                                 int rangeForeColor,
                                                 int rangeBackColor,
                                                 int rangeLineColor) {
        GeckoEvent event = GeckoEvent.get(NativeGeckoEvent.IME_EVENT);
        event.mAction = ImeAction.IME_ADD_COMPOSITION_RANGE.value;
        event.mStart = start;
        event.mEnd = end;
        event.mRangeType = rangeType;
        event.mRangeStyles = rangeStyles;
        event.mRangeLineStyle = rangeLineStyle;
        event.mRangeBoldLine = rangeBoldLine;
        event.mRangeForeColor = rangeForeColor;
        event.mRangeBackColor = rangeBackColor;
        event.mRangeLineColor = rangeLineColor;
        return event;
    }

    public static GeckoEvent createSizeChangedEvent(int w, int h, int screenw, int screenh) {
        GeckoEvent event = GeckoEvent.get(NativeGeckoEvent.SIZE_CHANGED);
        event.mPoints = new Point[2];
        event.mPoints[0] = new Point(w, h);
        event.mPoints[1] = new Point(screenw, screenh);
        return event;
    }

    @RobocopTarget
    public static GeckoEvent createBroadcastEvent(String subject, String data) {
        GeckoEvent event = GeckoEvent.get(NativeGeckoEvent.BROADCAST);
        event.mCharacters = subject;
        event.mCharactersExtra = data;
        return event;
    }

    public static GeckoEvent createViewportEvent(ImmutableViewportMetrics metrics, DisplayPortMetrics displayPort) {
        GeckoEvent event = GeckoEvent.get(NativeGeckoEvent.VIEWPORT);
        event.mCharacters = "Viewport:Change";
        StringBuilder sb = new StringBuilder(256);
        sb.append("{ \"x\" : ").append(metrics.viewportRectLeft)
          .append(", \"y\" : ").append(metrics.viewportRectTop)
          .append(", \"zoom\" : ").append(metrics.zoomFactor)
          .append(", \"fixedMarginLeft\" : ").append(metrics.marginLeft)
          .append(", \"fixedMarginTop\" : ").append(metrics.marginTop)
          .append(", \"fixedMarginRight\" : ").append(metrics.marginRight)
          .append(", \"fixedMarginBottom\" : ").append(metrics.marginBottom)
          .append(", \"displayPort\" :").append(displayPort.toJSON())
          .append('}');
        event.mCharactersExtra = sb.toString();
        return event;
    }

    public static GeckoEvent createURILoadEvent(String uri) {
        GeckoEvent event = GeckoEvent.get(NativeGeckoEvent.LOAD_URI);
        event.mCharacters = uri;
        event.mCharactersExtra = "";
        return event;
    }

    public static GeckoEvent createBookmarkLoadEvent(String uri) {
        GeckoEvent event = GeckoEvent.get(NativeGeckoEvent.LOAD_URI);
        event.mCharacters = uri;
        event.mCharactersExtra = "-bookmark";
        return event;
    }

    public static GeckoEvent createVisitedEvent(String data) {
        GeckoEvent event = GeckoEvent.get(NativeGeckoEvent.VISITED);
        event.mCharacters = data;
        return event;
    }

    public static GeckoEvent createNetworkEvent(int connectionType, boolean isWifi, int DHCPGateway) {
        GeckoEvent event = GeckoEvent.get(NativeGeckoEvent.NETWORK_CHANGED);
        event.mConnectionType = connectionType;
        event.mIsWifi = isWifi;
        event.mDHCPGateway = DHCPGateway;
        return event;
    }

    public static GeckoEvent createThumbnailEvent(int tabId, int bufw, int bufh, ByteBuffer buffer) {
        GeckoEvent event = GeckoEvent.get(NativeGeckoEvent.THUMBNAIL);
        event.mPoints = new Point[1];
        event.mPoints[0] = new Point(bufw, bufh);
        event.mMetaState = tabId;
        event.mBuffer = buffer;
        return event;
    }

    public static GeckoEvent createZoomedViewEvent(int tabId, int x, int y, int bufw, int bufh, float scaleFactor, ByteBuffer buffer) {
        GeckoEvent event = GeckoEvent.get(NativeGeckoEvent.ZOOMEDVIEW);
        event.mPoints = new Point[2];
        event.mPoints[0] = new Point(x, y);
        event.mPoints[1] = new Point(bufw, bufh);
        event.mX = (double) scaleFactor;
        event.mMetaState = tabId;
        event.mBuffer = buffer;
        return event;
    }

    public static GeckoEvent createScreenOrientationEvent(short aScreenOrientation) {
        GeckoEvent event = GeckoEvent.get(NativeGeckoEvent.SCREENORIENTATION_CHANGED);
        event.mScreenOrientation = aScreenOrientation;
        return event;
    }

    public static GeckoEvent createCallObserverEvent(String observerKey, String topic, String data) {
        GeckoEvent event = GeckoEvent.get(NativeGeckoEvent.CALL_OBSERVER);
        event.mCharacters = observerKey;
        event.mCharactersExtra = topic;
        event.mData = data;
        return event;
    }

    public static GeckoEvent createRemoveObserverEvent(String observerKey) {
        GeckoEvent event = GeckoEvent.get(NativeGeckoEvent.REMOVE_OBSERVER);
        event.mCharacters = observerKey;
        return event;
    }

    @RobocopTarget
    public static GeckoEvent createPreferencesObserveEvent(int requestId, String[] prefNames) {
        GeckoEvent event = GeckoEvent.get(NativeGeckoEvent.PREFERENCES_OBSERVE);
        event.mCount = requestId;
        event.mPrefNames = prefNames;
        return event;
    }

    @RobocopTarget
    public static GeckoEvent createPreferencesGetEvent(int requestId, String[] prefNames) {
        GeckoEvent event = GeckoEvent.get(NativeGeckoEvent.PREFERENCES_GET);
        event.mCount = requestId;
        event.mPrefNames = prefNames;
        return event;
    }

    @RobocopTarget
    public static GeckoEvent createPreferencesRemoveObserversEvent(int requestId) {
        GeckoEvent event = GeckoEvent.get(NativeGeckoEvent.PREFERENCES_REMOVE_OBSERVERS);
        event.mCount = requestId;
        return event;
    }

    public static GeckoEvent createLowMemoryEvent(int level) {
        GeckoEvent event = GeckoEvent.get(NativeGeckoEvent.LOW_MEMORY);
        event.mMetaState = level;
        return event;
    }

    public static GeckoEvent createNetworkLinkChangeEvent(String status) {
        GeckoEvent event = GeckoEvent.get(NativeGeckoEvent.NETWORK_LINK_CHANGE);
        event.mCharacters = status;
        return event;
    }

    public static GeckoEvent createTelemetryHistogramAddEvent(String histogram,
                                                              int value) {
        GeckoEvent event = GeckoEvent.get(NativeGeckoEvent.TELEMETRY_HISTOGRAM_ADD);
        event.mCharacters = histogram;
        event.mCount = value;
        return event;
    }

    public static GeckoEvent createTelemetryUISessionStartEvent(String session, long timestamp) {
        GeckoEvent event = GeckoEvent.get(NativeGeckoEvent.TELEMETRY_UI_SESSION_START);
        event.mCharacters = session;
        event.mTime = timestamp;
        return event;
    }

    public static GeckoEvent createTelemetryUISessionStopEvent(String session, String reason, long timestamp) {
        GeckoEvent event = GeckoEvent.get(NativeGeckoEvent.TELEMETRY_UI_SESSION_STOP);
        event.mCharacters = session;
        event.mCharactersExtra = reason;
        event.mTime = timestamp;
        return event;
    }

    public static GeckoEvent createTelemetryUIEvent(String action, String method, long timestamp, String extras) {
        GeckoEvent event = GeckoEvent.get(NativeGeckoEvent.TELEMETRY_UI_EVENT);
        event.mData = action;
        event.mCharacters = method;
        event.mCharactersExtra = extras;
        event.mTime = timestamp;
        return event;
    }

    public static GeckoEvent createGamepadAddRemoveEvent(int id, boolean added) {
        GeckoEvent event = GeckoEvent.get(NativeGeckoEvent.GAMEPAD_ADDREMOVE);
        event.mID = id;
        event.mAction = added ? ACTION_GAMEPAD_ADDED : ACTION_GAMEPAD_REMOVED;
        return event;
    }

    private static int boolArrayToBitfield(boolean[] array) {
        int bits = 0;
        for (int i = 0; i < array.length; i++) {
            if (array[i]) {
                bits |= 1<<i;
            }
        }
        return bits;
    }

    public static GeckoEvent createGamepadButtonEvent(int id,
                                                      int which,
                                                      boolean pressed,
                                                      float value) {
        GeckoEvent event = GeckoEvent.get(NativeGeckoEvent.GAMEPAD_DATA);
        event.mID = id;
        event.mAction = ACTION_GAMEPAD_BUTTON;
        event.mGamepadButton = which;
        event.mGamepadButtonPressed = pressed;
        event.mGamepadButtonValue = value;
        return event;
    }

    public static GeckoEvent createGamepadAxisEvent(int id, boolean[] valid,
                                                    float[] values) {
        GeckoEvent event = GeckoEvent.get(NativeGeckoEvent.GAMEPAD_DATA);
        event.mID = id;
        event.mAction = ACTION_GAMEPAD_AXES;
        event.mFlags = boolArrayToBitfield(valid);
        event.mCount = values.length;
        event.mGamepadValues = values;
        return event;
    }

    public void setAckNeeded(boolean ackNeeded) {
        mAckNeeded = ackNeeded;
    }
}
