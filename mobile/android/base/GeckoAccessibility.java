/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.util.Arrays;
import java.util.HashSet;
import java.util.List;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.AppConstants.Versions;
import org.mozilla.gecko.gfx.LayerView;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.util.UIAsyncTask;

import android.app.ActivityManager;
import android.app.ActivityManager.RunningServiceInfo;
import android.content.Context;
import android.graphics.Rect;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityManager;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityNodeProvider;

import com.googlecode.eyesfree.braille.selfbraille.SelfBrailleClient;
import com.googlecode.eyesfree.braille.selfbraille.WriteData;

public class GeckoAccessibility {
    private static final String LOGTAG = "GeckoAccessibility";
    private static final int VIRTUAL_ENTRY_POINT_BEFORE = 1;
    private static final int VIRTUAL_CURSOR_PREVIOUS = 2;
    private static final int VIRTUAL_CURSOR_POSITION = 3;
    private static final int VIRTUAL_CURSOR_NEXT = 4;
    private static final int VIRTUAL_ENTRY_POINT_AFTER = 5;

    private static boolean sEnabled;
    // Used to store the JSON message and populate the event later in the code path.
    private static JSONObject sEventMessage;
    private static AccessibilityNodeInfo sVirtualCursorNode;
    private static int sCurrentNode;

    // This is the number Brailleback uses to start indexing routing keys.
    private static final int BRAILLE_CLICK_BASE_INDEX = -275000000;
    private static SelfBrailleClient sSelfBrailleClient;

    private static final HashSet<String> sServiceWhitelist =
        new HashSet<String>(Arrays.asList(new String[] {
                    "com.google.android.marvin.talkback.TalkBackService", // Google Talkback screen reader
                    "com.mot.readout.ScreenReader", // Motorola screen reader
                    "info.spielproject.spiel.SpielService", // Spiel screen reader
                    "es.codefactory.android.app.ma.MAAccessibilityService" // Codefactory Mobile Accessibility screen reader
                }));

