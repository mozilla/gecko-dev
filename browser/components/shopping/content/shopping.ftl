# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

### This file is not in a locales directory to prevent it from
### being translated as the feature is still in heavy development
### and strings are likely to change often.

## Opt-in message strings for Review Checker when it is integrated into the global sidebar.

shopping-opt-in-integrated-headline = Can you trust these reviews?
shopping-opt-in-integrated-subtitle-single-site = Try Review Checker from { -brand-product-name } to find out. It helps you know if a product’s reviews are real or fake, before you buy. <a data-l10n-name="learn_more">Learn more</a>

# Dynamic subtitle. Sites displayed are limited to "Amazon", "Walmart" or "Best Buy".
# Variables:
#   $currentSite (str) - The current shopping page name
#   $secondSite (str) - A second shopping page name
#   $thirdSite (str) - A third shopping page name
shopping-opt-in-integrated-subtitle-all-sites = Try Review Checker from { -brand-product-name } to find out. It helps you know if a product’s reviews are real or fake, before you buy. It works on { $secondSite } and { $thirdSite }, too. <a data-l10n-name="learn_more">Learn more</a>

shopping-opt-in-integrated-privacy-policy-and-terms-of-use = Review Checker is powered by { -fakespot-brand-full-name }. By selecting “{ shopping-opt-in-integrated-button }“ you agree to { -brand-product-name }’s <a data-l10n-name="privacy_policy">privacy notice</a> and { -fakespot-brand-name }’s <a data-l10n-name="terms_of_use">terms of use.</a>
shopping-opt-in-integrated-button = Try Review Checker
