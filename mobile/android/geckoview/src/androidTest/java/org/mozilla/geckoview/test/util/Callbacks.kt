/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview.test.util

import org.mozilla.geckoview.AllowOrDeny
import org.mozilla.geckoview.GeckoResponse
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.GeckoSession.ContentDelegate.ContextElement
import org.mozilla.geckoview.GeckoSession.NavigationDelegate.LoadRequest
import org.mozilla.geckoview.MediaElement
import org.mozilla.geckoview.WebRequestError

import android.view.inputmethod.CursorAnchorInfo
import android.view.inputmethod.ExtractedText
import android.view.inputmethod.ExtractedTextRequest

class Callbacks private constructor() {
    object Default : All

    interface All : ContentDelegate, HistoryDelegate, MediaDelegate,
                    NavigationDelegate, PermissionDelegate, ProgressDelegate,
                    PromptDelegate, ScrollDelegate, SelectionActionDelegate,
                    TextInputDelegate, TrackingProtectionDelegate

    interface ContentDelegate : GeckoSession.ContentDelegate {
        override fun onTitleChange(session: GeckoSession, title: String) {
        }

        override fun onFocusRequest(session: GeckoSession) {
        }

        override fun onCloseRequest(session: GeckoSession) {
        }

        override fun onFullScreen(session: GeckoSession, fullScreen: Boolean) {
        }

        override fun onContextMenu(session: GeckoSession,
                                   screenX: Int, screenY: Int,
                                   element: ContextElement) {
        }

        override fun onExternalResponse(session: GeckoSession, response: GeckoSession.WebResponseInfo) {
        }

        override fun onCrash(session: GeckoSession) {
        }

        override fun onFirstComposite(session: GeckoSession) {
        }
    }

    interface NavigationDelegate : GeckoSession.NavigationDelegate {
        override fun onLocationChange(session: GeckoSession, url: String) {
        }

        override fun onCanGoBack(session: GeckoSession, canGoBack: Boolean) {
        }

        override fun onCanGoForward(session: GeckoSession, canGoForward: Boolean) {
        }

        override fun onLoadRequest(session: GeckoSession,
                                   request: LoadRequest): GeckoResult<AllowOrDeny>? {
            return null
        }

        override fun onNewSession(session: GeckoSession, uri: String): GeckoResult<GeckoSession>? {
            return null
        }

        override fun onLoadError(session: GeckoSession, uri: String?,
                                 error: WebRequestError): GeckoResult<String>? {
            return null
        }
    }

    interface PermissionDelegate : GeckoSession.PermissionDelegate {
        override fun onAndroidPermissionsRequest(session: GeckoSession, permissions: Array<out String>, callback: GeckoSession.PermissionDelegate.Callback) {
            callback.reject()
        }

        override fun onContentPermissionRequest(session: GeckoSession, uri: String, type: Int, callback: GeckoSession.PermissionDelegate.Callback) {
            callback.reject()
        }

        override fun onMediaPermissionRequest(session: GeckoSession, uri: String, video: Array<out GeckoSession.PermissionDelegate.MediaSource>, audio: Array<out GeckoSession.PermissionDelegate.MediaSource>, callback: GeckoSession.PermissionDelegate.MediaCallback) {
            callback.reject()
        }
    }

    interface ProgressDelegate : GeckoSession.ProgressDelegate {
        override fun onPageStart(session: GeckoSession, url: String) {
        }

        override fun onPageStop(session: GeckoSession, success: Boolean) {
        }

        override fun onProgressChange(session: GeckoSession, progress: Int) {
        }

        override fun onSecurityChange(session: GeckoSession, securityInfo: GeckoSession.ProgressDelegate.SecurityInformation) {
        }
    }

    interface PromptDelegate : GeckoSession.PromptDelegate {
        override fun onAlert(session: GeckoSession, title: String, msg: String, callback: GeckoSession.PromptDelegate.AlertCallback) {
            callback.dismiss()
        }

        override fun onButtonPrompt(session: GeckoSession, title: String, msg: String, btnMsg: Array<out String>, callback: GeckoSession.PromptDelegate.ButtonCallback) {
            callback.dismiss()
        }

        override fun onTextPrompt(session: GeckoSession, title: String, msg: String, value: String, callback: GeckoSession.PromptDelegate.TextCallback) {
            callback.dismiss()
        }

        override fun onAuthPrompt(session: GeckoSession, title: String, msg: String, options: GeckoSession.PromptDelegate.AuthOptions, callback: GeckoSession.PromptDelegate.AuthCallback) {
            callback.dismiss()
        }

        override fun onChoicePrompt(session: GeckoSession, title: String, msg: String, type: Int, choices: Array<out GeckoSession.PromptDelegate.Choice>, callback: GeckoSession.PromptDelegate.ChoiceCallback) {
            callback.dismiss()
        }

        override fun onColorPrompt(session: GeckoSession, title: String, value: String, callback: GeckoSession.PromptDelegate.TextCallback) {
            callback.dismiss()
        }

        override fun onDateTimePrompt(session: GeckoSession, title: String, type: Int, value: String, min: String, max: String, callback: GeckoSession.PromptDelegate.TextCallback) {
            callback.dismiss()
        }

        override fun onFilePrompt(session: GeckoSession, title: String, type: Int, mimeTypes: Array<out String>, callback: GeckoSession.PromptDelegate.FileCallback) {
            callback.dismiss()
        }

        override fun onPopupRequest(session: GeckoSession, targetUri: String): GeckoResult<AllowOrDeny>? {
            return null
        }
    }

    interface ScrollDelegate : GeckoSession.ScrollDelegate {
        override fun onScrollChanged(session: GeckoSession, scrollX: Int, scrollY: Int) {
        }
    }

    interface TrackingProtectionDelegate : GeckoSession.TrackingProtectionDelegate {
        override fun onTrackerBlocked(session: GeckoSession, uri: String, categories: Int) {
        }
    }

    interface SelectionActionDelegate : GeckoSession.SelectionActionDelegate {
        override fun onShowActionRequest(session: GeckoSession, selection: GeckoSession.SelectionActionDelegate.Selection, actions: Array<out String>, response: GeckoResponse<String>) {
        }

        override fun onHideAction(session: GeckoSession, reason: Int) {
        }
    }

    interface TextInputDelegate : GeckoSession.TextInputDelegate {
        override fun restartInput(session: GeckoSession, reason: Int) {
        }

        override fun showSoftInput(session: GeckoSession) {
        }

        override fun hideSoftInput(session: GeckoSession) {
        }

        override fun updateSelection(session: GeckoSession, selStart: Int, selEnd: Int, compositionStart: Int, compositionEnd: Int) {
        }

        override fun updateExtractedText(session: GeckoSession, request: ExtractedTextRequest, text: ExtractedText) {
        }

        override fun updateCursorAnchorInfo(session: GeckoSession, info: CursorAnchorInfo) {
        }

        override fun notifyAutoFill(session: GeckoSession, notification: Int, virtualId: Int) {
        }
    }

    interface MediaDelegate: GeckoSession.MediaDelegate {
        override fun onMediaAdd(session: GeckoSession, element: MediaElement) {
        }

        override fun onMediaRemove(session: GeckoSession, element: MediaElement) {
        }
    }

    interface HistoryDelegate : GeckoSession.HistoryDelegate {
        override fun onVisited(session: GeckoSession, url: String, lastVisitedURL: String?,
                               flags: Int): GeckoResult<Boolean>? {
            return null
        }

        override fun getVisited(session: GeckoSession,
                                urls: Array<out String>): GeckoResult<BooleanArray>? {
            return null
        }
    }
}
