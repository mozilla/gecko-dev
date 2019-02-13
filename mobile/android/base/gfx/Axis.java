/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import java.util.HashMap;
import java.util.Map;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.PrefsHelper;
import org.mozilla.gecko.util.FloatUtils;

import android.os.SystemClock;
import android.util.Log;
import android.view.View;

/**
 * This class represents the physics for one axis of movement (i.e. either
 * horizontal or vertical). It tracks the different properties of movement
 * like displacement, velocity, viewport dimensions, etc. pertaining to
 * a particular axis.
 */
abstract class Axis {
    private static final String LOGTAG = "GeckoAxis";

    private static final String PREF_SCROLLING_FRICTION_SLOW = "ui.scrolling.friction_slow";
    private static final String PREF_SCROLLING_FRICTION_FAST = "ui.scrolling.friction_fast";
    private static final String PREF_SCROLLING_MAX_EVENT_ACCELERATION = "ui.scrolling.max_event_acceleration";
    private static final String PREF_SCROLLING_OVERSCROLL_DECEL_RATE = "ui.scrolling.overscroll_decel_rate";
    private static final String PREF_SCROLLING_OVERSCROLL_SNAP_LIMIT = "ui.scrolling.overscroll_snap_limit";
    private static final String PREF_SCROLLING_MIN_SCROLLABLE_DISTANCE = "ui.scrolling.min_scrollable_distance";
    private static final String PREF_FLING_ACCEL_INTERVAL = "ui.scrolling.fling_accel_interval";
    private static final String PREF_FLING_ACCEL_BASE_MULTIPLIER = "ui.scrolling.fling_accel_base_multiplier";
    private static final String PREF_FLING_ACCEL_SUPPLEMENTAL_MULTIPLIER = "ui.scrolling.fling_accel_supplemental_multiplier";
    private static final String PREF_FLING_CURVE_FUNCTION_X1 = "ui.scrolling.fling_curve_function_x1";
    private static final String PREF_FLING_CURVE_FUNCTION_Y1 = "ui.scrolling.fling_curve_function_y1";
    private static final String PREF_FLING_CURVE_FUNCTION_X2 = "ui.scrolling.fling_curve_function_x2";
    private static final String PREF_FLING_CURVE_FUNCTION_Y2 = "ui.scrolling.fling_curve_function_y2";
    private static final String PREF_FLING_CURVE_THRESHOLD_VELOCITY = "ui.scrolling.fling_curve_threshold_velocity";
    private static final String PREF_FLING_CURVE_MAXIMUM_VELOCITY = "ui.scrolling.fling_curve_max_velocity";
    private static final String PREF_FLING_CURVE_NEWTON_ITERATIONS = "ui.scrolling.fling_curve_newton_iterations";

    // This fraction of velocity remains after every animation frame when the velocity is low.
    private static float FRICTION_SLOW;
    // This fraction of velocity remains after every animation frame when the velocity is high.
    private static float FRICTION_FAST;
    // Below this velocity (in pixels per frame), the friction starts increasing from FRICTION_FAST
    // to FRICTION_SLOW.
    private static float VELOCITY_THRESHOLD;
    // The maximum velocity change factor between events, per ms, in %.
    // Direction changes are excluded.
    private static float MAX_EVENT_ACCELERATION;

    // The rate of deceleration when the surface has overscrolled.
    private static float OVERSCROLL_DECEL_RATE;
    // The percentage of the surface which can be overscrolled before it must snap back.
    private static float SNAP_LIMIT;

    // The minimum amount of space that must be present for an axis to be considered scrollable,
    // in pixels.
    private static float MIN_SCROLLABLE_DISTANCE;

    // The interval within which if two flings are done then scrolling effect is accelerated.
    private static long FLING_ACCEL_INTERVAL;

    // The multiplication constant of the base velocity in case of accelerated scrolling.
    private static float FLING_ACCEL_BASE_MULTIPLIER;

    // The multiplication constant of the supplemental velocity in case of accelerated scrolling.
    private static float FLING_ACCEL_SUPPLEMENTAL_MULTIPLIER;

    // x co-ordinate of the second bezier control point
    private static float FLING_CURVE_FUNCTION_X1;

