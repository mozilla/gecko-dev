# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

## Translation Settings section

translations-settings-back-button =
    .aria-label = Back to Settings
translations-settings-header = More Translation Settings

translations-settings-description = Set your language and site translation preferences and manage languages downloaded for offline translation.
translations-settings-add-language-button =
    .label = Add language
translations-settings-always-translate = Always translate these languages
translations-settings-never-translate = Never translate these languages
translations-settings-never-sites-header = Never translate these sites
translations-settings-never-sites-description = To add to this list, visit a site and select “Never translate this site” from the translation menu.

## Section to download language models to enable offline translation.

translations-settings-download-languages = Download languages
translations-settings-download-all-languages = All languages
translations-settings-download-languages-link = Learn more about downloading languages
# Variables:
#   $size (number) - The size of the download in megabites
translations-settings-download-size = ({ $size })
translations-settings-language-header = Language

# Variables:
#   $name (string) - The language to be downloaded
translations-settings-language-download-error =
    .heading = Download Error
    .message = Could not download { $name } language. Please try again.

# Variables:
#   $name (string) - The language to be downloaded
translations-settings-language-remove-error =
    .heading = Remove Error
    .message = Could not remove { $name } language. Please try again.

# Variables:
#   $name (string) - The display name of the language that is to be downloaded
translations-settings-download-button =
  .aria-label = Download { $name }
# Variables:
#   $name (string) - The display name of the language that is to be removed
translations-settings-remove-button =
  .aria-label = Remove { $name }
# Variables:
#   $name (string) - The display name of the language that is loading
translations-settings-loading-button =
  .aria-label = Loading { $name }
translations-settings-download-all-button =
  .aria-label = Download all languages
translations-settings-remove-all-button =
  .aria-label = Remove all languages
translations-settings-loading-all-button =
  .aria-label = Loading all languages
# Variables:
#   $name (string) - The display name of the language that is Always/Never translated
translations-settings-remove-language-button-2 =
  .aria-label = Remove { $name }
# Variables:
#   $name (string) - The site address that is Never to be translated
translations-settings-remove-site-button-2 =
  .aria-label = Remove { $name }
