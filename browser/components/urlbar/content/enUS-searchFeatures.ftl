# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

### These strings are related to the Firefox Suggest feature. Firefox Suggest
### shows recommended and sponsored third-party results in the address bar
### panel. It also shows headings/labels above different groups of results. For
### example, a "Firefox Suggest" label is shown above bookmarks and history
### results, and an "{ $engine } Suggestions" label may be shown above search
### suggestion results.

## These terms are defined in this file because the feature is en-US only.
## They should be moved to toolkit/branding/brandings.ftl if the feature is
## exposed for localization.

-mdn-brand-name = MDN Web Docs
-mdn-brand-short-name = MDN
-yelp-brand-name = Yelp

## These strings are used in the urlbar panel.

# A label shown above the Shortcuts aka Top Sites group in the urlbar results
# if there's another result before that group. This should be consistent with
# addressbar-locbar-shortcuts-option.
urlbar-group-shortcuts =
  .label = Shortcuts

# A label shown above the top pick group in the urlbar results.
urlbar-group-best-match =
  .label = Top pick

# Label shown above an extension suggestion in the urlbar results (an
# alternative phrasing is "Extension for Firefox"). It's singular since only one
# suggested extension is displayed.
urlbar-group-addon =
  .label = { -brand-product-name } extension

# Label shown above a MDN suggestion in the urlbar results.
urlbar-group-mdn =
  .label = Recommended resource

# A label shown above urlbar suggestions for businesses and other locations
# in the user's city or a city they included in their search string (e.g., Yelp
# suggestions).
urlbar-group-local =
  .label = Local recommendations

# A message that replaces a result when the user dismisses a single suggestion.
firefox-suggest-dismissal-acknowledgment-one = Thanks for your feedback. You won’t see this suggestion again.

# A message that replaces a result when the user dismisses all suggestions of a
# particular type.
firefox-suggest-dismissal-acknowledgment-all = Thanks for your feedback. You won’t see these suggestions anymore.

# A message that replaces a result when the user dismisses a single MDN
# suggestion.
firefox-suggest-dismissal-acknowledgment-one-mdn = Thanks for your feedback. You won’t see this { -mdn-brand-short-name } suggestion again.

# A message that replaces a result when the user dismisses all MDN suggestions.
firefox-suggest-dismissal-acknowledgment-all-mdn = Thanks for your feedback. You won’t see { -mdn-brand-short-name } suggestions anymore.

# A message that replaces a result when the user dismisses a single Yelp
# suggestion.
firefox-suggest-dismissal-acknowledgment-one-yelp = Thanks for your feedback. You won’t see this { -yelp-brand-name } suggestion again.

# A message that replaces a result when the user dismisses all Yelp suggestions.
firefox-suggest-dismissal-acknowledgment-all-yelp = Thanks for your feedback. You won’t see { -yelp-brand-name } suggestions anymore.

# A message that replaces a result when the user dismisses a single Fakespot
# suggestion.
firefox-suggest-dismissal-acknowledgment-one-fakespot = Thanks for your feedback. You won’t see this { -fakespot-brand-name } suggestion again.

# A message that replaces a result when the user dismisses all Fakespot suggestions.
firefox-suggest-dismissal-acknowledgment-all-fakespot = Thanks for your feedback. You won’t see { -fakespot-brand-name } suggestions anymore.

## These strings are used for weather suggestions in the urlbar.

# This string is displayed above the current temperature
firefox-suggest-weather-currently = Currently

# This string displays the current temperature value and unit
# Variables:
#   $value (number) - The temperature value
#   $unit (String) - The unit for the temperature
firefox-suggest-weather-temperature = { $value }°{ $unit }

# This string is the title of the weather summary used for the "simplest" UI
# treatment. The temperature and unit substring should be in a <strong> tag. If
# the temperature and unit are not adjacent in the string, then it's OK to
# include only the temperature in the tag.
# Variables:
#   $temperature (number) - The temperature value
#   $unit (String) - The unit for the temperature
#   $city (String) - The name of the city the weather data is for
#   $region (String) - The name of the region (e.g., U.S. state)
firefox-suggest-weather-title-simplest = <strong>{ $temperature }°{ $unit }</strong> in { $city }, { $region }

# This string is the title of the weather summary used for the "full" and
# "simpler" UI treatments.
# Variables:
#   $city (String) - The name of the city the weather data is for
#   $region (String) - The name of the region (e.g., U.S. state)
firefox-suggest-weather-title = Weather for { $city }, { $region }

# This string displays the weather summary
# Variables:
#   $currentConditions (String) - The current weather conditions summary
#   $forecast (String) - The forecast weather conditions summary
firefox-suggest-weather-summary-text = { $currentConditions }; { $forecast }