    // y co-ordinate of the second bezier control point
    private static float FLING_CURVE_FUNCTION_Y1;

    // x co-ordinate of the third bezier control point
    private static float FLING_CURVE_FUNCTION_X2;

    // y co-ordinate of the third bezier control point
    private static float FLING_CURVE_FUNCTION_Y2;

    // Minimum velocity for curve to be implemented i.e fling curving
    private static float FLING_CURVE_THRESHOLD_VELOCITY;

    // Maximum permitted velocity
    private static float FLING_CURVE_MAXIMUM_VELOCITY;

    // Number of iterations in the Newton-Raphson method
    private static int FLING_CURVE_NEWTON_ITERATIONS;

    private static float getFloatPref(Map<String, Integer> prefs, String prefName, int defaultValue) {
        Integer value = (prefs == null ? null : prefs.get(prefName));
        return (value == null || value < 0 ? defaultValue : value) / 1000f;
    }

    private static int getIntPref(Map<String, Integer> prefs, String prefName, int defaultValue) {
        Integer value = (prefs == null ? null : prefs.get(prefName));
        return (value == null || value < 0 ? defaultValue : value);
    }

    static void initPrefs() {
        final String[] prefs = { PREF_SCROLLING_FRICTION_FAST,
                                 PREF_SCROLLING_FRICTION_SLOW,
                                 PREF_SCROLLING_MAX_EVENT_ACCELERATION,
                                 PREF_SCROLLING_OVERSCROLL_DECEL_RATE,
                                 PREF_SCROLLING_OVERSCROLL_SNAP_LIMIT,
                                 PREF_SCROLLING_MIN_SCROLLABLE_DISTANCE,
                                 PREF_FLING_ACCEL_INTERVAL,
                                 PREF_FLING_ACCEL_BASE_MULTIPLIER,
                                 PREF_FLING_ACCEL_SUPPLEMENTAL_MULTIPLIER,
                                 PREF_FLING_CURVE_FUNCTION_X1,
                                 PREF_FLING_CURVE_FUNCTION_Y1,
                                 PREF_FLING_CURVE_FUNCTION_X2,
                                 PREF_FLING_CURVE_FUNCTION_Y2,
                                 PREF_FLING_CURVE_THRESHOLD_VELOCITY,
                                 PREF_FLING_CURVE_MAXIMUM_VELOCITY,
                                 PREF_FLING_CURVE_NEWTON_ITERATIONS };

        PrefsHelper.getPrefs(prefs, new PrefsHelper.PrefHandlerBase() {
            Map<String, Integer> mPrefs = new HashMap<String, Integer>();

            @Override public void prefValue(String name, int value) {
                mPrefs.put(name, value);
            }

            @Override public void finish() {
                setPrefs(mPrefs);
            }
        });
    }

    static final float MS_PER_FRAME = 1000.0f / 60.0f;
    static final long NS_PER_FRAME = Math.round(1000000000f / 60f);
    private static final float FRAMERATE_MULTIPLIER = (1000f/60f) / MS_PER_FRAME;
    private static final int FLING_VELOCITY_POINTS = 8;

    //  The values we use for friction are based on a 16.6ms frame, adjust them to currentNsPerFrame:
    static float getFrameAdjustedFriction(float baseFriction, long currentNsPerFrame) {
        float framerateMultiplier = (float)currentNsPerFrame / NS_PER_FRAME;
        return (float)Math.pow(Math.E, (Math.log(baseFriction) / framerateMultiplier));
    }

