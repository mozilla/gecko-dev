/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.accounts

import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.distinctUntilChangedBy
import kotlinx.coroutines.flow.mapNotNull
import kotlinx.coroutines.launch
import mozilla.appservices.fxaclient.contentUrl
import mozilla.appservices.fxaclient.isCustom
import mozilla.components.browser.state.selector.findCustomTabOrSelectedTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.webextension.MessageHandler
import mozilla.components.concept.engine.webextension.Port
import mozilla.components.concept.engine.webextension.WebExtensionRuntime
import mozilla.components.concept.sync.AuthType
import mozilla.components.concept.sync.UserData
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.service.fxa.FxaAuthData
import mozilla.components.service.fxa.ServerConfig
import mozilla.components.service.fxa.SyncEngine
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.service.fxa.sync.toSyncEngines
import mozilla.components.service.fxa.toAuthType
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.kotlin.isSameOriginAs
import mozilla.components.support.webextensions.BuiltInWebExtensionController
import org.json.JSONArray
import org.json.JSONException
import org.json.JSONObject
import java.net.URL

/**
 * Configurable FxA capabilities.
 */
enum class FxaCapability {
    // Enables "choose what to sync" selection during support auth flows (currently, sign-up).
    CHOOSE_WHAT_TO_SYNC,
}

/**
 * Feature implementation that provides Firefox Accounts WebChannel support.
 * For more information https://github.com/mozilla/fxa/blob/master/packages/fxa-content-server/docs/relier-communication-protocols/fx-webchannel.md
 * This feature uses a web extension to communicate with FxA Web Content.
 *
 * @property customTabSessionId optional custom tab session ID, if feature is being used with a custom tab.
 * @property runtime the [WebExtensionRuntime] (e.g the browser engine) to use.
 * @property store a reference to the application's [BrowserStore].
 * @property accountManager a reference to application's [FxaAccountManager].
 * @property fxaCapabilities a set of [FxaCapability] that client supports.
 * @property onCommandExecuted an optional callback to know when a command has been executed.
 */