# This string displays the high and low temperatures
# Variables:
#   $high (number) - The number for the high temperature
#   $unit (String) - The unit for the temperature
#   $low (number) - The number for the low temperature
firefox-suggest-weather-high-low = High: { $high }°{ $unit } · Low: { $low }°{ $unit }

# This string displays the name of the weather provider
# Variables:
#   $provider (String) - The name of the weather provider
firefox-suggest-weather-sponsored = { $provider } · Sponsored

## These strings are used as labels of menu items in the result menu.

firefox-suggest-command-dont-show-this =
  .label = Don’t show this
firefox-suggest-command-dont-show-mdn =
  .label = Don’t show { -mdn-brand-short-name } suggestions
firefox-suggest-command-not-relevant =
  .label = Not relevant
firefox-suggest-command-not-interested =
  .label = Not interested
firefox-suggest-command-manage-fakespot =
  .label = Manage { -fakespot-brand-name } suggestions
firefox-suggest-command-dont-show-this-suggestion =
  .label = Don’t show this suggestion
firefox-suggest-command-dont-show-any-suggestions =
  .label = Don’t show any suggestions

## These strings are used for add-on suggestions in the urlbar.

# This string explaining that the add-on suggestion is a recommendation.
firefox-suggest-addons-recommended = Recommended

## These strings are used for MDN suggestions in the urlbar.

# This string is shown in MDN suggestions and indicates the suggestion is from
# MDN.
firefox-suggest-mdn-bottom-text = { -mdn-brand-name }

## These strings are used for Yelp suggestions in the urlbar.

# This string is shown as the title in Yelp suggestions when the suggestion
# subject is a general service instead of a business name.
# Variables:
#   $service (string) - The title of the service, e.g., "coffee shops".
firefox-suggest-yelp-service-title = Top results for { $service }

# This string is shown in Yelp suggestions and indicates the suggestion is for
# Yelp.
firefox-suggest-yelp-bottom-text = Yelp · Sponsored

## These strings are used for Fakespot suggestions in the urlbar.

# This string displays inside of the badge in Fakespot suggestion.
firefox-suggest-fakespot-badge = RELIABLE REVIEWS

## This string displays rating and total reviews as a label.
## Variables:
##  $rating (number) - The number of rating for the suggestion.
##  $totalReviews (number) - The number of total reviews for the suggestion.

# Show the exact number of reviews.
firefox-suggest-fakespot-rating-and-total-reviews =
  { $totalReviews ->
     [one] { $rating } · ({ $totalReviews } review)
    *[other] { $rating } · ({ $totalReviews } reviews)
  }
# Show an approximate number of reviews (e.g. 100,000+ reviews).
firefox-suggest-fakespot-rating-and-total-reviews-overflow =
  { $totalReviews ->
     [one] { $rating } · ({ $totalReviews }+ review)
    *[other] { $rating } · ({ $totalReviews }+ reviews)
  }

# This string is shown in Fakespot suggestion and indicates the suggestion is
# sponsored.
firefox-suggest-fakespot-sponsored = { -fakespot-brand-name } · Sponsored

# These strings are used for a toggle switch in the settings UI that opts the
# user into "online" Firefox Suggest, allowing them to receive suggestions from
# Mozilla's Merino server.
addressbar-firefox-suggest-data-collection =
  .label = Improve the { -firefox-suggest-brand-name } experience
  .description = Share search query data with { -vendor-short-name } to create a richer search experience.

## Used as title on the introduction pane. The text can be formatted to span
## multiple lines as needed (line breaks are significant).

firefox-suggest-onboarding-introduction-title-1 =
  Make sure you’ve got our latest
  search experience
firefox-suggest-onboarding-introduction-title-2 =
  We’re building a better search experience —
  one you can trust
firefox-suggest-onboarding-introduction-title-3 =
  We’re building a better way to find what
  you’re looking for on the web
firefox-suggest-onboarding-introduction-title-4 =
  A faster search experience is in the works
firefox-suggest-onboarding-introduction-title-5 =
  Together, we can create the kind of search
  experience the Internet deserves
firefox-suggest-onboarding-introduction-title-6 =
  Meet { -firefox-suggest-brand-name }, the next
  evolution in search
firefox-suggest-onboarding-introduction-title-7 =
  Find the best of the web, faster.

##

firefox-suggest-onboarding-introduction-close-button =
  .title = Close

firefox-suggest-onboarding-introduction-next-button-1 = Find out how
firefox-suggest-onboarding-introduction-next-button-2 = Find out more
firefox-suggest-onboarding-introduction-next-button-3 = Show me how

## Used as title on the main pane. The text can be formatted to span
## multiple lines as needed (line breaks are significant).

firefox-suggest-onboarding-main-title-1 =
  We’re building a richer search experience
firefox-suggest-onboarding-main-title-2 =
  Help us guide the way to the
  best of the Internet