    static void setPrefs(Map<String, Integer> prefs) {
        FRICTION_SLOW = getFloatPref(prefs, PREF_SCROLLING_FRICTION_SLOW, 850);
        FRICTION_FAST = getFloatPref(prefs, PREF_SCROLLING_FRICTION_FAST, 970);
        VELOCITY_THRESHOLD = 10 / FRAMERATE_MULTIPLIER;
        MAX_EVENT_ACCELERATION = getFloatPref(prefs, PREF_SCROLLING_MAX_EVENT_ACCELERATION, GeckoAppShell.getDpi() > 300 ? 100 : 40);
        OVERSCROLL_DECEL_RATE = getFloatPref(prefs, PREF_SCROLLING_OVERSCROLL_DECEL_RATE, 40);
        SNAP_LIMIT = getFloatPref(prefs, PREF_SCROLLING_OVERSCROLL_SNAP_LIMIT, 300);
        MIN_SCROLLABLE_DISTANCE = getFloatPref(prefs, PREF_SCROLLING_MIN_SCROLLABLE_DISTANCE, 500);
        FLING_ACCEL_INTERVAL = getIntPref(prefs, PREF_FLING_ACCEL_INTERVAL, 500);
        FLING_ACCEL_BASE_MULTIPLIER = getFloatPref(prefs, PREF_FLING_ACCEL_BASE_MULTIPLIER, 1000);
        FLING_ACCEL_SUPPLEMENTAL_MULTIPLIER = getFloatPref(prefs, PREF_FLING_ACCEL_SUPPLEMENTAL_MULTIPLIER, 1000);
        FLING_CURVE_FUNCTION_X1 = getFloatPref(prefs, PREF_FLING_CURVE_FUNCTION_X1, 410);
        FLING_CURVE_FUNCTION_Y1 = getFloatPref(prefs, PREF_FLING_CURVE_FUNCTION_Y1, 0);
        FLING_CURVE_FUNCTION_X2 = getFloatPref(prefs, PREF_FLING_CURVE_FUNCTION_X2, 800);
        FLING_CURVE_FUNCTION_Y2 = getFloatPref(prefs, PREF_FLING_CURVE_FUNCTION_Y2, 1000);
        FLING_CURVE_THRESHOLD_VELOCITY = getFloatPref(prefs, PREF_FLING_CURVE_THRESHOLD_VELOCITY, 30);
        FLING_CURVE_MAXIMUM_VELOCITY = getFloatPref(prefs, PREF_FLING_CURVE_MAXIMUM_VELOCITY, 70);
        FLING_CURVE_NEWTON_ITERATIONS = getIntPref(prefs, PREF_FLING_CURVE_NEWTON_ITERATIONS, 5);

        Log.i(LOGTAG, "Prefs: " + FRICTION_SLOW + "," + FRICTION_FAST + "," + VELOCITY_THRESHOLD + ","
                + MAX_EVENT_ACCELERATION + "," + OVERSCROLL_DECEL_RATE + "," + SNAP_LIMIT + "," + MIN_SCROLLABLE_DISTANCE);
    }

    static {
        // set the scrolling parameters to default values on startup
        setPrefs(null);
    }

    private enum FlingStates {
        STOPPED,
        PANNING,
        FLINGING,
    }

    private enum Overscroll {
        NONE,
        MINUS,      // Overscrolled in the negative direction
        PLUS,       // Overscrolled in the positive direction
        BOTH,       // Overscrolled in both directions (page is zoomed to smaller than screen)
    }

    private final SubdocumentScrollHelper mSubscroller;

    private int mOverscrollMode; /* Default to only overscrolling if we're allowed to scroll in a direction */
    private float mFirstTouchPos;           /* Position of the first touch event on the current drag. */
    private float mTouchPos;                /* Position of the most recent touch event on the current drag. */
    private float mLastTouchPos;            /* Position of the touch event before touchPos. */
    private float mVelocity;                /* Velocity in this direction; pixels per animation frame. */
    private final float[] mRecentVelocities;/* Circular buffer of recent velocities since last touch start. */
    private int mRecentVelocityCount;       /* Number of values put into mRecentVelocities (unbounded). */
    private boolean mScrollingDisabled;     /* Whether movement on this axis is locked. */
    private boolean mDisableSnap;           /* Whether overscroll snapping is disabled. */
    private float mDisplacement;
    private long mLastFlingTime;
    private float mLastFlingVelocity;

    private FlingStates mFlingState = FlingStates.STOPPED; /* The fling state we're in on this axis. */

    protected abstract float getOrigin();
    protected abstract float getViewportLength();
    protected abstract float getPageStart();
    protected abstract float getPageLength();
    protected abstract float getMarginStart();
    protected abstract float getMarginEnd();
    protected abstract boolean marginsHidden();

    Axis(SubdocumentScrollHelper subscroller) {
        mSubscroller = subscroller;
        mOverscrollMode = View.OVER_SCROLL_IF_CONTENT_SCROLLS;
        mRecentVelocities = new float[FLING_VELOCITY_POINTS];
    }