    public static void updateAccessibilitySettings (final Context context) {
        new UIAsyncTask.WithoutParams<Void>(ThreadUtils.getBackgroundHandler()) {
                @Override
                public Void doInBackground() {
                    JSONObject ret = new JSONObject();
                    sEnabled = false;
                    AccessibilityManager accessibilityManager =
                        (AccessibilityManager) context.getSystemService(Context.ACCESSIBILITY_SERVICE);
                    if (accessibilityManager.isEnabled()) {
                        ActivityManager activityManager =
                            (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
                        List<RunningServiceInfo> runningServices = activityManager.getRunningServices(Integer.MAX_VALUE);

                        for (RunningServiceInfo runningServiceInfo : runningServices) {
                            sEnabled = sServiceWhitelist.contains(runningServiceInfo.service.getClassName());
                            if (sEnabled)
                                break;
                        }
                        if (Versions.feature16Plus && sEnabled && sSelfBrailleClient == null) {
                            sSelfBrailleClient = new SelfBrailleClient(GeckoAppShell.getContext(), false);
                        }
                    }

                    try {
                        ret.put("enabled", sEnabled);
                    } catch (Exception ex) {
                        Log.e(LOGTAG, "Error building JSON arguments for Accessibility:Settings:", ex);
                    }

                    GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Accessibility:Settings",
                                                                                   ret.toString()));
                    return null;
                }

                @Override
                public void onPostExecute(Void args) {
                    boolean isGeckoApp = false;
                    try {
                        isGeckoApp = context instanceof GeckoApp;
                    } catch (NoClassDefFoundError ex) {}
                    if (isGeckoApp) {
                        // Disable the dynamic toolbar when enabling accessibility.
                        // These features tend not to interact well.
                        ((GeckoApp) context).setAccessibilityEnabled(sEnabled);
                    }
                }
            }.execute();
    }

    private static void populateEventFromJSON (AccessibilityEvent event, JSONObject message) {
        final JSONArray textArray = message.optJSONArray("text");
        if (textArray != null) {
            for (int i = 0; i < textArray.length(); i++)
                event.getText().add(textArray.optString(i));
        }

        event.setContentDescription(message.optString("description"));
        event.setEnabled(message.optBoolean("enabled", true));
        event.setChecked(message.optBoolean("checked"));
        event.setPassword(message.optBoolean("password"));
        event.setAddedCount(message.optInt("addedCount", -1));
        event.setRemovedCount(message.optInt("removedCount", -1));
        event.setFromIndex(message.optInt("fromIndex", -1));
        event.setItemCount(message.optInt("itemCount", -1));
        event.setCurrentItemIndex(message.optInt("currentItemIndex", -1));
        event.setBeforeText(message.optString("beforeText"));
        if (Versions.feature14Plus) {
            event.setToIndex(message.optInt("toIndex", -1));
            event.setScrollable(message.optBoolean("scrollable"));
            event.setScrollX(message.optInt("scrollX", -1));
            event.setScrollY(message.optInt("scrollY", -1));
        }
        if (Versions.feature15Plus) {
            event.setMaxScrollX(message.optInt("maxScrollX", -1));
            event.setMaxScrollY(message.optInt("maxScrollY", -1));
        }
    }

    private static void sendDirectAccessibilityEvent(int eventType, JSONObject message) {
        final AccessibilityEvent accEvent = AccessibilityEvent.obtain(eventType);
        accEvent.setClassName(GeckoAccessibility.class.getName());
        accEvent.setPackageName(GeckoAppShell.getContext().getPackageName());
        populateEventFromJSON(accEvent, message);
        AccessibilityManager accessibilityManager =
            (AccessibilityManager) GeckoAppShell.getContext().getSystemService(Context.ACCESSIBILITY_SERVICE);
        try {
            accessibilityManager.sendAccessibilityEvent(accEvent);
        } catch (IllegalStateException e) {
            // Accessibility is off.
        }
    }

    public static boolean isEnabled() {
        return sEnabled;
    }

    public static void sendAccessibilityEvent (final JSONObject message) {
        if (!sEnabled)
            return;

        final String exitView = message.optString("exitView");
        if (exitView.equals("moveNext")) {
            sCurrentNode = VIRTUAL_ENTRY_POINT_AFTER;
        } else if (exitView.equals("movePrevious")) {
            sCurrentNode = VIRTUAL_ENTRY_POINT_BEFORE;
        } else {
            sCurrentNode = VIRTUAL_CURSOR_POSITION;
        }

        final int eventType = message.optInt("eventType", -1);
        if (eventType < 0) {
            Log.e(LOGTAG, "No accessibility event type provided");
            return;
        }

        if (Versions.preJB) {
            // Before Jelly Bean we send events directly from here while spoofing the source by setting
            // the package and class name manually.
            ThreadUtils.postToBackgroundThread(new Runnable() {
                    @Override
                    public void run() {
                        sendDirectAccessibilityEvent(eventType, message);
                }
            });
        } else {
            // In Jelly Bean we populate an AccessibilityNodeInfo with the minimal amount of data to have
            // it work with TalkBack.
            final LayerView view = GeckoAppShell.getLayerView();
            if (view == null)
                return;

            if (sVirtualCursorNode == null)
                sVirtualCursorNode = AccessibilityNodeInfo.obtain(view, VIRTUAL_CURSOR_POSITION);
            sVirtualCursorNode.setEnabled(message.optBoolean("enabled", true));
            sVirtualCursorNode.setClickable(message.optBoolean("clickable"));
            sVirtualCursorNode.setCheckable(message.optBoolean("checkable"));
            sVirtualCursorNode.setChecked(message.optBoolean("checked"));
            sVirtualCursorNode.setPassword(message.optBoolean("password"));

            final JSONArray textArray = message.optJSONArray("text");
            StringBuilder sb = new StringBuilder();
            if (textArray != null && textArray.length() > 0) {
                sb.append(textArray.optString(0));
                for (int i = 1; i < textArray.length(); i++) {
                    sb.append(" ").append(textArray.optString(i));
                }
            }
            sVirtualCursorNode.setText(sb.toString());
            sVirtualCursorNode.setContentDescription(message.optString("description"));

            JSONObject bounds = message.optJSONObject("bounds");
            if (bounds != null) {
                Rect relativeBounds = new Rect(bounds.optInt("left"), bounds.optInt("top"),
                                               bounds.optInt("right"), bounds.optInt("bottom"));
                sVirtualCursorNode.setBoundsInParent(relativeBounds);
                int[] locationOnScreen = new int[2];
                view.getLocationOnScreen(locationOnScreen);
                Rect screenBounds = new Rect(relativeBounds);
                screenBounds.offset(locationOnScreen[0], locationOnScreen[1]);
                sVirtualCursorNode.setBoundsInScreen(screenBounds);
            }

            final JSONObject braille = message.optJSONObject("brailleOutput");
            if (braille != null) {
                sendBrailleText(view, braille.optString("text"),
                                braille.optInt("selectionStart"), braille.optInt("selectionEnd"));
            }

            ThreadUtils.postToUiThread(new Runnable() {
                    @Override
                    public void run() {
                        // If this is an accessibility focus, a lot of internal voodoo happens so we perform an
                        // accessibility focus action on the view, and it in turn sends the right events.
                        switch (eventType) {
                        case AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUSED:
                            sEventMessage = message;
                            view.performAccessibilityAction(AccessibilityNodeInfo.ACTION_ACCESSIBILITY_FOCUS, null);
                            break;
                        case AccessibilityEvent.TYPE_ANNOUNCEMENT:
                        case AccessibilityEvent.TYPE_VIEW_SCROLLED:
                            sEventMessage = null;
                            final AccessibilityEvent accEvent = AccessibilityEvent.obtain(eventType);
                            view.onInitializeAccessibilityEvent(accEvent);
                            populateEventFromJSON(accEvent, message);
                            view.getParent().requestSendAccessibilityEvent(view, accEvent);
                            break;
                        default:
                            sEventMessage = message;
                            view.sendAccessibilityEvent(eventType);
                            break;
                        }
                    }
                });

        }
    }

    private static void sendBrailleText(final View view, final String text, final int selectionStart, final int selectionEnd) {
        AccessibilityNodeInfo info = AccessibilityNodeInfo.obtain(view, VIRTUAL_CURSOR_POSITION);
        WriteData data = WriteData.forInfo(info);
        data.setText(text);
        // Set either the focus blink or the current caret position/selection
        data.setSelectionStart(selectionStart);
        data.setSelectionEnd(selectionEnd);
        sSelfBrailleClient.write(data);
    }

    public static void setDelegate(LayerView layerview) {
        // Only use this delegate in Jelly Bean.
        if (Versions.feature16Plus) {
            layerview.setAccessibilityDelegate(new GeckoAccessibilityDelegate());
            layerview.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_YES);
        }
    }

    public static void setAccessibilityStateChangeListener(final Context context) {
        // The state change listener is only supported on API14+
        if (Versions.feature14Plus) {
            AccessibilityManager accessibilityManager =
                (AccessibilityManager) context.getSystemService(Context.ACCESSIBILITY_SERVICE);
            accessibilityManager.addAccessibilityStateChangeListener(new AccessibilityManager.AccessibilityStateChangeListener() {
                @Override
                public void onAccessibilityStateChanged(boolean enabled) {
                    updateAccessibilitySettings(context);
                }
            });
        }
    }

    public static void onLayerViewFocusChanged(LayerView layerview, boolean gainFocus) {
        if (sEnabled)
            GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Accessibility:Focus",
                                                                           gainFocus ? "true" : "false"));
    }

    public static class GeckoAccessibilityDelegate extends View.AccessibilityDelegate {
        AccessibilityNodeProvider mAccessibilityNodeProvider;

        @Override
        public void onPopulateAccessibilityEvent (View host, AccessibilityEvent event) {
            super.onPopulateAccessibilityEvent(host, event);
            if (sEventMessage != null) {
                populateEventFromJSON(event, sEventMessage);
                event.setSource(host, sCurrentNode);
            }
            // We save the hover enter event so that we could reuse it for a subsequent accessibility focus event.
            if (event.getEventType() != AccessibilityEvent.TYPE_VIEW_HOVER_ENTER)
                sEventMessage = null;
        }

        @Override
        public AccessibilityNodeProvider getAccessibilityNodeProvider(final View host) {
            if (mAccessibilityNodeProvider == null)
                // The accessibility node structure for web content consists of 5 LayerView child nodes:
                // 1. VIRTUAL_ENTRY_POINT_BEFORE: Represents the entry point before the LayerView.
                // 2. VIRTUAL_CURSOR_PREVIOUS: Represents the virtual cursor position that is previous to the
                // current one.
                // 3. VIRTUAL_CURSOR_POSITION: Represents the current position of the virtual cursor.
                // 4. VIRTUAL_CURSOR_NEXT: Represents the next virtual cursor position.
                // 5. VIRTUAL_ENTRY_POINT_AFTER: Represents the entry point after the LayerView.
                mAccessibilityNodeProvider = new AccessibilityNodeProvider() {
                        @Override
                        public AccessibilityNodeInfo createAccessibilityNodeInfo(int virtualDescendantId) {
                            AccessibilityNodeInfo info = (virtualDescendantId == VIRTUAL_CURSOR_POSITION && sVirtualCursorNode != null) ?
                                AccessibilityNodeInfo.obtain(sVirtualCursorNode) :
                                AccessibilityNodeInfo.obtain(host, virtualDescendantId);

                            switch (virtualDescendantId) {
                            case View.NO_ID:
                                // This is the parent LayerView node, populate it with children.
                                onInitializeAccessibilityNodeInfo(host, info);
                                info.addChild(host, VIRTUAL_ENTRY_POINT_BEFORE);
                                info.addChild(host, VIRTUAL_CURSOR_PREVIOUS);
                                info.addChild(host, VIRTUAL_CURSOR_POSITION);
                                info.addChild(host, VIRTUAL_CURSOR_NEXT);
                                info.addChild(host, VIRTUAL_ENTRY_POINT_AFTER);
                                break;
                            default:
                                info.setParent(host);
                                info.setSource(host, virtualDescendantId);
                                info.setVisibleToUser(host.isShown());
                                info.setPackageName(GeckoAppShell.getContext().getPackageName());
                                info.setClassName(host.getClass().getName());
                                info.setEnabled(true);
                                info.addAction(AccessibilityNodeInfo.ACTION_CLEAR_ACCESSIBILITY_FOCUS);
                                info.addAction(AccessibilityNodeInfo.ACTION_ACCESSIBILITY_FOCUS);
                                info.addAction(AccessibilityNodeInfo.ACTION_CLICK);
                                info.addAction(AccessibilityNodeInfo.ACTION_LONG_CLICK);
                                info.addAction(AccessibilityNodeInfo.ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY);
                                info.addAction(AccessibilityNodeInfo.ACTION_NEXT_AT_MOVEMENT_GRANULARITY);
                                info.setMovementGranularities(AccessibilityNodeInfo.MOVEMENT_GRANULARITY_CHARACTER |
                                                              AccessibilityNodeInfo.MOVEMENT_GRANULARITY_WORD |
                                                              AccessibilityNodeInfo.MOVEMENT_GRANULARITY_PARAGRAPH);
                                break;
                            }
                            return info;
                        }

                        @Override
                        public boolean performAction (int virtualViewId, int action, Bundle arguments) {
                            if (action == AccessibilityNodeInfo.ACTION_ACCESSIBILITY_FOCUS) {
                                // The accessibility focus is permanently on the middle node, VIRTUAL_CURSOR_POSITION.
                                // When accessibility focus is requested on one of its siblings we move the virtual cursor
                                // either forward or backward depending on which sibling was selected.
                                // When we enter the view forward or backward we just ask Gecko to get focus, keeping the current position.

                                switch (virtualViewId) {
                                case VIRTUAL_CURSOR_PREVIOUS:
                                    GeckoAppShell.
                                        sendEventToGecko(GeckoEvent.createBroadcastEvent("Accessibility:PreviousObject", null));
                                    return true;
                                case VIRTUAL_CURSOR_NEXT:
                                    GeckoAppShell.
                                        sendEventToGecko(GeckoEvent.createBroadcastEvent("Accessibility:NextObject", null));
                                    return true;
                                case VIRTUAL_ENTRY_POINT_BEFORE:
                                case VIRTUAL_ENTRY_POINT_AFTER:
                                    GeckoAppShell.
                                        sendEventToGecko(GeckoEvent.createBroadcastEvent("Accessibility:Focus", "true"));
                                default:
                                    break;
                                }
                            } else if (action == AccessibilityNodeInfo.ACTION_CLICK && virtualViewId == VIRTUAL_CURSOR_POSITION) {
                                GeckoAppShell.
                                    sendEventToGecko(GeckoEvent.createBroadcastEvent("Accessibility:ActivateObject", null));
                                return true;
                            } else if (action == AccessibilityNodeInfo.ACTION_LONG_CLICK && virtualViewId == VIRTUAL_CURSOR_POSITION) {
                                GeckoAppShell.
                                    sendEventToGecko(GeckoEvent.createBroadcastEvent("Accessibility:LongPress", null));
                                return true;
                            } else if (action == AccessibilityNodeInfo.ACTION_NEXT_AT_MOVEMENT_GRANULARITY &&
                                       virtualViewId == VIRTUAL_CURSOR_POSITION) {
                                // XXX: Self brailling gives this action with a bogus argument instead of an actual click action;
                                // the argument value is the BRAILLE_CLICK_BASE_INDEX - the index of the routing key that was hit
                                int granularity = arguments.getInt(AccessibilityNodeInfo.ACTION_ARGUMENT_MOVEMENT_GRANULARITY_INT);
                                if (granularity < 0) {
                                    int keyIndex = BRAILLE_CLICK_BASE_INDEX - granularity;
                                    JSONObject activationData = new JSONObject();
                                    try {
                                        activationData.put("keyIndex", keyIndex);
                                    } catch (JSONException e) {
                                        return true;
                                    }
                                    GeckoAppShell.
                                        sendEventToGecko(GeckoEvent.createBroadcastEvent("Accessibility:ActivateObject", activationData.toString()));
                                } else {
                                    JSONObject movementData = new JSONObject();
                                    try {
                                        movementData.put("direction", "Next");
                                        movementData.put("granularity", granularity);
                                    } catch (JSONException e) {
                                        return true;
                                    }
                                    GeckoAppShell.
                                        sendEventToGecko(GeckoEvent.createBroadcastEvent("Accessibility:MoveByGranularity", movementData.toString()));
                                }
                                return true;
                            } else if (action == AccessibilityNodeInfo.ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY &&
                                       virtualViewId == VIRTUAL_CURSOR_POSITION) {
                                JSONObject movementData = new JSONObject();
                                try {
                                    movementData.put("direction", "Previous");
                                    movementData.put("granularity", arguments.getInt(AccessibilityNodeInfo.ACTION_ARGUMENT_MOVEMENT_GRANULARITY_INT));
                                } catch (JSONException e) {
                                    return true;
                                }
                                GeckoAppShell.
                                    sendEventToGecko(GeckoEvent.createBroadcastEvent("Accessibility:MoveByGranularity", movementData.toString()));
                                return true;
                            }
                            return host.performAccessibilityAction(action, arguments);
                        }
                    };

            return mAccessibilityNodeProvider;
        }
    }
}
