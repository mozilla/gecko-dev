# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

addons-window =
    .title = Add-ons Manager

search-header =
    .placeholder = Search addons.mozilla.org
    .searchbuttonlabel = Search

search-header-shortcut =
    .key = f

loading-label =
    .value = Loading…

list-empty-installed =
    .value = You don’t have any add-ons of this type installed

list-empty-available-updates =
    .value = No updates found

list-empty-recent-updates =
    .value = You haven’t recently updated any add-ons

list-empty-find-updates =
    .label = Check For Updates

list-empty-button =
    .label = Learn more about add-ons

install-addon-from-file =
    .label = Install Add-on From File…
    .accesskey = I

help-button = Add-ons Support

preferences =
    { PLATFORM() ->
        [windows] { -brand-short-name } Options
       *[other] { -brand-short-name } Preferences
    }

tools-menu =
    .tooltiptext = Tools for all add-ons

show-unsigned-extensions-button =
    .label = Some extensions could not be verified

show-all-extensions-button =
    .label = Show all extensions

debug-addons =
    .label = Debug Add-ons
    .accesskey = b

cmd-show-details =
    .label = Show More Information
    .accesskey = S

cmd-find-updates =
    .label = Find Updates
    .accesskey = F

cmd-preferences =
    .label =
        { PLATFORM() ->
            [windows] Options
           *[other] Preferences
        }
    .accesskey =
        { PLATFORM() ->
            [windows] O
           *[other] P
        }

cmd-enable-theme =
    .label = Wear Theme
    .accesskey = W

cmd-disable-theme =
    .label = Stop Wearing Theme
    .accesskey = W

cmd-install-addon =
    .label = Install
    .accesskey = I

cmd-contribute =
    .label = Contribute
    .accesskey = C
    .tooltiptext = Contribute to the development of this add-on

discover-title = What are Add-ons?

discover-description =
    Add-ons are applications that let you personalize { -brand-short-name } with
    extra functionality or style. Try a time-saving sidebar, a weather notifier, or a themed look to make { -brand-short-name }
    your own.

discover-footer =
    When you’re connected to the internet, this pane will feature
    some of the best and most popular add-ons for you to try out.

detail-version =
    .label = Version

detail-last-updated =
    .label = Last Updated

detail-contributions-description = The developer of this add-on asks that you help support its continued development by making a small contribution.

detail-update-type =
    .value = Automatic Updates

detail-update-default =
    .label = Default
    .tooltiptext = Automatically install updates only if that’s the default

detail-update-automatic =
    .label = On
    .tooltiptext = Automatically install updates

detail-update-manual =
    .label = Off
    .tooltiptext = Don’t automatically install updates

detail-home =
    .label = Homepage

detail-home-value =
    .value = { detail-home.label }

detail-repository =
    .label = Add-on Profile

detail-repository-value =
    .value = { detail-repository.label }

detail-check-for-updates =
    .label = Check for Updates
    .accesskey = U
    .tooltiptext = Check for updates for this add-on

detail-show-preferences =
    .label =
        { PLATFORM() ->
            [windows] Options
           *[other] Preferences
        }
    .accesskey =
        { PLATFORM() ->
            [windows] O
           *[other] P
        }
    .tooltiptext =
        { PLATFORM() ->
            [windows] Change this add-on’s options
           *[other] Change this add-on’s preferences
        }

detail-rating =
    .value = Rating

addon-restart-now =
    .label = Restart now

disabled-unsigned-heading =
    .value = Some add-ons have been disabled

disabled-unsigned-description =
    The following add-ons have not been verified for use in { -brand-short-name }. You can
    <label data-l10n-name="find-addons">find replacements</label> or ask the developer to get them verified.

disabled-unsigned-learn-more = Learn more about our efforts to help keep you safe online.

disabled-unsigned-devinfo =
    Developers interested in getting their add-ons verified can continue by reading our
    <label data-l10n-name="learn-more">manual</label>.

plugin-deprecation-description =
    Missing something? Some plugins are no longer supported by { -brand-short-name }. <label data-l10n-name="learn-more">Learn More.</label>

legacy-warning-show-legacy = Show legacy extensions

legacy-extensions =
    .value = Legacy Extensions

legacy-extensions-description =
    These extensions do not meet current { -brand-short-name } standards so they have been deactivated. <label data-l10n-name="legacy-learn-more">Learn about the changes to add-ons</label>

extensions-view-discover =
    .name = Get Add-ons
    .tooltiptext = { extensions-view-discover.name }

extensions-view-recent-updates =
    .name = Recent Updates
    .tooltiptext = { extensions-view-recent-updates.name }

extensions-view-available-updates =
    .name = Available Updates
    .tooltiptext = { extensions-view-available-updates.name }

## These are global warnings

extensions-warning-safe-mode-label =
    .value = All add-ons have been disabled by safe mode.
extensions-warning-safe-mode-container =
    .tooltiptext = { extensions-warning-safe-mode-label.value }

extensions-warning-check-compatibility-label =
    .value = Add-on compatibility checking is disabled. You may have incompatible add-ons.
extensions-warning-check-compatibility-container =
    .tooltiptext = { extensions-warning-check-compatibility-label.value }

extensions-warning-check-compatibility-enable =
    .label = Enable
    .tooltiptext = Enable add-on compatibility checking

extensions-warning-update-security-label =
    .value = Add-on update security checking is disabled. You may be compromised by updates.
extensions-warning-update-security-container =
    .tooltiptext = { extensions-warning-update-security-label.value }

extensions-warning-update-security-enable =
    .label = Enable
    .tooltiptext = Enable add-on update security checking

## Strings connected to add-on updates

extensions-updates-check-for-updates =
    .label = Check for Updates
    .accesskey = C

extensions-updates-view-updates =
    .label = View Recent Updates
    .accesskey = V

# This menu item is a checkbox that toggles the default global behavior for
# add-on update checking.

extensions-updates-update-addons-automatically =
    .label = Update Add-ons Automatically
    .accesskey = A

## Specific add-ons can have custom update checking behaviors ("Manually",
## "Automatically", "Use default global behavior"). These menu items reset the
## update checking behavior for all add-ons to the default global behavior
## (which itself is either "Automatically" or "Manually", controlled by the
## extensions-updates-update-addons-automatically.label menu item).

extensions-updates-reset-updates-to-automatic =
    .label = Reset All Add-ons to Update Automatically
    .accesskey = R

extensions-updates-reset-updates-to-manual =
    .label = Reset All Add-ons to Update Manually
    .accesskey = R

## Status messages displayed when updating add-ons

extensions-updates-updating =
    .value = Updating add-ons
extensions-updates-installed =
    .value = Your add-ons have been updated.
extensions-updates-downloaded =
    .value = Your add-on updates have been downloaded.
extensions-updates-restart =
    .label = Restart now to complete installation
extensions-updates-none-found =
    .value = No updates found
extensions-updates-manual-updates-found =
    .label = View Available Updates
extensions-updates-update-selected =
    .label = Install Updates
    .tooltiptext = Install available updates in this list
