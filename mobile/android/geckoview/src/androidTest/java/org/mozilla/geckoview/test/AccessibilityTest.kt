/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.AssertCalled
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.WithDisplay
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.WithDevToolsAPI

import android.graphics.Rect

import android.os.Build
import android.os.Bundle
import android.os.SystemClock

import android.support.test.filters.MediumTest
import android.support.test.InstrumentationRegistry
import android.support.test.runner.AndroidJUnit4
import android.text.InputType
import android.util.SparseLongArray

import android.view.accessibility.AccessibilityNodeInfo
import android.view.accessibility.AccessibilityNodeProvider
import android.view.accessibility.AccessibilityEvent
import android.view.accessibility.AccessibilityRecord
import android.view.View
import android.view.ViewGroup
import android.widget.EditText

import android.widget.FrameLayout

import org.hamcrest.Matchers.*
import org.junit.Test
import org.junit.Before
import org.junit.After
import org.junit.runner.RunWith
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.Setting
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.ReuseSession

const val DISPLAY_WIDTH = 480
const val DISPLAY_HEIGHT = 640

@RunWith(AndroidJUnit4::class)
@MediumTest
@WithDisplay(width = DISPLAY_WIDTH, height = DISPLAY_HEIGHT)
@WithDevToolsAPI
class AccessibilityTest : BaseSessionTest() {
    lateinit var view: View
    val screenRect = Rect(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT)
    val provider: AccessibilityNodeProvider get() = view.accessibilityNodeProvider
    private val nodeInfos = mutableListOf<AccessibilityNodeInfo>()

    // Given a child ID, return the virtual descendent ID.
    private fun getVirtualDescendantId(childId: Long): Int {
        try {
            val getVirtualDescendantIdMethod =
                AccessibilityNodeInfo::class.java.getMethod("getVirtualDescendantId", Long::class.java)
            val virtualDescendantId = getVirtualDescendantIdMethod.invoke(null, childId) as Int
            return if (virtualDescendantId == Int.MAX_VALUE) -1 else virtualDescendantId
        } catch (ex: Exception) {
            return 0
        }
    }

    // Retrieve the virtual descendent ID of the event's source.
    private fun getSourceId(event: AccessibilityEvent): Int {
        try {
            val getSourceIdMethod =
                AccessibilityRecord::class.java.getMethod("getSourceNodeId")
            return getVirtualDescendantId(getSourceIdMethod.invoke(event) as Long)
        } catch (ex: Exception) {
            return 0
        }
    }

    private fun createNodeInfo(id: Int): AccessibilityNodeInfo {
        val node = provider.createAccessibilityNodeInfo(id);
        nodeInfos.add(node)
        return node;
    }

    // Get a child ID by index.
    private fun AccessibilityNodeInfo.getChildId(index: Int): Int =
            getVirtualDescendantId(
                    if (Build.VERSION.SDK_INT >= 21)
                        AccessibilityNodeInfo::class.java.getMethod(
                                "getChildId", Int::class.java).invoke(this, index) as Long
                    else
                        (AccessibilityNodeInfo::class.java.getMethod("getChildNodeIds")
                                .invoke(this) as SparseLongArray).get(index))

    private interface EventDelegate {
        fun onAccessibilityFocused(event: AccessibilityEvent) { }
        fun onClicked(event: AccessibilityEvent) { }
        fun onFocused(event: AccessibilityEvent) { }
        fun onSelected(event: AccessibilityEvent) { }
        fun onScrolled(event: AccessibilityEvent) { }
        fun onTextSelectionChanged(event: AccessibilityEvent) { }
        fun onTextChanged(event: AccessibilityEvent) { }
        fun onTextTraversal(event: AccessibilityEvent) { }
        fun onWinContentChanged(event: AccessibilityEvent) { }
        fun onWinStateChanged(event: AccessibilityEvent) { }
    }

    @Before fun setup() {
        // We initialize a view with a parent and grandparent so that the
        // accessibility events propagate up at least to the parent.
        view = FrameLayout(InstrumentationRegistry.getTargetContext())
        FrameLayout(InstrumentationRegistry.getTargetContext()).addView(view)
        FrameLayout(InstrumentationRegistry.getTargetContext()).addView(view.parent as View)

        // Force on accessibility and assign the session's accessibility
        // object a view.
        sessionRule.setPrefsUntilTestEnd(mapOf("accessibility.force_disabled" to -1))
        mainSession.accessibility.view = view

        // Set up an external delegate that will intercept accessibility events.
        sessionRule.addExternalDelegateUntilTestEnd(
            EventDelegate::class,
        { newDelegate -> (view.parent as View).setAccessibilityDelegate(object : View.AccessibilityDelegate() {
            override fun onRequestSendAccessibilityEvent(host: ViewGroup, child: View, event: AccessibilityEvent): Boolean {
                when (event.eventType) {
                    AccessibilityEvent.TYPE_VIEW_FOCUSED -> newDelegate.onFocused(event)
                    AccessibilityEvent.TYPE_VIEW_CLICKED -> newDelegate.onClicked(event)
                    AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUSED -> newDelegate.onAccessibilityFocused(event)
                    AccessibilityEvent.TYPE_VIEW_SELECTED -> newDelegate.onSelected(event)
                    AccessibilityEvent.TYPE_VIEW_SCROLLED -> newDelegate.onScrolled(event)
                    AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED -> newDelegate.onTextSelectionChanged(event)
                    AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED -> newDelegate.onTextChanged(event)
                    AccessibilityEvent.TYPE_VIEW_TEXT_TRAVERSED_AT_MOVEMENT_GRANULARITY -> newDelegate.onTextTraversal(event)
                    AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED -> newDelegate.onWinContentChanged(event)
                    AccessibilityEvent.TYPE_WINDOW_STATE_CHANGED -> newDelegate.onWinStateChanged(event)
                    else -> {}
                }
                return false
            }
        }) },
        { (view.parent as View).setAccessibilityDelegate(null) },
        object : EventDelegate { })
    }

