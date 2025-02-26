# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

### This file is not in a locales directory to prevent it from
### being translated as the feature is still in heavy development
### and strings are likely to change often.

## Opt-in message strings for Review Checker when it is integrated into the global sidebar.

shopping-opt-in-integrated-headline = Can you trust these reviews?
shopping-opt-in-integrated-subtitle = Turn on Review Checker in { -brand-product-name } to see how reliable product reviews are, before you buy. It uses AI technology to analyze reviews. <a data-l10n-name="learn_more">Learn more</a>

## Messages for callout for users not opted into the sidebar integrated version of Review Checker.

# Appears underneath shopping-opt-in-integrated-headline to answer the question 'Can you trust these reviews?'
shopping-callout-not-opted-in-integrated-paragraph1 = Turn on Review Checker from { -brand-product-name } to find out. It’s powered by { -fakespot-brand-full-name } and uses AI technology to analyze reviews.

shopping-callout-not-opted-in-integrated-paragraph2 = By selecting “{ shopping-opt-in-integrated-button }” you agree to { -brand-product-name }’s <a data-l10n-name="privacy_policy">privacy notice</a> and { -fakespot-brand-full-name }’s <a data-l10n-name="terms_of_use">terms of use</a>.
shopping-callout-not-opted-in-integrated-reminder-dismiss-button = Dismiss
shopping-callout-not-opted-in-integrated-reminder-accept-button = Turn on Review Checker
shopping-callout-not-opted-in-integrated-reminder-do-not-show = Don’t show this recommendation again
shopping-callout-not-opted-in-integrated-reminder-show-fewer = Show fewer recommendations
shopping-callout-not-opted-in-integrated-reminder-manage-settings = Manage settings
shopping-opt-in-integrated-subtitle-unsupported-site = Review Checker from { -brand-product-name } helps you know how reliable a product’s reviews are, before you buy. It uses AI technology to analyze reviews. <a data-l10n-name="learn_more">Learn more</a>

shopping-opt-in-integrated-privacy-policy-and-terms-of-use = Review Checker is powered by { -fakespot-brand-full-name }. By selecting “{ shopping-opt-in-integrated-button }“ you agree to { -brand-product-name }’s <a data-l10n-name="privacy_policy">privacy notice</a> and { -fakespot-brand-name }’s <a data-l10n-name="terms_of_use">terms of use.</a>
shopping-opt-in-integrated-button = Try Review Checker

## Message strings for Review Checker's empty states.

shopping-empty-state-header = Ready to check reviews
shopping-empty-state-supported-site = View a product and { -brand-product-name } will check if the reviews are reliable.

# We show a list of sites supported by Review Checker whenever a user opens the feature in an unsupported site.
# This string will be displayed above the list of sites. The list will be hardcoded and does not require localization.
shopping-empty-state-non-supported-site = Review Checker works when you shop on:

## Confirm disabling Review Checker for newly opted out users

shopping-integrated-callout-opted-out-title = Review Checker is off
shopping-integrated-callout-opted-out-subtitle = To turn it back on, select the price tag in the sidebar and turn on Review Checker.

## Callout for where to find Review Checker when the sidebar closes

shopping-integrated-callout-sidebar-closed-title = Get back to Review Checker
shopping-integrated-callout-sidebar-closed-subtitle = Select the price tag in the sidebar to see if you can trust a product’s reviews.

## Pref confirmation callout for auto-open

shopping-integrated-callout-disabled-auto-open-title = Get back to Review Checker
shopping-integrated-callout-disabled-auto-open-subtitle = Select the price tag in the sidebar to see if you can trust a product’s reviews.
shopping-integrated-callout-no-logo-disabled-auto-open-subtitle = Select the sidebar button to see if you can trust a product’s reviews.

## Auto-close toggle in settings

shopping-settings-auto-close-toggle =
    .label = Automatically close Review Checker

# Description text for regions where we support three sites. Sites are limited to Amazon, Walmart and Best Buy.
# Variables:
#   $firstSite (String) - The first shopping page name
#   $secondSite (String) - The second shopping page name
#   $thirdSite (String) - The third shopping page name
shopping-settings-auto-close-description-three-sites = When leaving { $firstSite }, { $secondSite }, and { $thirdSite }

# Description text for regions where we support only one site (e.g. currently used in FR/DE with Amazon).
# Variables:
#   $currentSite (String) - The current shopping page name
shopping-settings-auto-close-description-single-site = When leaving { $currentSite }

## Strings for a notification card about Review Checker's new position in the sidebar.
## The card will only appear for users that have the default sidebar position, which is on the left side for non RTL locales.
## Review Checker in the sidebar is only available to en-US users at this time, so we can assume that the default position is on the left side.

shopping-integrated-new-position-notification-title = Same Review Checker, new spot
shopping-integrated-new-position-notification-subtitle = Keep Review Checker and the rest of the { -brand-product-name } sidebar here — or move them to the right. Switch now or anytime in <a data-l10n-name="sidebar_settings">sidebar settings</a>.
shopping-integrated-new-position-notification-move-right-button = Move right
shopping-integrated-new-position-notification-move-left-button = Move left
shopping-integrated-new-position-notification-dismiss-button = Got it

##
