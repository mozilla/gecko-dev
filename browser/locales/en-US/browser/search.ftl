# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

## These strings are used for errors when installing OpenSearch engines, e.g.
## via "Add Search Engine" on the address bar or search bar.
## Variables
## $location-url (String) - the URL of the OpenSearch engine that was attempted to be installed.

opensearch-error-duplicate-title = Install Error
opensearch-error-duplicate-desc = { -brand-short-name } could not install the search plugin from “{ $location-url }” because an engine with the same name already exists.

opensearch-error-format-title = Invalid Format
opensearch-error-format-desc = { -brand-short-name } could not install the search engine from: { $location-url }

opensearch-error-download-title = Download Error
opensearch-error-download-desc =
    { -brand-short-name } could not download the search plugin from: { $location-url }

##

searchbar-submit =
    .tooltiptext = Submit search

# This string is displayed in the search box when the input field is empty
searchbar-input =
    .placeholder = Search

searchbar-icon =
    .tooltiptext = Search

## Infobar shown when search engine is removed and replaced.
## Variables
## $oldEngine (String) - the search engine to be removed.
## $newEngine (String) - the search engine to replace the removed search engine.

removed-search-engine-message2 = <strong>Your default search engine has been changed.</strong> { $oldEngine } is no longer available as a default search engine in { -brand-short-name }. { $newEngine } is now your default search engine. To change to another default search engine, go to settings.
remove-search-engine-button = OK

## Infobar shown when default search engine is reset due to an issue reading the settings file.
## Variables
## $newEngine (String) - the name of the new default search engine.

reset-search-settings-message = Due to a technical issue, your default search engine has been changed back to { $newEngine }. To change the default search engine, go to settings.
reset-search-settings-button = OK

## Infobar shown when user is prompted to install search engine.
## Variables
## $engineName (String) - the name of the search engine to install.

install-search-engine = Add { $engineName } as a search engine?
install-search-engine-add = Add
install-search-engine-no = No

## These strings are used for the add engine dialog when adding a custom search engine
## in settings or when adding an HTML form as a search engine.

add-engine-window =
    .title = Add Search Engine
    .style = min-width: 32em;

edit-engine-window =
    .title = Edit Search Engine
    .style = min-width: 32em;

add-engine-button = Add Custom Engine

## The following strings are used as input labels.

add-engine-name = Search engine name
add-engine-url2 = URL with %s in place of search term
add-engine-keyword2 = Keyword (optional)
# POST and GET refer to the HTTP methods.
add-engine-post-data = POST data with %s in place of search term (leave empty for GET)
add-engine-suggest-url = Suggestions URL with %s in place of search term (optional)

## The following placeholders are shown in the inputs when adding a new engine.

add-engine-name-placeholder =
    .placeholder = e.g., Mozilla Developer Network
add-engine-url-placeholder =
    .placeholder = e.g., https://developer.mozilla.com/search?q=%s
add-engine-keyword-placeholder =
    .placeholder = e.g., @mdn

## The following strings are used as error messages.

add-engine-keyword-exists = That keyword is already being used. Try a different one.
add-engine-name-exists = That name is already being used. Please choose a different one.
add-engine-no-name = Please add a name.
add-engine-no-url = Please enter a URL.
add-engine-invalid-url = That URL doesn’t look right. Please check it and try again.
add-engine-invalid-protocol = That URL doesn’t look right. Use a URL that starts with http or https.
# This error is shown when the user typed URL is missing %s.
add-engine-missing-terms-url = Try including %s in place of the search term.
# This error is shown when the user typed post data is missing %s.
add-engine-missing-terms-post-data = Try including %s in place of the search term.

## The following strings are used as labels for the dialog's buttons.

# buttonlabelextra1 is the label of a button to open the advanced section
# of the dialog.
add-engine-dialog2 =
    .buttonlabelaccept = Add Engine
    .buttonaccesskeyaccept = A
    .buttonlabelextra1 = Advanced

# buttonlabelextra1 is the label of a button to open the advanced section
# of the dialog.
edit-engine-dialog =
    .buttonlabelaccept = Save Engine
    .buttonaccesskeyaccept = S
    .buttonlabelextra1 = Advanced