    @After fun teardown() {
        sessionRule.session.accessibility.view = null
        nodeInfos.forEach { node -> node.recycle() }
    }

    private fun waitForInitialFocus(moveToFirstChild: Boolean = false) {
        // XXX: Sometimes we get the window state change of the initial
        // about:blank page loading. Need to figure out how to ignore that.
        sessionRule.waitUntilCalled(object : EventDelegate {
            @AssertCalled(count = 1)
            override fun onFocused(event: AccessibilityEvent) { }

            @AssertCalled
            override fun onWinStateChanged(event: AccessibilityEvent) { }
        })

        if (moveToFirstChild) {
            provider.performAction(View.NO_ID,
                AccessibilityNodeInfo.ACTION_NEXT_HTML_ELEMENT, null)
        }
    }

    @Test fun testRootNode() {
        assertThat("provider is not null", provider, notNullValue())
        val node = createNodeInfo(AccessibilityNodeProvider.HOST_VIEW_ID)
        assertThat("Root node should have WebView class name",
            node.className.toString(), equalTo("android.webkit.WebView"))
    }

    @Test fun testPageLoad() {
        sessionRule.session.loadTestPath(INPUTS_PATH)

        sessionRule.waitUntilCalled(object : EventDelegate {
            @AssertCalled(count = 1)
            override fun onFocused(event: AccessibilityEvent) { }
        })
    }

    @Test fun testAccessibilityFocus() {
        var nodeId = AccessibilityNodeProvider.HOST_VIEW_ID
        sessionRule.session.loadTestPath(INPUTS_PATH)
        waitForInitialFocus(true)

        sessionRule.waitUntilCalled(object : EventDelegate {
            @AssertCalled(count = 1)
            override fun onAccessibilityFocused(event: AccessibilityEvent) {
                nodeId = getSourceId(event)
                val node = createNodeInfo(nodeId)
                assertThat("Label accessibility focused", node.className.toString(),
                        equalTo("android.view.View"))
                assertThat("Text node should not be focusable", node.isFocusable, equalTo(false))
            }
        })

        provider.performAction(nodeId,
            AccessibilityNodeInfo.ACTION_NEXT_HTML_ELEMENT, null)

        sessionRule.waitUntilCalled(object : EventDelegate {
            @AssertCalled(count = 1)
            override fun onAccessibilityFocused(event: AccessibilityEvent) {
                nodeId = getSourceId(event)
                val node = createNodeInfo(nodeId)
                assertThat("Editbox accessibility focused", node.className.toString(),
                        equalTo("android.widget.EditText"))
                assertThat("Entry node should be focusable", node.isFocusable, equalTo(true))
            }
        })
    }

    @Test fun testTextEntryNode() {
        sessionRule.session.loadString("<input aria-label='Name' value='Tobias'>", "text/html")
        waitForInitialFocus()

        mainSession.evaluateJS("$('input').focus()")

        sessionRule.waitUntilCalled(object : EventDelegate {
            @AssertCalled(count = 1)
            override fun onFocused(event: AccessibilityEvent) {
                val nodeId = getSourceId(event)
                val node = createNodeInfo(nodeId)
                assertThat("Focused EditBox", node.className.toString(),
                        equalTo("android.widget.EditText"))
                if (Build.VERSION.SDK_INT >= 19) {
                    assertThat("Hint has field name",
                            node.extras.getString("AccessibilityNodeInfo.hint"),
                            equalTo("Name"))
                }
            }
        })
    }

    private fun waitUntilTextSelectionChanged(fromIndex: Int, toIndex: Int) {
        var eventFromIndex = 0;
        var eventToIndex = 0;
        do {
            sessionRule.waitUntilCalled(object : EventDelegate {
                override fun onTextSelectionChanged(event: AccessibilityEvent) {
                    eventFromIndex = event.fromIndex;
                    eventToIndex = event.toIndex;
                }
            })
        } while (fromIndex != eventFromIndex || toIndex != eventToIndex)
    }