    // Implementors can override these to show effects when the axis overscrolls
    protected void overscrollFling(float velocity) { }
    protected void overscrollPan(float displacement) { }

    public void setOverScrollMode(int overscrollMode) {
        mOverscrollMode = overscrollMode;
    }

    public int getOverScrollMode() {
        return mOverscrollMode;
    }

    private float getViewportEnd() {
        return getOrigin() + getViewportLength();
    }

    private float getPageEnd() {
        return getPageStart() + getPageLength();
    }

    void startTouch(float pos) {
        mVelocity = 0.0f;
        mScrollingDisabled = false;
        mFirstTouchPos = mTouchPos = mLastTouchPos = pos;
        mRecentVelocityCount = 0;
    }

    float panDistance(float currentPos) {
        return currentPos - mFirstTouchPos;
    }

    void setScrollingDisabled(boolean disabled) {
        mScrollingDisabled = disabled;
    }

    void saveTouchPos() {
        mLastTouchPos = mTouchPos;
    }

    // Calculates and return the slope of the curve at given parameter t
    float getSlope(float t) {
        float y1 = FLING_CURVE_FUNCTION_Y1;
        float y2 = FLING_CURVE_FUNCTION_Y2;

        return (3 * y1)
             + t * (6 * y2 - 12 * y1)
             + t * t * (9 * y1 - 9 * y2 + 3);
    }

    // Calculates and returns the value of the bezier curve with the given parameter t and control points p1 and p2
    float cubicBezier(float p1, float p2, float t) {
        return (3 * t * (1-t) * (1-t) * p1)
             + (3 * t * t * (1-t) * p2)
             + (t * t * t);
    }

    // Responsible for mapping the physical velocity to a the velocity obtained after applying bezier curve (with control points (X1,Y1) and (X2,Y2))
    float flingCurve(float By) {
        int ni = FLING_CURVE_NEWTON_ITERATIONS;
        float[] guess = new float[ni];
        float y1 = FLING_CURVE_FUNCTION_Y1;
        float y2 = FLING_CURVE_FUNCTION_Y2;
        guess[0] = By;

        for (int i = 1; i < ni; i++) {
            guess[i] = guess[i-1] - (cubicBezier(y1, y2, guess[i-1]) - By) / getSlope(guess[i-1]);
        }
        // guess[4] is the final approximate root the cubic equation.
        float t = guess[4];

        float x1 = FLING_CURVE_FUNCTION_X1;
        float x2 = FLING_CURVE_FUNCTION_X2;
        return cubicBezier(x1, x2, t);
    }

    void updateWithTouchAt(float pos, float timeDelta) {
        float curveVelocityThreshold = FLING_CURVE_THRESHOLD_VELOCITY * GeckoAppShell.getDpi() * MS_PER_FRAME;
        float maxVelocity = FLING_CURVE_MAXIMUM_VELOCITY * GeckoAppShell.getDpi() * MS_PER_FRAME;

        float newVelocity = (mTouchPos - pos) / timeDelta * MS_PER_FRAME;

        if (Math.abs(newVelocity) > curveVelocityThreshold && Math.abs(newVelocity) < maxVelocity) {
            float sign = Math.signum(newVelocity);
            newVelocity = newVelocity * sign;
            float scale = maxVelocity - curveVelocityThreshold;
            float functInp = (newVelocity - curveVelocityThreshold) / scale;
            float functOut = flingCurve(functInp);
            newVelocity = functOut * scale + curveVelocityThreshold;
            newVelocity = newVelocity * sign;
        }

        mRecentVelocities[mRecentVelocityCount % FLING_VELOCITY_POINTS] = newVelocity;
        mRecentVelocityCount++;

        // If there's a direction change, or current velocity is very low,
        // allow setting of the velocity outright. Otherwise, use the current
        // velocity and a maximum change factor to set the new velocity.
        boolean curVelocityIsLow = Math.abs(mVelocity) < 1.0f / FRAMERATE_MULTIPLIER;
        boolean directionChange = (mVelocity > 0) != (newVelocity > 0);
        if (curVelocityIsLow || (directionChange && !FloatUtils.fuzzyEquals(newVelocity, 0.0f))) {
            mVelocity = newVelocity;
        } else {
            float maxChange = Math.abs(mVelocity * timeDelta * MAX_EVENT_ACCELERATION);
            mVelocity = Math.min(mVelocity + maxChange, Math.max(mVelocity - maxChange, newVelocity));
        }

        mTouchPos = pos;
    }