firefox-suggest-onboarding-main-title-3 =
  A richer, smarter search experience
firefox-suggest-onboarding-main-title-4 =
  Finding the best of the web, faster
firefox-suggest-onboarding-main-title-5 =
  We’re building a better search experience —
  you can help
firefox-suggest-onboarding-main-title-6 =
  It’s time to think outside the search engine
firefox-suggest-onboarding-main-title-7 =
  We’re building a smarter search experience —
  one you can trust
firefox-suggest-onboarding-main-title-8 =
  Finding the best of the web should be
  simpler and more secure.
firefox-suggest-onboarding-main-title-9 =
  Find the best of the web, faster

##

firefox-suggest-onboarding-main-description-1 = Allowing { -vendor-short-name } to process your search queries means you’re helping us create smarter, more relevant search suggestions. And, as always, we’ll keep your privacy top of mind.
firefox-suggest-onboarding-main-description-2 = When you allow { -vendor-short-name } to process your search queries, you’re helping build a better { -firefox-suggest-brand-name } for everyone. And, as always, we’ll keep your privacy top of mind.
firefox-suggest-onboarding-main-description-3 = What if your browser helped you zero in on what you’re actually looking for? Allowing { -vendor-short-name } to process your search queries helps us create more relevant search suggestions that still keep your privacy top of mind.
firefox-suggest-onboarding-main-description-4 = You’re trying to get where you’re going on the web and get on with it. When you allow { -vendor-short-name } to process your search queries, we can help you get there faster—while keeping your privacy top of mind.
firefox-suggest-onboarding-main-description-5 = Allowing { -vendor-short-name } to process your search queries will help us create more relevant suggestions for everyone. And, as always, we’ll keep your privacy top of mind.
firefox-suggest-onboarding-main-description-6 = Allowing { -vendor-short-name } to process your search queries will help us create more relevant search suggestions. We’re building { -firefox-suggest-brand-name } to help you get where you’re going on the Internet while keeping your privacy in mind.
firefox-suggest-onboarding-main-description-7 = Allowing { -vendor-short-name } to process your search queries helps us create more relevant search suggestions.
firefox-suggest-onboarding-main-description-8 = Allowing { -vendor-short-name } to process your search queries helps us provide more relevant search suggestions. We don’t use this data to profile you on the web.
firefox-suggest-onboarding-main-description-9 =
  We’re building a better search experience. When you allow { -vendor-short-name } to process your search queries, we can create more relevant search suggestions for you.
  <a data-l10n-name="learn-more-link">Learn more</a>

firefox-suggest-onboarding-main-privacy-first = No user profiling. Privacy-first, always.

firefox-suggest-onboarding-main-accept-option-label = Allow. <a data-l10n-name="learn-more-link">Learn more</a>
firefox-suggest-onboarding-main-accept-option-label-2 = Enable

firefox-suggest-onboarding-main-accept-option-description-1 = Help improve the { -firefox-suggest-brand-name } feature with more relevant suggestions. Your search queries will be processed.
firefox-suggest-onboarding-main-accept-option-description-2 = Recommended for people who support improving the { -firefox-suggest-brand-name } feature. Your search queries will be processed.
firefox-suggest-onboarding-main-accept-option-description-3 = Help improve the { -firefox-suggest-brand-name } experience. Your search queries will be processed.

firefox-suggest-onboarding-main-reject-option-label = Don’t allow.
firefox-suggest-onboarding-main-reject-option-label-2 = Keep disabled

firefox-suggest-onboarding-main-reject-option-description-1 = Keep the default { -firefox-suggest-brand-name } experience with the strictest data-sharing controls.
firefox-suggest-onboarding-main-reject-option-description-2 = Recommended for people who prefer the strictest data-sharing controls. Keep the default experience.
firefox-suggest-onboarding-main-reject-option-description-3 = Leave the default { -firefox-suggest-brand-name } experience with the strictest data-sharing controls.

firefox-suggest-onboarding-main-submit-button = Save preferences
firefox-suggest-onboarding-main-skip-link = Not now

urlbar-firefox-suggest-contextual-opt-in-title-1 =
  Find the best of the web, faster
urlbar-firefox-suggest-contextual-opt-in-description-3 =
  We’re building a better search experience. When you share search query data with { -vendor-short-name }, we can create more relevant suggestions from { -brand-short-name } and our partners.
  <a data-l10n-name="learn-more-link">Learn more</a>
urlbar-firefox-suggest-contextual-opt-in-allow = Allow suggestions
urlbar-firefox-suggest-contextual-opt-in-dismiss = Not now

## Local search mode indicator labels in the urlbar

urlbar-search-mode-bookmarks-en = Bookmarks
urlbar-search-mode-tabs-en = Tabs
urlbar-search-mode-history-en = History
urlbar-search-mode-actions-en = Actions