    private fun waitUntilTextTraversed(fromIndex: Int, toIndex: Int) {
        sessionRule.waitUntilCalled(object : EventDelegate {
            @AssertCalled(count = 1)
            override fun onTextTraversal(event: AccessibilityEvent) {
              assertThat("fromIndex matches", event.fromIndex, equalTo(fromIndex))
              assertThat("toIndex matches", event.toIndex, equalTo(toIndex))
            }
        })
    }

    private fun waitUntilClick(checked: Boolean) {
        sessionRule.waitUntilCalled(object : EventDelegate {
            @AssertCalled(count = 1)
            override fun onClicked(event: AccessibilityEvent) {
                var nodeId = getSourceId(event)
                var node = createNodeInfo(nodeId)
                assertThat("Event's checked state matches", event.isChecked, equalTo(checked))
                assertThat("Checkbox node has correct checked state", node.isChecked, equalTo(checked))
            }
        })
    }

    private fun waitUntilSelect(selected: Boolean) {
        sessionRule.waitUntilCalled(object : EventDelegate {
            @AssertCalled(count = 1)
            override fun onSelected(event: AccessibilityEvent) {
                var nodeId = getSourceId(event)
                var node = createNodeInfo(nodeId)
                assertThat("Selectable node has correct selected state", node.isSelected, equalTo(selected))
            }
        })
    }

    private fun setSelectionArguments(start: Int, end: Int): Bundle {
        val arguments = Bundle(2)
        arguments.putInt(AccessibilityNodeInfo.ACTION_ARGUMENT_SELECTION_START_INT, start)
        arguments.putInt(AccessibilityNodeInfo.ACTION_ARGUMENT_SELECTION_END_INT, end)
        return arguments
    }

    private fun moveByGranularityArguments(granularity: Int, extendSelection: Boolean = false): Bundle {
        val arguments = Bundle(2)
        arguments.putInt(AccessibilityNodeInfo.ACTION_ARGUMENT_MOVEMENT_GRANULARITY_INT, granularity)
        arguments.putBoolean(AccessibilityNodeInfo.ACTION_ARGUMENT_EXTEND_SELECTION_BOOLEAN, extendSelection)
        return arguments
    }

    @Test fun testClipboard() {
        var nodeId = AccessibilityNodeProvider.HOST_VIEW_ID;
        sessionRule.session.loadString("<input value='hello cruel world' id='input'>", "text/html")
        waitForInitialFocus()

        mainSession.evaluateJS("$('input').focus()")

        sessionRule.waitUntilCalled(object : EventDelegate {
            @AssertCalled(count = 1)
            override fun onFocused(event: AccessibilityEvent) {
                nodeId = getSourceId(event)
                val node = createNodeInfo(nodeId)
                assertThat("Focused EditBox", node.className.toString(),
                        equalTo("android.widget.EditText"))
            }

            @AssertCalled(count = 1)
            override fun onTextSelectionChanged(event: AccessibilityEvent) {
                assertThat("fromIndex should be at start", event.fromIndex, equalTo(0))
                assertThat("toIndex should be at start", event.toIndex, equalTo(0))
            }
        })

        provider.performAction(nodeId, AccessibilityNodeInfo.ACTION_SET_SELECTION, setSelectionArguments(5, 11))
        waitUntilTextSelectionChanged(5, 11)

        provider.performAction(nodeId, AccessibilityNodeInfo.ACTION_COPY, null)

        provider.performAction(nodeId, AccessibilityNodeInfo.ACTION_SET_SELECTION, setSelectionArguments(11, 11))
        waitUntilTextSelectionChanged(11, 11)

        provider.performAction(nodeId, AccessibilityNodeInfo.ACTION_PASTE, null)
        sessionRule.waitUntilCalled(object : EventDelegate {
            @AssertCalled(count = 1)
            override fun onTextChanged(event: AccessibilityEvent) {
                assertThat("text should be pasted", event.text[0].toString(), equalTo("hello cruel cruel world"))
            }
        })

        provider.performAction(nodeId, AccessibilityNodeInfo.ACTION_SET_SELECTION, setSelectionArguments(17, 23))
        waitUntilTextSelectionChanged(17, 23)

        provider.performAction(nodeId, AccessibilityNodeInfo.ACTION_PASTE, null)
        sessionRule.waitUntilCalled(object : EventDelegate {
            @AssertCalled
            override fun onTextChanged(event: AccessibilityEvent) {
                assertThat("text should be pasted", event.text[0].toString(), equalTo("hello cruel cruel cruel"))
            }
        })
    }