class FxaWebChannelFeature(
    private val customTabSessionId: String?,
    private val runtime: WebExtensionRuntime,
    private val store: BrowserStore,
    private val accountManager: FxaAccountManager,
    private val serverConfig: ServerConfig,
    private val fxaCapabilities: Set<FxaCapability> = emptySet(),
    private val onCommandExecuted: (WebChannelCommand) -> Unit = {},
) : LifecycleAwareFeature {

    private var scope: CoroutineScope? = null

    @VisibleForTesting
    // This is an internal var to make it mutable for unit testing purposes only
    internal var extensionController = BuiltInWebExtensionController(
        WEB_CHANNEL_EXTENSION_ID,
        WEB_CHANNEL_EXTENSION_URL,
        WEB_CHANNEL_MESSAGING_ID,
    )

    override fun start() {
        val messageHandler = WebChannelViewBackgroundMessageHandler(serverConfig)
        extensionController.registerBackgroundMessageHandler(messageHandler, WEB_CHANNEL_BACKGROUND_MESSAGING_ID)

        extensionController.install(runtime)

        scope = store.flowScoped { flow ->
            flow.mapNotNull { state -> state.findCustomTabOrSelectedTab(customTabSessionId) }
                .distinctUntilChangedBy { it.engineState.engineSession }
                .collect {
                    it.engineState.engineSession?.let { engineSession ->
                        registerFxaContentMessageHandler(engineSession)
                    }
                }
        }
    }

    override fun stop() {
        scope?.cancel()
    }

    @Suppress("MaxLineLength", "")
    /**
     * Communication channel is established from fxa-web-content to this class via webextension, as follows:
     * [fxa-web-content] <--js events--> [fxawebchannel.js webextension] <--port messages--> [FxaWebChannelFeature]
     *
     * Overall message flow, as implemented by this class, is documented below. For detailed message descriptions, see:
     * https://github.com/mozilla/fxa/blob/master/packages/fxa-content-server/docs/relier-communication-protocols/fx-webchannel.md
     *
     * ```
     * [fxa-web-channel]            [FxaWebChannelFeature]         Notes:
     *     loaded           ------>          |                  fxa web content loaded
     *     fxa-status       ------>          |                  web content requests account status & device capabilities
     *        |             <------ fxa-status-response         this class responds, based on state of [accountManager]
     *     can-link-account ------>          |                  user submitted credentials, web content verifying if account linking is allowed
     *        |             <------ can-link-account-response   this class responds, based on state of [accountManager]
     *     oauth-login      ------>                             authentication completed within fxa web content, this class receives OAuth code & state
     * ```
     */
    private class WebChannelViewContentMessageHandler(
        private val accountManager: FxaAccountManager,
        private val serverConfig: ServerConfig,
        private val fxaCapabilities: Set<FxaCapability>,
        private val onCommandExecuted: (WebChannelCommand) -> Unit,
    ) : MessageHandler {
        @SuppressWarnings("ComplexMethod")
        override fun onPortMessage(message: Any, port: Port) {
            if (!isCommunicationAllowed(serverConfig, port)) {
                logger.error("Communication disallowed, ignoring WebChannel message.")
                return
            }

            val json = try {
                message as JSONObject
            } catch (e: ClassCastException) {
                logger.error("Received an invalid WebChannel message of type: ${message.javaClass}")
                // TODO ideally, this should log to Sentry
                return
            }

            val payload: JSONObject
            val command: WebChannelCommand?
            val rawCommand: String?
            val messageId: String

            try {
                payload = json.getJSONObject("message")
                rawCommand = payload.getString("command")
                command = rawCommand.toWebChannelCommand()
                messageId = payload.optString("messageId", "")
            } catch (e: JSONException) {
                // We don't have control over what messages we will get from the webchannel.
                // If somehow we're receiving mis-constructed messages, it's probably best to not
                // blow up the host application. This comes at a cost: we might not catch problems
                // as quickly if we're not crashing (and thus receiving crash logs).
                // TODO ideally, this should log to Sentry.
                logger.error("Error while processing WebChannel command", e)
                return
            }

            logger.debug("Processing WebChannel command: $rawCommand")

            val response = when (command) {
                WebChannelCommand.CAN_LINK_ACCOUNT -> processCanLinkAccountCommand(messageId)
                WebChannelCommand.FXA_STATUS -> processFxaStatusCommand(accountManager, messageId, fxaCapabilities)
                WebChannelCommand.OAUTH_LOGIN -> processOauthLoginCommand(accountManager, payload)
                WebChannelCommand.LOGIN -> processLoginCommand(accountManager, payload)
                WebChannelCommand.SYNC_PREFERENCES -> processSyncPreferencesCommand(accountManager)
                WebChannelCommand.LOGOUT, WebChannelCommand.DELETE_ACCOUNT -> processLogoutCommand(accountManager)
                else -> processUnknownCommand(rawCommand)
            }
            response?.let { port.postMessage(it) }

            // Finally, let any consumer be aware of the action to update any UI affordances
            // for ONLY processed messages since we can receive unsupported ones too and we
            // shouldn't forward that onwards.
            command?.let {
                onCommandExecuted(command)
            }
        }
    }

    private fun registerFxaContentMessageHandler(engineSession: EngineSession) {
        val messageHandler = WebChannelViewContentMessageHandler(
            accountManager,
            serverConfig,
            fxaCapabilities,
            onCommandExecuted,
        )
        extensionController.registerContentMessageHandler(engineSession, messageHandler)
    }

    private class WebChannelViewBackgroundMessageHandler(
        private val serverConfig: ServerConfig,
    ) : MessageHandler {
        override fun onPortConnected(port: Port) {
            if (serverConfig.server.isCustom()) {
                port.postMessage(
                    JSONObject()
                        .put("type", "overrideFxAServer")
                        .put("url", serverConfig.server.contentUrl()),
                )
            }
        }
    }

    companion object {
        private val logger = Logger("mozac-fxawebchannel")

        @VisibleForTesting
        internal const val WEB_CHANNEL_EXTENSION_ID = "fxa@mozac.org"

        @VisibleForTesting
        internal const val WEB_CHANNEL_MESSAGING_ID = "mozacWebchannel"

        @VisibleForTesting
        internal const val WEB_CHANNEL_BACKGROUND_MESSAGING_ID = "mozacWebchannelBackground"

        @VisibleForTesting
        internal const val WEB_CHANNEL_EXTENSION_URL = "resource://android/assets/extensions/fxawebchannel/"

        // Constants for incoming messages from the WebExtension.
        private const val CHANNEL_ID = "account_updates"

        enum class WebChannelCommand {
            CAN_LINK_ACCOUNT,
            LOGIN,
            OAUTH_LOGIN,
            SYNC_PREFERENCES,
            FXA_STATUS,
            LOGOUT,
            DELETE_ACCOUNT,
        }

        // For all possible messages and their meaning/payloads, see:
        // https://github.com/mozilla/fxa/blob/master/packages/fxa-content-server/docs/relier-communication-protocols/fx-webchannel.md

        /**
         * Gets triggered when user initiates a login within FxA web content.
         * Expects a response.
         * On Fx Desktop, this event triggers "a different user was previously signed in on this machine" warning.
         */
        private const val COMMAND_CAN_LINK_ACCOUNT = "fxaccounts:can_link_account"

        /**
         * Gets triggered when a user successfully authenticates via OAuth.
         */
        private const val COMMAND_OAUTH_LOGIN = "fxaccounts:oauth_login"

        /**
         * Gets triggered on startup to fetch the FxA state from the host application.
         * Expects a response, which includes application's capabilities and a description of the
         * current Firefox Account (if present).
         */
        private const val COMMAND_STATUS = "fxaccounts:fxa_status"

        /**
         * Gets triggered when the web content is signed in/up, but not necessarily verified
         * it passes in its payload the session token the web content is holding on to
         */
        private const val COMMAND_LOGIN = "fxaccounts:login"

        /**
         * Gets triggered when the web content signals to open sync preferences,
         * typically right after a sign-in/sign-up.
         */
        private const val COMMAND_SYNC_PREFERENCES = "fxaccounts:sync_preferences"

        /**
         * Triggered when web content logs out of the account.
         */
        private const val COMMAND_LOGOUT = "fxaccounts:logout"

        /**
         * Triggered when web content notifies a delete account request.
         */
        private const val COMMAND_DELETE_ACCOUNT = "fxaccounts:delete"

        /**
         * Handles the [COMMAND_CAN_LINK_ACCOUNT] event from the web-channel.
         * Currently this always response with 'ok=true'.
         * On Fx Desktop, this event prompts a possible "another user was previously logged in on
         * this device" warning. Currently we don't support propagating this warning to a consuming application.
         */
        private fun processCanLinkAccountCommand(messageId: String): JSONObject {
            // TODO don't allow linking if we're logged in already? This is requested after user
            // entered their credentials.
            return JSONObject().also { status ->
                status.put("id", CHANNEL_ID)
                status.put(
                    "message",
                    JSONObject().also { message ->
                        message.put("messageId", messageId)
                        message.put("command", COMMAND_CAN_LINK_ACCOUNT)
                        message.put(
                            "data",
                            JSONObject().also { data ->
                                data.put("ok", true)
                            },
                        )
                    },
                )
            }
        }

        /**
         * Handles the [COMMAND_STATUS] event from the web-channel.
         * Responds with supported application capabilities and information about currently signed-in Firefox Account.
         */
        @Suppress("ComplexMethod")
        private fun processFxaStatusCommand(
            accountManager: FxaAccountManager,
            messageId: String,
            fxaCapabilities: Set<FxaCapability>,
        ): JSONObject {
            val status = JSONObject()
            status.put("id", CHANNEL_ID)
            status.put(
                "message",
                JSONObject().also { message ->
                    message.put("messageId", messageId)
                    message.put("command", COMMAND_STATUS)
                    message.put(
                        "data",
                        JSONObject().also { data ->
                            data.put(
                                "capabilities",
                                JSONObject().also { capabilities ->
                                    capabilities.put(
                                        "engines",
                                        JSONArray().also { engines ->
                                            accountManager.supportedSyncEngines()?.forEach { engine ->
                                                engines.put(engine.nativeName)
                                            } ?: emptyArray<SyncEngine>()
                                        },
                                    )

                                    if (fxaCapabilities.contains(FxaCapability.CHOOSE_WHAT_TO_SYNC)) {
                                        capabilities.put("choose_what_to_sync", true)
                                    }
                                },
                            )
                            val account = accountManager.authenticatedAccount()
                            if (account == null) {
                                data.put("signedInUser", JSONObject.NULL)
                            } else {
                                data.put(
                                    "signedInUser",
                                    JSONObject().also { signedInUser ->
                                        signedInUser.put(
                                            "email",
                                            accountManager.accountProfile()?.email ?: JSONObject.NULL,
                                        )
                                        signedInUser.put(
                                            "uid",
                                            accountManager.accountProfile()?.uid ?: JSONObject.NULL,
                                        )
                                        signedInUser.put(
                                            "sessionToken",
                                            account.getSessionToken() ?: JSONObject.NULL,
                                        )
                                        // Our account state machine only ever completes authentication for
                                        // "verified" accounts, so this is always 'true'.
                                        signedInUser.put(
                                            "verified",
                                            true,
                                        )
                                    },
                                )
                            }
                        },
                    )
                },
            )
            return status
        }

        private fun JSONArray.toStringList(): List<String> {
            val result = mutableListOf<String>()
            for (i in 0 until this.length()) {
                this.optString(i, null)?.let { result.add(it) }
            }
            return result
        }

        /**
         * Handles the [COMMAND_LOGIN] event from the web-channel
         */
        private fun processLoginCommand(accountManager: FxaAccountManager, payload: JSONObject): JSONObject? {
            val sessionToken: String
            val email: String
            val uid: String
            val verified: Boolean

            try {
                val data = payload.getJSONObject("data")
                sessionToken = data.getString("sessionToken")
                email = data.getString("email")
                uid = data.getString("uid")
                verified = data.getBoolean("verified")
            } catch (e: JSONException) {
                logger.error("Error while processing WebChannel login command", e)
                return null
            }
            val userData = UserData(sessionToken, email, uid, verified)
            CoroutineScope(Dispatchers.Main).launch {
                accountManager.setUserData(userData)
            }
            return null
        }

        /**
         * Handles the [COMMAND_OAUTH_LOGIN] event from the web-channel.
         */
        private fun processOauthLoginCommand(accountManager: FxaAccountManager, payload: JSONObject): JSONObject? {
            val authType: AuthType
            val code: String
            val state: String
            val declinedEngines: List<String>?

            try {
                val data = payload.getJSONObject("data")
                authType = data.getString("action").toAuthType()
                code = data.getString("code")
                state = data.getString("state")
                declinedEngines = data.optJSONArray("declinedSyncEngines")?.toStringList()
            } catch (e: JSONException) {
                // TODO ideally, this should log to Sentry.
                logger.error("Error while processing WebChannel oauth-login command", e)
                return null
            }

            CoroutineScope(Dispatchers.Main).launch {
                accountManager.finishAuthentication(
                    FxaAuthData(
                        authType = authType,
                        code = code,
                        state = state,
                        declinedEngines = declinedEngines?.toSyncEngines(),
                    ),
                )
            }

            return null
        }

        /**
         * Handles unknown message types by responding with a `data` json that only contains an
         * `error` error message.
         */
        private fun processUnknownCommand(command: String): JSONObject {
            return JSONObject().apply {
                put("id", CHANNEL_ID)
                put(
                    "message",
                    JSONObject().apply {
                        put(
                            "data",
                            JSONObject().put(
                                "error",
                                "Unrecognized FxAccountsWebChannel command: $command",
                            ),
                        )
                    },
                )
            }
        }

        /**
         * Handles the [COMMAND_SYNC_PREFERENCES] event from the web-channel. The UI affordances
         * will be handled by [FxaWebChannelFeature.onCommandExecuted].
         */
        private fun processSyncPreferencesCommand(accountManager: FxaAccountManager): JSONObject {
            // If we don't have an authenticated account, we should let FxA know that we couldn't
            // handle the message.
            val canHandle = accountManager.authenticatedAccount() != null
            return JSONObject().apply {
                put("id", CHANNEL_ID)
                put(
                    "message",
                    JSONObject().apply {
                        put(
                            "data",
                            JSONObject().put("ok", canHandle),
                        )
                    },
                )
            }
        }

        /**
         * Handles the [COMMAND_LOGOUT] and [COMMAND_DELETE_ACCOUNT] event from the web-channel.
         */
        private fun processLogoutCommand(accountManager: FxaAccountManager): JSONObject? {
            CoroutineScope(Dispatchers.Main).launch {
                accountManager.logout()
            }
            return null
        }

        private fun String.toWebChannelCommand(): WebChannelCommand? {
            return when (this) {
                COMMAND_CAN_LINK_ACCOUNT -> WebChannelCommand.CAN_LINK_ACCOUNT
                COMMAND_OAUTH_LOGIN -> WebChannelCommand.OAUTH_LOGIN
                COMMAND_STATUS -> WebChannelCommand.FXA_STATUS
                COMMAND_LOGIN -> WebChannelCommand.LOGIN
                COMMAND_SYNC_PREFERENCES -> WebChannelCommand.SYNC_PREFERENCES
                COMMAND_LOGOUT -> WebChannelCommand.LOGOUT
                COMMAND_DELETE_ACCOUNT -> WebChannelCommand.DELETE_ACCOUNT
                else -> {
                    logger.warn("Unrecognized FxAccountsWebChannel command: $this")
                    null
                }
            }
        }

        private fun isCommunicationAllowed(serverConfig: ServerConfig, port: Port): Boolean {
            val senderOrigin = port.senderUrl()
            val expectedOrigin = serverConfig.server.contentUrl()
            return isCommunicationAllowed(senderOrigin, expectedOrigin)
        }

        @VisibleForTesting
        internal fun isCommunicationAllowed(senderOrigin: String, expectedOrigin: String): Boolean {
            if (!isSafeUrl(senderOrigin)) {
                logger.error("$senderOrigin looks unsafe, aborting.")
                return false
            }

            if (!senderOrigin.isSameOriginAs(expectedOrigin)) {
                logger.error("Host mismatch for WebChannel message. Expected: $expectedOrigin, got: $senderOrigin.")
                return false
            }
            return true
        }

        /**
         * Rejects URLs that are deemed "unsafe" (not expected).
         */
        private fun isSafeUrl(urlStr: String): Boolean {
            val url = URL(urlStr)
            return url.userInfo.isNullOrEmpty() &&
                (url.protocol == "https" || url.host == "localhost" || url.host == "127.0.0.1")
        }
    }
}
