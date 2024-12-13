# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

## Error messages for failed HTTP web requests.
## https://developer.mozilla.org/en-US/docs/Web/HTTP/Status#client_error_responses
## Variables:
##   $status (Number) - HTTP status code, for example 403

firefox-relay-mask-generation-failed = { -relay-brand-name } could not generate a new mask. HTTP error code: { $status }.
firefox-relay-get-reusable-masks-failed = { -relay-brand-name } could not find reusable masks. HTTP error code: { $status }.

##

firefox-relay-must-login-to-account = Sign in to your account to use your { -relay-brand-name } email masks.
firefox-relay-get-unlimited-masks =
    .label = Manage masks
    .accesskey = M
# This is followed, on a new line, by firefox-relay-opt-in-subtitle-1
firefox-relay-opt-in-title-1 = Protect your email address:
# This is preceded by firefox-relay-opt-in-title-1 (on a different line), which
# ends with a colon. You might need to adapt the capitalization of this string.
firefox-relay-opt-in-subtitle-1 = Use { -relay-brand-name } email mask
firefox-relay-use-mask-title = Use { -relay-brand-name } email mask
# This is followed, on a new line, by firefox-relay-opt-in-subtitle-a
firefox-relay-opt-in-title-a = Prevent spam with a free email mask
# This is preceded by firefox-relay-opt-in-title-a (on a different line)
firefox-relay-opt-in-subtitle-a = Hide your real email address
# This is followed, on a new line, by firefox-relay-opt-in-subtitle-b
firefox-relay-opt-in-title-b = Get a free email mask
# This is preceded by firefox-relay-opt-in-title-b (on a different line)
firefox-relay-opt-in-subtitle-b = Protect your inbox from spam
firefox-relay-opt-in-confirmation-enable-button =
    .label = Use email mask
    .accesskey = U
firefox-relay-opt-in-confirmation-disable =
    .label = Don’t show me this again
    .accesskey = D
firefox-relay-opt-in-confirmation-postpone =
    .label = Not now
    .accesskey = N

## The "control" variation of the Relay offer popup

firefox-relay-and-fxa-popup-notification-second-sentence-control = First, sign up or sign in to your account to use an email mask

firefox-relay-offer-legal-notice-control = By signing up and creating an email mask, you agree to the <label data-l10n-name="tos-url">Terms of Service</label> and <label data-l10n-name="privacy-url">Privacy Notice</label>.

firefox-relay-and-fxa-opt-in-confirmation-enable-button =
    .label = Sign in to { -brand-product-name } and use mask
    .accesskey = S
firefox-relay-and-fxa-opt-in-confirmation-enable-button-sign-up =
    .label = Sign up
    .accesskey = S
firefox-relay-and-fxa-opt-in-confirmation-disable =
    .label = Don’t show me this again
    .accesskey = D
firefox-relay-and-fxa-opt-in-confirmation-postpone =
    .label = Not now
    .accesskey = N

## The "basic-info" variation of the Relay offer popup

firefox-relay-and-fxa-popup-notification-header-basic-info = Prevent spam with a free email mask

firefox-relay-and-fxa-popup-notification-first-sentence-basic-info = Prevent spam by hiding your real email address with a free <label data-l10n-name="firefox-relay-learn-more-url">email mask</label>. Emails from <label data-l10n-name="firefox-fxa-and-relay-offer-domain">this site</label> will still come to your inbox, but with your email hidden.

firefox-relay-and-fxa-popup-notification-second-sentence-basic-info = First, sign up or sign in to your account to use an email mask

firefox-relay-and-fxa-opt-in-confirmation-enable-button-basic-info =
    .label = Sign up
    .accesskey = S

## The "with-domain" variation of the Relay offer popup

firefox-relay-and-fxa-popup-notification-header-with-domain = Get a free email mask

firefox-relay-and-fxa-popup-notification-first-sentence-with-domain  = Protect your inbox from spam by using a free <label data-l10n-name="firefox-relay-learn-more-url">email mask</label> to hide your real address. Emails from <label data-l10n-name="firefox-fxa-and-relay-offer-domain">this site</label> will still come to your inbox, but with your email hidden.

firefox-relay-and-fxa-popup-notification-second-sentence-with-domain = First, sign up or sign in to your account to use an email mask

firefox-relay-and-fxa-opt-in-confirmation-enable-button-with-domain =
    .label = Sign up
    .accesskey = S

## The "with-domain-and-value-prop" variation of the Relay offer popup

firefox-relay-and-fxa-popup-notification-header-with-domain-and-value-prop = Protect against spam with an email mask

firefox-relay-and-fxa-popup-notification-first-sentence-with-domain-and-value-prop  = Protect against spam by hiding your real address with an <label data-l10n-name="firefox-relay-learn-more-url">email mask</label>. You’ll still receive mails from <label data-l10n-name="firefox-fxa-and-relay-offer-domain">this site</label> in your regular inbox, with your address masked.

firefox-relay-and-fxa-popup-notification-second-sentence-with-domain-and-value-prop = First, sign up or sign in to your account to use an email mask

firefox-relay-and-fxa-opt-in-confirmation-enable-button-with-domain-and-value-prop =
    .label = Next
    .accesskey = N