    boolean overscrolled() {
        return getOverscroll() != Overscroll.NONE;
    }

    private Overscroll getOverscroll() {
        boolean minus = (getOrigin() < getPageStart());
        boolean plus = (getViewportEnd() > getPageEnd());
        if (minus && plus) {
            return Overscroll.BOTH;
        } else if (minus) {
            return Overscroll.MINUS;
        } else if (plus) {
            return Overscroll.PLUS;
        } else {
            return Overscroll.NONE;
        }
    }

    // Returns the amount that the page has been overscrolled. If the page hasn't been
    // overscrolled on this axis, returns 0.
    private float getExcess() {
        switch (getOverscroll()) {
        case MINUS:     return getPageStart() - getOrigin();
        case PLUS:      return getViewportEnd() - getPageEnd();
        case BOTH:      return (getViewportEnd() - getPageEnd()) + (getPageStart() - getOrigin());
        default:        return 0.0f;
        }
    }

    /*
     * Returns true if the page is zoomed in to some degree along this axis such that scrolling is
     * possible and this axis has not been scroll locked while panning. Otherwise, returns false.
     */
    boolean scrollable() {
        // If we're scrolling a subdocument, ignore the viewport length restrictions (since those
        // apply to the top-level document) and only take into account axis locking.
        if (mSubscroller.scrolling()) {
            return !mScrollingDisabled;
        }

        // if we are axis locked, return false
        if (mScrollingDisabled) {
            return false;
        }

        // if there are margins on this axis but they are currently hidden,
        // we must be able to scroll in order to make them visible, so allow
        // scrolling in that case
        if (marginsHidden()) {
            return true;
        }

        // there is scrollable space, and we're not disabled, or the document fits the viewport
        // but we always allow overscroll anyway
        return getViewportLength() <= getPageLength() - MIN_SCROLLABLE_DISTANCE ||
               getOverScrollMode() == View.OVER_SCROLL_ALWAYS;
    }

    /*
     * Returns the resistance, as a multiplier, that should be taken into account when
     * tracking or pinching.
     */
    float getEdgeResistance(boolean forPinching) {
        float excess = getExcess();
        if (excess > 0.0f && (getOverscroll() == Overscroll.BOTH || !forPinching)) {
            // excess can be greater than viewport length, but the resistance
            // must never drop below 0.0
            return Math.max(0.0f, SNAP_LIMIT - excess / getViewportLength());
        }
        return 1.0f;
    }

    /* Returns the velocity. If the axis is locked, returns 0. */
    float getRealVelocity() {
        return scrollable() ? mVelocity : 0f;
    }

    void startPan() {
        mFlingState = FlingStates.PANNING;
    }

    private float calculateFlingVelocity() {
        int usablePoints = Math.min(mRecentVelocityCount, FLING_VELOCITY_POINTS);
        if (usablePoints <= 1) {
            return mVelocity;
        }
        float average = 0;
        for (int i = 0; i < usablePoints; i++) {
            average += mRecentVelocities[i];
        }
        return average / usablePoints;
    }

    float accelerate(float velocity, float lastFlingVelocity){
        return (FLING_ACCEL_BASE_MULTIPLIER * velocity + FLING_ACCEL_SUPPLEMENTAL_MULTIPLIER * lastFlingVelocity);
    }

    void startFling(boolean stopped) {
        mDisableSnap = mSubscroller.scrolling();

        if (stopped) {
            mFlingState = FlingStates.STOPPED;
        } else {
            long now = SystemClock.uptimeMillis();
            mVelocity = calculateFlingVelocity();

            if ((now - mLastFlingTime < FLING_ACCEL_INTERVAL) && Math.signum(mVelocity) == Math.signum(mLastFlingVelocity)) {
                mVelocity = accelerate(mVelocity, mLastFlingVelocity);
            }
            mFlingState = FlingStates.FLINGING;
            mLastFlingVelocity = mVelocity;
            mLastFlingTime = now;
        }
    }

