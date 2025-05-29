# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import logging
import os
import shutil
import subprocess
import tempfile


def generate_browser_desktop_entry_file_text(
    log,
    build_variables,
    release_product,
    release_type,
    fluent_localization,
    fluent_resource_loader,
):
    desktop_entry = generate_browser_desktop_entry(
        log,
        build_variables,
        release_product,
        release_type,
        fluent_localization,
        fluent_resource_loader,
    )
    desktop_entry_file_text = "\n".join(desktop_entry)
    return desktop_entry_file_text


def generate_browser_desktop_entry(
    log,
    build_variables,
    release_product,
    release_type,
    fluent_localization,
    fluent_resource_loader,
):
    localizations = _create_fluent_localizations(
        fluent_resource_loader, fluent_localization, release_type, release_product, log
    )
    return _generate_browser_desktop_entry(build_variables, localizations)


def _create_fluent_localizations(
    fluent_resource_loader, fluent_localization, release_type, release_product, log
):
    brand_fluent_filename = "brand.ftl"
    l10n_central_repo = "https://github.com/mozilla-l10n/firefox-l10n"
    desktop_entry_fluent_filename = "linuxDesktopEntry.ftl"

    l10n_dir = tempfile.mkdtemp()

    loader = fluent_resource_loader(os.path.join(l10n_dir, "{locale}"))

    localizations = {}
    linux_l10n_changesets = _load_linux_l10n_changesets(
        "browser/locales/l10n-changesets.json"
    )
    locales = ["en-US"]
    locales.extend(linux_l10n_changesets.keys())
    en_US_brand_fluent_filename = _get_en_US_brand_fluent_filename(
        brand_fluent_filename, release_type, release_product
    )

    all_revisions = {val["revision"] for val in linux_l10n_changesets.values()}
    assert len(all_revisions) == 1
    l10n_revision = all_revisions.pop()

    with tempfile.TemporaryDirectory() as l10n_repo_clone:
        subprocess.check_call(["git", "init", l10n_repo_clone])
        subprocess.check_call(
            ["git", "remote", "add", "origin", l10n_central_repo], cwd=l10n_repo_clone
        )
        subprocess.check_call(
            [
                "git",
                "fetch",
                "--no-progress",
                "--depth=1",
                "origin",
                l10n_revision,
            ],
            cwd=l10n_repo_clone,
        )
        subprocess.check_call(
            [
                "git",
                "reset",
                "--hard",
                "FETCH_HEAD",
            ],
            cwd=l10n_repo_clone,
        )

        for locale in locales:
            locale_dir = os.path.join(l10n_dir, locale)
            os.mkdir(locale_dir)
            localized_desktop_entry_filename = os.path.join(
                locale_dir, desktop_entry_fluent_filename
            )
            if locale == "en-US":
                en_US_desktop_entry_fluent_filename = os.path.join(
                    "browser",
                    "locales",
                    "en-US",
                    "browser",
                    desktop_entry_fluent_filename,
                )
                shutil.copyfile(
                    en_US_desktop_entry_fluent_filename,
                    localized_desktop_entry_filename,
                )
            else:
                non_en_US_fluent_resource_file_name = os.path.join(
                    f"{l10n_repo_clone}",
                    f"{locale}",
                    "browser",
                    "browser",
                    f"{desktop_entry_fluent_filename}",
                )
                print("locale", locale, non_en_US_fluent_resource_file_name)
                if not os.path.isfile(non_en_US_fluent_resource_file_name):
                    log(
                        logging.WARNING,
                        "repackage-deb",
                        {
                            "fluent_resource_file_name": desktop_entry_fluent_filename,
                            "locale": locale,
                            "resource_file_name": non_en_US_fluent_resource_file_name,
                        },
                        "Missing {fluent_resource_file_name} for {locale}: {resource_file_name}",
                    )
                    continue
                shutil.copyfile(
                    non_en_US_fluent_resource_file_name,
                    localized_desktop_entry_filename,
                )

            shutil.copyfile(
                en_US_brand_fluent_filename,
                os.path.join(locale_dir, brand_fluent_filename),
            )

            fallbacks = [locale]
            if locale != "en-US":
                fallbacks.append("en-US")
            localizations[locale] = fluent_localization(
                fallbacks,
                [desktop_entry_fluent_filename, brand_fluent_filename],
                loader,
            )

    return localizations


def _get_en_US_brand_fluent_filename(
    brand_fluent_filename, release_type, release_product
):
    branding_fluent_filename_template = os.path.join(
        "browser/branding/{brand}/locales/en-US", brand_fluent_filename
    )
    if release_type == "nightly":
        return branding_fluent_filename_template.format(brand="nightly")
    elif release_type == "release" or release_type == "release-rc":
        return branding_fluent_filename_template.format(brand="official")
    elif release_type == "beta" and release_product == "firefox":
        return branding_fluent_filename_template.format(brand="official")
    elif release_type == "beta" and release_product == "devedition":
        return branding_fluent_filename_template.format(brand="aurora")
    elif release_type.startswith("esr"):
        return branding_fluent_filename_template.format(brand="official")
    else:
        return branding_fluent_filename_template.format(brand="unofficial")