    @Test fun testMoveByCharacter() {
        var nodeId = AccessibilityNodeProvider.HOST_VIEW_ID
        sessionRule.session.loadTestPath(LOREM_IPSUM_HTML_PATH)
        waitForInitialFocus(true)

        sessionRule.waitUntilCalled(object : EventDelegate {
            @AssertCalled(count = 1)
            override fun onAccessibilityFocused(event: AccessibilityEvent) {
                nodeId = getSourceId(event)
                val node = createNodeInfo(nodeId)
                assertThat("Accessibility focus on first paragraph", node.text as String, startsWith("Lorem ipsum"))
            }
        })

        provider.performAction(nodeId,
                AccessibilityNodeInfo.ACTION_NEXT_AT_MOVEMENT_GRANULARITY,
                moveByGranularityArguments(AccessibilityNodeInfo.MOVEMENT_GRANULARITY_CHARACTER))
        waitUntilTextTraversed(0, 1) // "L"

        provider.performAction(nodeId,
                AccessibilityNodeInfo.ACTION_NEXT_AT_MOVEMENT_GRANULARITY,
                moveByGranularityArguments(AccessibilityNodeInfo.MOVEMENT_GRANULARITY_CHARACTER))
        waitUntilTextTraversed(1, 2) // "o"

        provider.performAction(nodeId,
                AccessibilityNodeInfo.ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY,
                moveByGranularityArguments(AccessibilityNodeInfo.MOVEMENT_GRANULARITY_CHARACTER))
        waitUntilTextTraversed(0, 1) // "L"
    }

    @Test fun testMoveByWord() {
        var nodeId = AccessibilityNodeProvider.HOST_VIEW_ID
        sessionRule.session.loadTestPath(LOREM_IPSUM_HTML_PATH)
        waitForInitialFocus(true)

        sessionRule.waitUntilCalled(object : EventDelegate {
            @AssertCalled(count = 1)
            override fun onAccessibilityFocused(event: AccessibilityEvent) {
                nodeId = getSourceId(event)
                val node = createNodeInfo(nodeId)
                assertThat("Accessibility focus on first paragraph", node.text as String, startsWith("Lorem ipsum"))
            }
        })

        provider.performAction(nodeId,
                AccessibilityNodeInfo.ACTION_NEXT_AT_MOVEMENT_GRANULARITY,
                moveByGranularityArguments(AccessibilityNodeInfo.MOVEMENT_GRANULARITY_WORD))
        waitUntilTextTraversed(0, 5) // "Lorem"

        provider.performAction(nodeId,
                AccessibilityNodeInfo.ACTION_NEXT_AT_MOVEMENT_GRANULARITY,
                moveByGranularityArguments(AccessibilityNodeInfo.MOVEMENT_GRANULARITY_WORD))
        waitUntilTextTraversed(6, 11) // "ipsum"

        provider.performAction(nodeId,
                AccessibilityNodeInfo.ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY,
                moveByGranularityArguments(AccessibilityNodeInfo.MOVEMENT_GRANULARITY_WORD))
        waitUntilTextTraversed(0, 5) // "Lorem"
    }

    @Test fun testMoveByLine() {
        var nodeId = AccessibilityNodeProvider.HOST_VIEW_ID
        sessionRule.session.loadTestPath(LOREM_IPSUM_HTML_PATH)
        waitForInitialFocus(true)

        provider.performAction(AccessibilityNodeProvider.HOST_VIEW_ID,
                AccessibilityNodeInfo.ACTION_ACCESSIBILITY_FOCUS, null)

        sessionRule.waitUntilCalled(object : EventDelegate {
            @AssertCalled(count = 1)
            override fun onAccessibilityFocused(event: AccessibilityEvent) {
                nodeId = getSourceId(event)
                val node = createNodeInfo(nodeId)
                assertThat("Accessibility focus on first paragraph", node.text as String, startsWith("Lorem ipsum"))
            }
        })

        provider.performAction(nodeId,
                AccessibilityNodeInfo.ACTION_NEXT_AT_MOVEMENT_GRANULARITY,
                moveByGranularityArguments(AccessibilityNodeInfo.MOVEMENT_GRANULARITY_LINE))
        waitUntilTextTraversed(0, 18) // "Lorem ipsum dolor "

        provider.performAction(nodeId,
                AccessibilityNodeInfo.ACTION_NEXT_AT_MOVEMENT_GRANULARITY,
                moveByGranularityArguments(AccessibilityNodeInfo.MOVEMENT_GRANULARITY_LINE))
        waitUntilTextTraversed(18, 28) // "sit amet, "

        provider.performAction(nodeId,
                AccessibilityNodeInfo.ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY,
                moveByGranularityArguments(AccessibilityNodeInfo.MOVEMENT_GRANULARITY_LINE))
        waitUntilTextTraversed(0, 18) // "Lorem ipsum dolor "
    }

    @Test fun testCheckbox() {
        var nodeId = AccessibilityNodeProvider.HOST_VIEW_ID;
        sessionRule.session.loadString("<label><input type='checkbox'>many option</label>", "text/html")
        waitForInitialFocus(true)

        sessionRule.waitUntilCalled(object : EventDelegate {
            @AssertCalled(count = 1)
            override fun onAccessibilityFocused(event: AccessibilityEvent) {
                nodeId = getSourceId(event)
                var node = createNodeInfo(nodeId)
                assertThat("Checkbox node is checkable", node.isCheckable, equalTo(true))
                assertThat("Checkbox node is clickable", node.isClickable, equalTo(true))
                assertThat("Checkbox node is focusable", node.isFocusable, equalTo(true))
                assertThat("Checkbox node is not checked", node.isChecked, equalTo(false))
                assertThat("Checkbox node has correct role", node.text.toString(), equalTo("many option"))
            }
        })

        provider.performAction(nodeId, AccessibilityNodeInfo.ACTION_CLICK, null)
        waitUntilClick(true)

        provider.performAction(nodeId, AccessibilityNodeInfo.ACTION_CLICK, null)
        waitUntilClick(false)
    }

