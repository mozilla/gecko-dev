/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.certificate

import android.app.Activity
import android.security.KeyChain
import mozilla.components.concept.engine.prompt.PromptRequest.CertificateRequest
import mozilla.components.feature.prompts.PromptContainer

internal class CertificatePicker(
    private val container: PromptContainer,
) {
    fun handleCertificateRequest(promptRequest: CertificateRequest) {
        // This API creates a dialog populated with any available client
        // authentication certificates and asks the user to select one.  It
        // returns to the callback the alias of the selected certificate, which
        // can later be used with `KeyChain.getCertificateChain` or
        // `KeyChain.getPrivateKey`. The user can opt to not select a
        // certificate, in which case `alias` will be null.
        // This code passes the value of `alias` back via the given prompt's
        // `onComplete` callback.
        KeyChain.choosePrivateKeyAlias(
            container.context as Activity,
            { alias -> promptRequest.onComplete(alias) },
            arrayOf("RSA", "EC"), // keyTypes
            null, // issuers - currently not supported
            promptRequest.host, // the host that requested the certificate
            -1, // specify the default port to simplify the UI
            null, // alias - leave null for now to not preselect a certificate
        )
    }
}