def _load_linux_l10n_changesets(l10n_changesets_filename):
    with open(l10n_changesets_filename) as l10n_changesets_file:
        l10n_changesets = json.load(l10n_changesets_file)
        return {
            locale: changeset
            for locale, changeset in l10n_changesets.items()
            if any(platform.startswith("linux") for platform in changeset["platforms"])
        }


def _generate_browser_desktop_entry(build_variables, localizations):
    mime_types = [
        "application/json",
        "application/pdf",
        "application/rdf+xml",
        "application/rss+xml",
        "application/x-xpinstall",
        "application/xhtml+xml",
        "application/xml",
        "audio/flac",
        "audio/ogg",
        "audio/webm",
        "image/avif",
        "image/gif",
        "image/jpeg",
        "image/png",
        "image/svg+xml",
        "image/webp",
        "text/html",
        "text/xml",
        "video/ogg",
        "video/webm",
        "x-scheme-handler/chrome",
        "x-scheme-handler/http",
        "x-scheme-handler/https",
        "x-scheme-handler/mailto",
    ]

    categories = [
        "GNOME",
        "GTK",
        "Network",
        "WebBrowser",
    ]

    actions = [
        {
            "name": "new-window",
            "message": "desktop-action-new-window-name",
            "command": f"{build_variables['PKG_NAME']} --new-window %u",
        },
        {
            "name": "new-private-window",
            "message": "desktop-action-new-private-window-name",
            "command": f"{build_variables['PKG_NAME']} --private-window %u",
        },
        {
            "name": "open-profile-manager",
            "message": "desktop-action-open-profile-manager",
            "command": f"{build_variables['PKG_NAME']} --ProfileManager",
        },
    ]

    desktop_entry = _desktop_entry_section(
        "Desktop Entry",
        [
            {
                "key": "Version",
                "value": "1.0",
            },
            {
                "key": "Type",
                "value": "Application",
            },
            {
                "key": "Exec",
                "value": f"{build_variables['PKG_NAME']} %u",
            },
            {
                "key": "Terminal",
                "value": "false",
            },
            {
                "key": "X-MultipleArgs",
                "value": "false",
            },
            {
                "key": "Icon",
                "value": build_variables["Icon"],
            },
            {
                "key": "StartupWMClass",
                "value": (
                    "firefox-aurora"
                    if build_variables["PKG_NAME"] == "firefox-devedition"
                    else build_variables.get(
                        "StartupWMClass", build_variables["PKG_NAME"]
                    )
                ),
            },
            {
                "key": "DBusActivatable",
                "value": build_variables.get("DBusActivatable", "false"),
                "condition": "DBusActivatable" in build_variables,
            },
            {
                "key": "Categories",
                "value": _desktop_entry_list(categories),
            },
            {
                "key": "MimeType",
                "value": _desktop_entry_list(mime_types),
            },
            {
                "key": "StartupNotify",
                "value": "true",
            },
            {
                "key": "Actions",
                "value": _desktop_entry_list([action["name"] for action in actions]),
            },
            {"key": "Name", "value": "desktop-entry-name", "l10n": True},
            {"key": "Comment", "value": "desktop-entry-comment", "l10n": True},
            {"key": "GenericName", "value": "desktop-entry-generic-name", "l10n": True},
            {"key": "Keywords", "value": "desktop-entry-keywords", "l10n": True},
            {
                "key": "X-GNOME-FullName",
                "value": "desktop-entry-x-gnome-full-name",
                "l10n": True,
            },
        ],
        localizations,
    )

    for action in actions:
        desktop_entry.extend(
            _desktop_entry_section(
                f"Desktop Action {action['name']}",
                [
                    {
                        "key": "Name",
                        "value": action["message"],
                        "l10n": True,
                    },
                    {
                        "key": "Exec",
                        "value": action["command"],
                    },
                ],
                localizations,
            )
        )

    return desktop_entry


def _desktop_entry_list(iterable):
    delimiter = ";"
    return f"{delimiter.join(iterable)}{delimiter}"


def _desktop_entry_attribute(key, value, locale=None, localizations=None):
    if not locale and not localizations:
        return f"{key}={value}"
    if locale and locale == "en-US":
        return f"{key}={localizations[locale].format_value(value)}"
    else:
        return f"{key}[{locale.replace('-', '_')}]={localizations[locale].format_value(value)}"


def _desktop_entry_section(header, attributes, localizations):
    desktop_entry_section = [f"[{header}]"]
    l10n_attributes = [attribute for attribute in attributes if attribute.get("l10n")]
    non_l10n_attributes = [
        attribute for attribute in attributes if not attribute.get("l10n")
    ]
    for attribute in non_l10n_attributes:
        if not attribute.get("condition", True):
            continue
        desktop_entry_section.append(
            _desktop_entry_attribute(attribute["key"], attribute["value"])
        )
    for attribute in l10n_attributes:
        for locale in localizations:
            desktop_entry_section.append(
                _desktop_entry_attribute(
                    attribute["key"], attribute["value"], locale, localizations
                )
            )
    desktop_entry_section.append("")
    return desktop_entry_section