    @Test fun testSelectable() {
        var nodeId = View.NO_ID
        sessionRule.session.loadString(
                """<ul style="list-style-type: none;" role="listbox">
                        <li id="li" role="option" onclick="this.setAttribute('aria-selected',
                            this.getAttribute('aria-selected') == 'true' ? 'false' : 'true')">1</li>
                        <li role="option" aria-selected="false">2</li>
                </ul>""","text/html")
        waitForInitialFocus(true)

        sessionRule.waitUntilCalled(object : EventDelegate {
            @AssertCalled(count = 1)
            override fun onAccessibilityFocused(event: AccessibilityEvent) {
                nodeId = getSourceId(event)
                var node = createNodeInfo(nodeId)
                assertThat("Selectable node is clickable", node.isClickable, equalTo(true))
                assertThat("Selectable node is not selected", node.isSelected, equalTo(false))
                assertThat("Selectable node has correct text", node.text.toString(), equalTo("1"))
            }
        })

        provider.performAction(nodeId, AccessibilityNodeInfo.ACTION_CLICK, null)
        waitUntilSelect(true)

        provider.performAction(nodeId, AccessibilityNodeInfo.ACTION_CLICK, null)
        waitUntilSelect(false)

        provider.performAction(nodeId, AccessibilityNodeInfo.ACTION_SELECT, null)
        waitUntilSelect(true)

        provider.performAction(nodeId, AccessibilityNodeInfo.ACTION_SELECT, null)
        waitUntilSelect(false)
    }

    private fun screenContainsNode(nodeId: Int): Boolean {
        var node = createNodeInfo(nodeId)
        var nodeBounds = Rect()
        node.getBoundsInScreen(nodeBounds)
        return screenRect.contains(nodeBounds)
    }

    @ReuseSession(false)
    @Test fun testScroll() {
        var nodeId = View.NO_ID
        sessionRule.session.loadString(
                """<body style="margin: 0;">
                        <div style="height: 100vh;"></div>
                        <button>Hello</button>
                        <p style="margin: 0;">Lorem ipsum dolor sit amet, consectetur adipiscing elit,
                            sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.</p>
                </body>""",
                "text/html")

        sessionRule.waitUntilCalled(object : EventDelegate {
            @AssertCalled
            override fun onWinStateChanged(event: AccessibilityEvent) { }

            @AssertCalled(count = 1)
            override fun onFocused(event: AccessibilityEvent) {
                nodeId = getSourceId(event)
                var node = createNodeInfo(nodeId)
                var nodeBounds = Rect()
                node.getBoundsInParent(nodeBounds)
                assertThat("Default root node bounds are correct", nodeBounds, equalTo(screenRect))
            }
        })

        provider.performAction(View.NO_ID, AccessibilityNodeInfo.ACTION_NEXT_HTML_ELEMENT, null)
        sessionRule.waitUntilCalled(object : EventDelegate {
            @AssertCalled(count = 1, order = [1])
            override fun onAccessibilityFocused(event: AccessibilityEvent) {
                nodeId = getSourceId(event)
                assertThat("Focused node is onscreen", screenContainsNode(nodeId), equalTo(true))
            }

            @AssertCalled(count = 1, order = [2])
            override fun onScrolled(event: AccessibilityEvent) {
                assertThat("View is scrolled for focused node to be onscreen", event.scrollY, greaterThan(0))
                assertThat("View is not scrolled to the end", event.scrollY, lessThan(event.maxScrollY))
            }

            @AssertCalled(count = 1, order = [3])
            override fun onWinContentChanged(event: AccessibilityEvent) {
                assertThat("Focused node is onscreen", screenContainsNode(nodeId), equalTo(true))
            }
        })

        SystemClock.sleep(100);
        provider.performAction(nodeId, AccessibilityNodeInfo.ACTION_SCROLL_FORWARD, null)
        sessionRule.waitUntilCalled(object : EventDelegate {
            @AssertCalled(count = 1, order = [1])
            override fun onScrolled(event: AccessibilityEvent) {
                assertThat("View is scrolled to the end", event.scrollY.toDouble(), closeTo(event.maxScrollY.toDouble(), 1.0))
            }

            @AssertCalled(count = 1, order = [2])
            override fun onWinContentChanged(event: AccessibilityEvent) {
                assertThat("Focused node is still onscreen", screenContainsNode(nodeId), equalTo(true))
            }
        })

        SystemClock.sleep(100)
        provider.performAction(nodeId, AccessibilityNodeInfo.ACTION_SCROLL_BACKWARD, null)
        sessionRule.waitUntilCalled(object : EventDelegate {
            @AssertCalled(count = 1, order = [1])
            override fun onScrolled(event: AccessibilityEvent) {
                assertThat("View is scrolled to the beginning", event.scrollY, equalTo(0))
            }

            @AssertCalled(count = 1, order = [2])
            override fun onWinContentChanged(event: AccessibilityEvent) {
                assertThat("Focused node is offscreen", screenContainsNode(nodeId), equalTo(false))
            }
        })

        SystemClock.sleep(100)
        provider.performAction(nodeId, AccessibilityNodeInfo.ACTION_NEXT_HTML_ELEMENT, null)
        sessionRule.waitUntilCalled(object : EventDelegate {
            @AssertCalled(count = 1, order = [1])
            override fun onAccessibilityFocused(event: AccessibilityEvent) {
                nodeId = getSourceId(event)
                assertThat("Focused node is onscreen", screenContainsNode(nodeId), equalTo(true))
            }

            @AssertCalled(count = 1, order = [2])
            override fun onScrolled(event: AccessibilityEvent) {
                assertThat("View is scrolled to the end", event.scrollY.toDouble(), closeTo(event.maxScrollY.toDouble(), 1.0))
            }

            @AssertCalled(count = 1, order = [3])
            override fun onWinContentChanged(event: AccessibilityEvent) {
                assertThat("Focused node is onscreen", screenContainsNode(nodeId), equalTo(true))
            }
        })
    }

