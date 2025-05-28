/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.efficiency.helpers

import android.util.Log
import androidx.compose.ui.test.SemanticsNodeInteraction
import androidx.compose.ui.test.assertIsDisplayed
import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import androidx.compose.ui.test.onNodeWithContentDescription
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performImeAction
import androidx.compose.ui.test.performTextInput
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.ViewInteraction
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.action.ViewActions.pressImeActionButton
import androidx.test.espresso.action.ViewActions.typeText
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withContentDescription
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiObject
import androidx.test.uiautomator.UiSelector
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.ui.efficiency.navigation.NavigationRegistry
import org.mozilla.fenix.ui.efficiency.navigation.NavigationStep

abstract class BasePage(
    protected val composeRule: AndroidComposeTestRule<HomeActivityIntentTestRule, *>,
) {
    abstract val pageName: String

    open fun navigateToPage(url: String = ""): BasePage {
        if (mozWaitForPageToLoad()) {
            PageStateTracker.currentPageName = pageName
            return this
        }

        val fromPage = PageStateTracker.currentPageName
        Log.i("PageNavigation", "🔍 Trying to find path from '$fromPage' to '$pageName'")

        val path = NavigationRegistry.findPath(fromPage, pageName)

        if (path == null) {
            NavigationRegistry.logGraph()
            throw AssertionError("❌ No navigation path found from '$fromPage' to '$pageName'")
        } else {
            Log.i("PageNavigation", "✅ Navigation path found from '$fromPage' to '$pageName':")
            path.forEachIndexed { i, step -> Log.i("PageNavigation", "   Step ${i + 1}: $step") }
        }

        path.forEach { step ->
            when (step) {
                is NavigationStep.Click -> mozClick(step.selector)
                is NavigationStep.Swipe -> mozSwipeTo(step.selector, step.direction)
                is NavigationStep.OpenNotificationsTray -> mozOpenNotificationsTray()
                is NavigationStep.EnterText -> mozEnterText(url, step.selector)
                is NavigationStep.PressEnter -> mozPressEnter(step.selector)
            }
        }

        if (!mozWaitForPageToLoad()) {
            throw AssertionError("Failed to navigate to $pageName")
        }

        PageStateTracker.currentPageName = pageName
        return this
    }

    private fun mozWaitForPageToLoad(timeout: Long = 10_000, interval: Long = 500): Boolean {
        val requiredSelectors = mozGetSelectorsByGroup("requiredForPage")
        val deadline = System.currentTimeMillis() + timeout

        while (System.currentTimeMillis() < deadline) {
            if (requiredSelectors.all { mozVerifyElement(it) }) {
                return true
            }
            android.os.SystemClock.sleep(interval)
        }

        return false
    }

    abstract fun mozGetSelectorsByGroup(group: String = "requiredForPage"): List<Selector>

    fun mozVerifyElementsByGroup(group: String = "requiredForPage"): BasePage {
        val selectors = mozGetSelectorsByGroup(group)
        val allPresent = selectors.all { mozVerifyElement(it) }

        if (!allPresent) {
            throw AssertionError("Not all elements in group '$group' are present")
        }

        return this
    }

    fun mozClick(selector: Selector): BasePage {
        val element = mozGetElement(selector)

        if (element == null) {
            throw AssertionError("Element not found for selector: ${selector.description} (${selector.strategy} -> ${selector.value})")
        }

        when (element) {
            is ViewInteraction -> {
                try {
                    element.perform(click())
                } catch (e: Exception) {
                    throw AssertionError("Failed to click on Espresso element for selector: ${selector.description}", e)
                }
            }

            is UiObject -> {
                if (!element.exists()) {
                    throw AssertionError("UiObject does not exist for selector: ${selector.description}")
                }
                if (!element.click()) {
                    throw AssertionError("Failed to click on UiObject for selector: ${selector.description}")
                }
            }

            is SemanticsNodeInteraction -> {
                try {
                    element.assertExists()
                    element.assertIsDisplayed()
                    element.performClick()
                } catch (e: Exception) {
                    throw AssertionError("Failed to click on Compose node for selector: ${selector.description}", e)
                }
            }

            else -> {
                throw AssertionError("Unsupported element type (${element::class.simpleName}) for selector: ${selector.description}")
            }
        }

        return this
    }

    fun mozSwipeTo(
        selector: Selector,
        direction: SwipeDirection = SwipeDirection.DOWN,
        maxSwipes: Int = 5,
        ): BasePage {
        repeat(maxSwipes) { attempt ->
            val element = mozGetElement(selector)

            val isVisible = when (element) {
                is ViewInteraction -> try {
                    element.check(matches(isDisplayed()))
                    true
                } catch (_: Exception) {
                    false
                }

                is UiObject -> element.exists()

                is SemanticsNodeInteraction -> try {
                    element.assertExists()
                    element.assertIsDisplayed()
                    true
                } catch (_: AssertionError) {
                    false
                }

                else -> false
            }

            if (isVisible) {
                Log.i("MozSwipeTo", "✅ Element '${selector.description}' found after $attempt swipe(s)")
                return this
            }

            Log.i("MozSwipeTo", "🔄 Swipe attempt ${attempt + 1} for selector '${selector.description}'")
            performSwipe(direction)
            Thread.sleep(500)
        }

        throw AssertionError("❌ Element '${selector.description}' not found after $maxSwipes swipe(s)")
    }

    fun mozOpenNotificationsTray(): BasePage {
        mDevice.openNotification()

        return this
    }

    private fun performSwipe(direction: SwipeDirection) {
        val height = mDevice.displayHeight
        val width = mDevice.displayWidth

        val (startX, startY, endX, endY) = when (direction) {
            SwipeDirection.UP -> listOf(width / 2, height / 2, width / 2, height / 4)
            SwipeDirection.DOWN -> listOf(width / 2, height / 2, width / 2, height * 3 / 4)
            SwipeDirection.LEFT -> listOf(width * 3 / 4, height / 2, width / 4, height / 2)
            SwipeDirection.RIGHT -> listOf(width / 4, height / 2, width * 3 / 4, height / 2)
        }

        mDevice.swipe(startX, startY, endX, endY, 10)
    }

    fun mozEnterText(text: String, selector: Selector): BasePage {
        val element = mozGetElement(selector)
            ?: throw AssertionError("Element not found for selector: ${selector.description} (${selector.strategy} -> ${selector.value})")

        when (element) {
            is ViewInteraction -> {
                try {
                    element.perform(typeText(text))
                } catch (e: Exception) {
                    throw AssertionError("Failed to enter text on Espresso element for selector: ${selector.description}", e)
                }
            }

            is UiObject -> {
                try {
                    element.setText(text)
                } catch (e: Exception) {
                    throw AssertionError("Failed to enter text on UIObject element for selector: ${selector.description}", e)
                }
            }

            is SemanticsNodeInteraction -> {
                try {
                    element.performTextInput(text)
                } catch (e: Exception) {
                    throw AssertionError("Failed to enter text on Compose element for selector: ${selector.description}", e)
                }
            }

            else -> {
                throw AssertionError("Unsupported element type (${element::class.simpleName}) for selector: ${selector.description}")
            }
        }

        return this
    }

    fun mozPressEnter(selector: Selector): BasePage {
        val element = mozGetElement(selector)

        if (element == null) {
            throw AssertionError("Element not found for selector: ${selector.description} (${selector.strategy} -> ${selector.value})")
        }

        when (element) {
            is ViewInteraction -> {
                try {
                    element.perform(pressImeActionButton())
                } catch (e: Exception) {
                    throw AssertionError("Failed to press IMEActionButton on Espresso element for selector: ${selector.description}", e)
                }
            }

            is UiObject -> {
                try {
                    mDevice.pressEnter()
                } catch (e: Exception) {
                    throw AssertionError("Failed to press Enter on UIObject element for selector: ${selector.description}", e)
                }
            }

            is SemanticsNodeInteraction -> {
                try {
                    element.performImeAction()
                } catch (e: Exception) {
                    throw AssertionError("Failed to press IMEActionButton on Compose element for selector: ${selector.description}", e)
                }
            }

            else -> {
                throw AssertionError("Unsupported element type (${element::class.simpleName}) for selector: ${selector.description}")
            }
        }

        return this
    }

    private fun mozGetElement(selector: Selector): Any? {
        if (selector.value.isBlank()) {
            Log.i("mozGetElement", "Empty or blank selector value: ${selector.description}")
            return null
        }

        return when (selector.strategy) {
            SelectorStrategy.COMPOSE_BY_TAG -> {
                try {
                    composeRule.onNodeWithTag(selector.value)
                } catch (e: Exception) {
                    Log.i("mozGetElement", "Compose node not found for tag: ${selector.value}")
                    null
                }
            }

            SelectorStrategy.COMPOSE_BY_TEXT -> {
                try {
                    composeRule.onNodeWithText(selector.value, useUnmergedTree = true)
                } catch (e: Exception) {
                    Log.i("mozGetElement", "Compose node not found for text: ${selector.value}")
                    null
                }
            }

            SelectorStrategy.COMPOSE_BY_CONTENT_DESCRIPTION -> {
                try {
                    composeRule.onNodeWithContentDescription(selector.value)
                } catch (e: Exception) {
                    Log.i("mozGetElement", "Compose node not found for content description: ${selector.value}")
                    null
                }
            }

            SelectorStrategy.ESPRESSO_BY_ID -> {
                val resId = selector.toResourceId()
                if (resId == 0) {
                    Log.i("mozGetElement", "Invalid resource ID for: ${selector.value}")
                    null
                } else {
                    onView(withId(resId))
                }
            }

            SelectorStrategy.ESPRESSO_BY_TEXT -> onView(withText(selector.value))
            SelectorStrategy.ESPRESSO_BY_CONTENT_DESC -> onView(withContentDescription(selector.value))

            SelectorStrategy.UIAUTOMATOR2_BY_CLASS -> {
                val obj = mDevice.findObject(UiSelector().className(selector.value))
                if (!obj.exists()) null else obj
            }

            SelectorStrategy.UIAUTOMATOR2_BY_TEXT -> {
                val obj = mDevice.findObject(UiSelector().text(selector.value))
                if (!obj.exists()) null else obj
            }

            SelectorStrategy.UIAUTOMATOR2_BY_RES -> {
                val obj = mDevice.findObject(By.res(selector.value))
                if (obj == null) {
                    Log.i("MozGetElement", "mozGetElement: UIObject2 not found for res: ${selector.value}")
                    null
                } else { obj }
            }

            SelectorStrategy.UIAUTOMATOR_WITH_RES_ID -> {
                val obj = mDevice.findObject(UiSelector().resourceId(packageName + ":id/" + selector.value))
                if (!obj.exists()) null else obj
            }

            SelectorStrategy.UIAUTOMATOR_WITH_TEXT -> {
                val obj = mDevice.findObject(UiSelector().text(selector.value))
                if (!obj.exists()) null else obj
            }

            SelectorStrategy.UIAUTOMATOR_WITH_TEXT_CONTAINS -> {
                val obj = mDevice.findObject(UiSelector().textContains(selector.value))
                if (!obj.exists()) null else obj
            }

            SelectorStrategy.UIAUTOMATOR_WITH_DESCRIPTION_CONTAINS -> {
                val obj = mDevice.findObject(UiSelector().descriptionContains(selector.value))
                if (!obj.exists()) null else obj
            }
        }
    }

    private fun mozVerifyElement(selector: Selector): Boolean {
        val element = mozGetElement(selector)

        return when (element) {
            is ViewInteraction -> {
                try {
                    element.check(matches(isDisplayed()))
                    true
                } catch (e: Exception) {
                    false
                }
            }
            is UiObject -> element.exists()
            is SemanticsNodeInteraction -> {
                try {
                    element.assertExists()
                    element.assertIsDisplayed()
                    true
                } catch (e: AssertionError) {
                    false
                }
            }
            else -> false
        }
    }
}