    /* Advances a fling animation by one step. */
    boolean advanceFling(long realNsPerFrame) {
        if (mFlingState != FlingStates.FLINGING) {
            return false;
        }
        if (mSubscroller.scrolling() && !mSubscroller.lastScrollSucceeded()) {
            // if the subdocument stopped scrolling, it's because it reached the end
            // of the subdocument. we don't do overscroll on subdocuments, so there's
            // no point in continuing this fling.
            return false;
        }

        float excess = getExcess();
        Overscroll overscroll = getOverscroll();
        boolean decreasingOverscroll = false;
        if ((overscroll == Overscroll.MINUS && mVelocity > 0) ||
            (overscroll == Overscroll.PLUS && mVelocity < 0))
        {
            decreasingOverscroll = true;
        }

        if (mDisableSnap || FloatUtils.fuzzyEquals(excess, 0.0f) || decreasingOverscroll) {
            // If we aren't overscrolled, just apply friction.
            if (Math.abs(mVelocity) >= VELOCITY_THRESHOLD) {
                mVelocity *= getFrameAdjustedFriction(FRICTION_FAST, realNsPerFrame);
            } else {
                float t = mVelocity / VELOCITY_THRESHOLD;
                mVelocity *= FloatUtils.interpolate(getFrameAdjustedFriction(FRICTION_SLOW, realNsPerFrame),
                                                    getFrameAdjustedFriction(FRICTION_FAST, realNsPerFrame), t);
            }
        } else {
            // Otherwise, decrease the velocity linearly.
            float elasticity = 1.0f - excess / (getViewportLength() * SNAP_LIMIT);
            float overscrollDecelRate = getFrameAdjustedFriction(OVERSCROLL_DECEL_RATE, realNsPerFrame);
            if (overscroll == Overscroll.MINUS) {
                mVelocity = Math.min((mVelocity + overscrollDecelRate) * elasticity, 0.0f);
            } else { // must be Overscroll.PLUS
                mVelocity = Math.max((mVelocity - overscrollDecelRate) * elasticity, 0.0f);
            }
        }

        return true;
    }

    void stopFling() {
        mVelocity = 0.0f;
        mFlingState = FlingStates.STOPPED;
    }

    // Performs displacement of the viewport position according to the current velocity.
    void displace() {
        // if this isn't scrollable just return
        if (!scrollable())
            return;

        if (mFlingState == FlingStates.PANNING)
            mDisplacement += (mLastTouchPos - mTouchPos) * getEdgeResistance(false);
        else
            mDisplacement += mVelocity * getEdgeResistance(false);

        // if overscroll is disabled and we're trying to overscroll, reset the displacement
        // to remove any excess. Using getExcess alone isn't enough here since it relies on
        // getOverscroll which doesn't take into account any new displacment being applied.
        // If we using a subscroller, we don't want to alter the scrolling being done
        if (getOverScrollMode() == View.OVER_SCROLL_NEVER && !mSubscroller.scrolling()) {
            float originalDisplacement = mDisplacement;

            if (mDisplacement + getOrigin() < getPageStart() - getMarginStart()) {
                mDisplacement = getPageStart() - getMarginStart() - getOrigin();
            } else if (mDisplacement + getViewportEnd() > getPageEnd() + getMarginEnd()) {
                mDisplacement = getPageEnd() - getMarginEnd() - getViewportEnd();
            }

            // Return the amount of overscroll so that the overscroll controller can draw it for us
            if (originalDisplacement != mDisplacement) {
                if (mFlingState == FlingStates.FLINGING) {
                    overscrollFling(mVelocity / MS_PER_FRAME * 1000);
                    stopFling();
                } else if (mFlingState == FlingStates.PANNING) {
                    overscrollPan(originalDisplacement - mDisplacement);
                }
            }
        }
    }

    float resetDisplacement() {
        float d = mDisplacement;
        mDisplacement = 0.0f;
        return d;
    }

    void setAutoscrollVelocity(float velocity) {
        if (mFlingState != FlingStates.STOPPED) {
            Log.e(LOGTAG, "Setting autoscroll velocity while in a fling is not allowed!");
            return;
        }
        mVelocity = velocity;
    }
}