    @Setting(key = Setting.Key.FULL_ACCESSIBILITY_TREE, value = "true")
    @Test fun autoFill() {
        // Wait for the accessibility nodes to populate.
        mainSession.loadTestPath(FORMS_HTML_PATH)
        waitForInitialFocus()

        val autoFills = mapOf(
                "#user1" to "bar", "#pass1" to "baz", "#user2" to "bar", "#pass2" to "baz") +
                if (Build.VERSION.SDK_INT >= 19) mapOf(
                        "#email1" to "a@b.c", "#number1" to "24", "#tel1" to "42")
                else mapOf(
                        "#email1" to "bar", "#number1" to "", "#tel1" to "bar")

        // Set up promises to monitor the values changing.
        val promises = autoFills.flatMap { entry ->
            // Repeat each test with both the top document and the iframe document.
            arrayOf("document", "$('#iframe').contentDocument").map { doc ->
                mainSession.evaluateJS("""new Promise(resolve =>
                    $doc.querySelector('${entry.key}').addEventListener(
                        'input', event => {
                          let eventInterface =
                            event instanceof InputEvent ? "InputEvent" :
                            event instanceof UIEvent ? "UIEvent" :
                            event instanceof Event ? "Event" : "Unknown";
                          resolve([event.target.value, '${entry.value}', eventInterface]);
                        }, { once: true }))""").asJSPromise()
            }
        }

        // Perform auto-fill and return number of auto-fills performed.
        fun autoFillChild(id: Int, child: AccessibilityNodeInfo) {
            // Seal the node info instance so we can perform actions on it.
            if (child.childCount > 0) {
                for (i in 0 until child.childCount) {
                    val childId = child.getChildId(i)
                    autoFillChild(childId, createNodeInfo(childId))
                }
            }

            if (EditText::class.java.name == child.className) {
                assertThat("Input should be enabled", child.isEnabled, equalTo(true))
                assertThat("Input should be focusable", child.isFocusable, equalTo(true))
                if (Build.VERSION.SDK_INT >= 19) {
                    assertThat("Password type should match", child.isPassword, equalTo(
                            child.inputType == InputType.TYPE_CLASS_TEXT or
                                    InputType.TYPE_TEXT_VARIATION_WEB_PASSWORD))
                }

                val args = Bundle(1)
                val value = if (child.isPassword) "baz" else
                    if (Build.VERSION.SDK_INT < 19) "bar" else
                        when (child.inputType) {
                            InputType.TYPE_CLASS_TEXT or
                                    InputType.TYPE_TEXT_VARIATION_WEB_EMAIL_ADDRESS -> "a@b.c"
                            InputType.TYPE_CLASS_NUMBER -> "24"
                            InputType.TYPE_CLASS_PHONE -> "42"
                            else -> "bar"
                        }

                val ACTION_ARGUMENT_SET_TEXT_CHARSEQUENCE = if (Build.VERSION.SDK_INT >= 21)
                    AccessibilityNodeInfo.ACTION_ARGUMENT_SET_TEXT_CHARSEQUENCE else
                    "ACTION_ARGUMENT_SET_TEXT_CHARSEQUENCE"
                val ACTION_SET_TEXT = if (Build.VERSION.SDK_INT >= 21)
                    AccessibilityNodeInfo.ACTION_SET_TEXT else 0x200000

                args.putCharSequence(ACTION_ARGUMENT_SET_TEXT_CHARSEQUENCE, value)
                assertThat("Can perform auto-fill",
                           provider.performAction(id, ACTION_SET_TEXT, args), equalTo(true))
            }
        }

        autoFillChild(View.NO_ID, createNodeInfo(View.NO_ID))

        // Wait on the promises and check for correct values.
        for ((actual, expected, eventInterface) in promises.map { it.value.asJSList<String>() }) {
            assertThat("Auto-filled value must match", actual, equalTo(expected))
            assertThat("input event should be dispatched with InputEvent interface", eventInterface, equalTo("InputEvent"))
        }
    }

