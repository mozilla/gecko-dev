config = {
    "nightly_build": True,
    "branch": "mozilla-central",
    "en_us_binary_url": "https://ftp.mozilla.org/pub/mozilla.org/firefox/nightly/latest-mozilla-central/",
    "update_channel": "nightly",
    "latest_mar_dir": '/pub/mozilla.org/firefox/nightly/latest-mozilla-central',

    # l10n
    "hg_l10n_base": "https://hg.mozilla.org/l10n-central",

    # mar
    "enable_partials": True,
    "mar_tools_url": "https://ftp.mozilla.org/pub/mozilla.org/firefox/nightly/latest-mozilla-central/",
    "previous_mar_url": "http://ftp.mozilla.org/pub/mozilla.org/firefox/nightly/latest-mozilla-central-l10n",

    # repositories
    "mozilla_dir": "mozilla-central",
    "repos": [{
        "vcs": "hg",
        "repo": "https://hg.mozilla.org/mozilla-central",
        "revision": "default",
        "dest": "ash",
    }, {
        "vcs": "hg",
        "repo": "https://hg.mozilla.org/build/tools",
        "revision": "default",
        "dest": "tools",
    }, {
        "vcs": "hg",
        "repo": "https://hg.mozilla.org/build/compare-locales",
        "revision": "RELEASE_AUTOMATION"
    }],
}