    @Setting(key = Setting.Key.FULL_ACCESSIBILITY_TREE, value = "true")
    @Test fun autoFill_navigation() {
        fun countAutoFillNodes(cond: (AccessibilityNodeInfo) -> Boolean =
                                       { it.className == "android.widget.EditText" },
                               id: Int = View.NO_ID): Int {
            val info = createNodeInfo(id)
            return (if (cond(info) && info.className != "android.webkit.WebView" ) 1 else 0) + (if (info.childCount > 0)
                (0 until info.childCount).sumBy {
                    countAutoFillNodes(cond, info.getChildId(it))
                } else 0)
        }

        // Wait for the accessibility nodes to populate.
        mainSession.loadTestPath(FORMS_HTML_PATH)
        waitForInitialFocus()

        assertThat("Initial auto-fill count should match",
                   countAutoFillNodes(), equalTo(14))
        assertThat("Password auto-fill count should match",
                   countAutoFillNodes({ it.isPassword }), equalTo(4))

        // Now wait for the nodes to clear.
        mainSession.loadTestPath(HELLO_HTML_PATH)
        waitForInitialFocus()
        assertThat("Should not have auto-fill fields",
                   countAutoFillNodes(), equalTo(0))

        // Now wait for the nodes to reappear.
        mainSession.goBack()
        waitForInitialFocus()
        assertThat("Should have auto-fill fields again",
                   countAutoFillNodes(), equalTo(14))
        assertThat("Should not have focused field",
                   countAutoFillNodes({ it.isFocused }), equalTo(0))

        mainSession.evaluateJS("$('#pass1').focus()")
        sessionRule.waitUntilCalled(object : EventDelegate {
            @AssertCalled
            override fun onFocused(event: AccessibilityEvent) {
            }
        })
        assertThat("Should have one focused field",
                   countAutoFillNodes({ it.isFocused }), equalTo(1))

        mainSession.evaluateJS("$('#pass1').blur()")
        sessionRule.waitUntilCalled(object : EventDelegate {
            @AssertCalled
            override fun onFocused(event: AccessibilityEvent) {
            }
        })
        assertThat("Should not have focused field",
                   countAutoFillNodes({ it.isFocused }), equalTo(0))
    }

    @Setting(key = Setting.Key.FULL_ACCESSIBILITY_TREE, value = "true")
    @Test fun testTree() {
        sessionRule.session.loadString(
                "<label for='name'>Name:</label><input id='name' type='text' value='Julie'><button>Submit</button>",
                "text/html")
        waitForInitialFocus()

        val rootNode = createNodeInfo(View.NO_ID)
        assertThat("Document has 3 children", rootNode.childCount, equalTo(3))

        val labelNode = createNodeInfo(rootNode.getChildId(0))
        assertThat("First node is a label", labelNode.className.toString(), equalTo("android.view.View"))
        assertThat("Label has text", labelNode.text.toString(), equalTo("Name:"))

        val entryNode = createNodeInfo(rootNode.getChildId(1))
        assertThat("Second node is an entry", entryNode.className.toString(), equalTo("android.widget.EditText"))
        assertThat("Entry has vieIdwResourceName of 'name'", entryNode.viewIdResourceName, equalTo("name"))
        assertThat("Entry value is text", entryNode.text.toString(), equalTo("Julie"))
        if (Build.VERSION.SDK_INT >= 19) {
            assertThat("Entry hint is label",
                    entryNode.extras.getString("AccessibilityNodeInfo.hint"),
                    equalTo("Name:"))
            assertThat("Entry input type is correct", entryNode.inputType,
                    equalTo(InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_VARIATION_WEB_EDIT_TEXT))
        }


        val buttonNode = createNodeInfo(rootNode.getChildId(2))
        assertThat("Last node is a button", buttonNode.className.toString(), equalTo("android.widget.Button"))
        assertThat("Button has a single text leaf", buttonNode.childCount, equalTo(1))
        assertThat("Button has correct text", buttonNode.text.toString(), equalTo("Submit"))

        val textLeaf = createNodeInfo(buttonNode.getChildId(0))
        assertThat("First node is a label", textLeaf.className.toString(), equalTo("android.view.View"))
        assertThat("Text leaf has correct text", textLeaf.text.toString(), equalTo("Submit"))
    }

    @Setting(key = Setting.Key.FULL_ACCESSIBILITY_TREE, value = "true")
    @Test fun testCollection() {
        sessionRule.session.loadString(
                """<ul>
                  |  <li>One</li>
                  |  <li>Two</li>
                  |</ul>
                  |<ul>
                  |  <li>1<ul><li>1.1</li><li>1.2</li></ul></li>
                  |</ul>
                """.trimMargin(),
                "text/html")
        waitForInitialFocus()

        val rootNode = createNodeInfo(View.NO_ID)
        assertThat("Document has 2 children", rootNode.childCount, equalTo(2))

        val firstList = createNodeInfo(rootNode.getChildId(0))
        assertThat("First list has 2 children", firstList.childCount, equalTo(2))
        assertThat("List is a ListView", firstList.className.toString(), equalTo("android.widget.ListView"))
        if (Build.VERSION.SDK_INT >= 19) {
            assertThat("First list should have collectionInfo", firstList.collectionInfo, notNullValue())
            assertThat("First list has 2 rowCount", firstList.collectionInfo.rowCount, equalTo(2))
            assertThat("First list should not be hierarchical", firstList.collectionInfo.isHierarchical, equalTo(false))
        }

        val firstListFirstItem = createNodeInfo(firstList.getChildId(0))
        if (Build.VERSION.SDK_INT >= 19) {
            assertThat("Item has collectionItemInfo", firstListFirstItem.collectionItemInfo, notNullValue())
            assertThat("Item has collectionItemInfo", firstListFirstItem.collectionItemInfo.rowIndex, equalTo(1))
        }

        val secondList = createNodeInfo(rootNode.getChildId(1))
        assertThat("Second list has 1 child", secondList.childCount, equalTo(1))
        if (Build.VERSION.SDK_INT >= 19) {
            assertThat("Second list should have collectionInfo", secondList.collectionInfo, notNullValue())
            assertThat("Second list has 2 rowCount", secondList.collectionInfo.rowCount, equalTo(1))
            assertThat("Second list should be hierarchical", secondList.collectionInfo.isHierarchical, equalTo(true))
        }
    }

    @Setting(key = Setting.Key.FULL_ACCESSIBILITY_TREE, value = "true")
    @Test fun testRange() {
        sessionRule.session.loadString(
                """<input type="range" aria-label="Rating" min="1" max="10" value="4">
                  |<input type="range" aria-label="Stars" min="1" max="5" step="0.5" value="4.5">
                  |<input type="range" aria-label="Percent" min="0" max="1" step="0.01" value="0.83">
                """.trimMargin(),
                "text/html")
        waitForInitialFocus()

        val rootNode = createNodeInfo(View.NO_ID)
        assertThat("Document has 3 children", rootNode.childCount, equalTo(3))

        val firstRange = createNodeInfo(rootNode.getChildId(0))
        assertThat("Range has right label", firstRange.text.toString(), equalTo("Rating"))
        assertThat("Range is SeekBar", firstRange.className.toString(), equalTo("android.widget.SeekBar"))
        if (Build.VERSION.SDK_INT >= 19) {
            assertThat("'Rating' has rangeInfo", firstRange.rangeInfo, notNullValue())
            assertThat("'Rating' has correct value", firstRange.rangeInfo.current, equalTo(4f))
            assertThat("'Rating' has correct max", firstRange.rangeInfo.max, equalTo(10f))
            assertThat("'Rating' has correct min", firstRange.rangeInfo.min, equalTo(1f))
            assertThat("'Rating' has correct range type", firstRange.rangeInfo.type, equalTo(AccessibilityNodeInfo.RangeInfo.RANGE_TYPE_INT))
        }

        val secondRange = createNodeInfo(rootNode.getChildId(1))
        assertThat("Range has right label", secondRange.text.toString(), equalTo("Stars"))
        if (Build.VERSION.SDK_INT >= 19) {
            assertThat("'Rating' has rangeInfo", secondRange.rangeInfo, notNullValue())
            assertThat("'Rating' has correct value", secondRange.rangeInfo.current, equalTo(4.5f))
            assertThat("'Rating' has correct max", secondRange.rangeInfo.max, equalTo(5f))
            assertThat("'Rating' has correct min", secondRange.rangeInfo.min, equalTo(1f))
            assertThat("'Rating' has correct range type", secondRange.rangeInfo.type, equalTo(AccessibilityNodeInfo.RangeInfo.RANGE_TYPE_FLOAT))
        }

        val thirdRange = createNodeInfo(rootNode.getChildId(2))
        assertThat("Range has right label", thirdRange.text.toString(), equalTo("Percent"))
        if (Build.VERSION.SDK_INT >= 19) {
            assertThat("'Rating' has rangeInfo", thirdRange.rangeInfo, notNullValue())
            assertThat("'Rating' has correct value", thirdRange.rangeInfo.current, equalTo(0.83f))
            assertThat("'Rating' has correct max", thirdRange.rangeInfo.max, equalTo(1f))
            assertThat("'Rating' has correct min", thirdRange.rangeInfo.min, equalTo(0f))
            assertThat("'Rating' has correct range type", thirdRange.rangeInfo.type, equalTo(AccessibilityNodeInfo.RangeInfo.RANGE_TYPE_PERCENT))
        }
    }
}
