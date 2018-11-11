ChromeUtils.import("resource://gre/modules/Services.jsm");

const BASE_HOSTNAMES = ["example.org", "example.co.uk"];
const SUBDOMAINS = ["", "pub.", "www.", "other."];

const cs = Cc["@mozilla.org/cookieService;1"].getService(Ci.nsICookieService);
const cm = cs.QueryInterface(Ci.nsICookieManager);

function run_test() {
    Services.prefs.setIntPref("network.cookie.staleThreshold", 0);
    add_task(async function() {
        await test_basic_eviction("example.org");
        cm.removeAll();
    });

    run_next_test();
}

async function test_basic_eviction(base_host) {
    Services.prefs.setIntPref("network.cookie.quotaPerHost", 2);
    Services.prefs.setIntPref("network.cookie.maxPerHost", 5);

    const BASE_URI = Services.io.newURI("http://" + base_host);
    const FOO_PATH = Services.io.newURI("http://" + base_host + "/foo/");
    const BAR_PATH = Services.io.newURI("http://" + base_host + "/bar/");

    await setCookie("session_foo_path_1", null, "/foo", null, FOO_PATH);
    await setCookie("session_foo_path_2", null, "/foo", null, FOO_PATH);
    await setCookie("session_foo_path_3", null, "/foo", null, FOO_PATH);
    await setCookie("session_foo_path_4", null, "/foo", null, FOO_PATH);
    await setCookie("session_foo_path_5", null, "/foo", null, FOO_PATH);
    verifyCookies(["session_foo_path_1",
                   "session_foo_path_2",
                   "session_foo_path_3",
                   "session_foo_path_4",
                   "session_foo_path_5"], BASE_URI);

    // Check if cookies are evicted by creation time.
    await setCookie("session_foo_path_6", null, "/foo", null, FOO_PATH);
    verifyCookies(["session_foo_path_4",
                   "session_foo_path_5",
                   "session_foo_path_6"], BASE_URI);

    await setCookie("session_bar_path_1", null, "/bar", null, BAR_PATH);
    await setCookie("session_bar_path_2", null, "/bar", null, BAR_PATH);

    verifyCookies(["session_foo_path_4",
                   "session_foo_path_5",
                   "session_foo_path_6",
                   "session_bar_path_1",
                   "session_bar_path_2"], BASE_URI);

    // Check if cookies are evicted by last accessed time.
    cs.getCookieString(FOO_PATH, null);
    await setCookie("session_foo_path_7", null, "/foo", null, FOO_PATH);
    verifyCookies(["session_foo_path_5",
                   "session_foo_path_6",
                   "session_foo_path_7"], BASE_URI);

    const EXPIRED_TIME = 3;

    await setCookie("non_session_expired_foo_path_1", null, "/foo", EXPIRED_TIME, FOO_PATH);
    await setCookie("non_session_expired_foo_path_2", null, "/foo", EXPIRED_TIME, FOO_PATH);
    verifyCookies(["session_foo_path_5",
                   "session_foo_path_6",
                   "session_foo_path_7",
                   "non_session_expired_foo_path_1",
                   "non_session_expired_foo_path_2"], BASE_URI);

    // Check if expired cookies are evicted first.
    await new Promise(resolve => do_timeout(EXPIRED_TIME * 1000, resolve));
    await setCookie("session_foo_path_8", null, "/foo", null, FOO_PATH);
    verifyCookies(["session_foo_path_6",
                   "session_foo_path_7",
                   "session_foo_path_8"], BASE_URI);
}

// Verify that the given cookie names exist, and are ordered from least to most recently accessed
function verifyCookies(names, uri) {
    Assert.equal(cm.countCookiesFromHost(uri.host), names.length);
    let actual_cookies = [];
    for (let cookie of cm.getCookiesFromHost(uri.host, {})) {
        actual_cookies.push(cookie);
    }
    if (names.length != actual_cookies.length) {
        let left = names.filter(function(n) {
            return actual_cookies.findIndex(function(c) {
                return c.name == n;
            }) == -1;
        });
        let right = actual_cookies.filter(function(c) {
            return names.findIndex(function(n) {
                return c.name == n;
            }) == -1;
        }).map(function(c) { return c.name; });
        if (left.length) {
            info("unexpected cookies: " + left);
        }
        if (right.length) {
            info("expected cookies: " + right);
        }
    }
    Assert.equal(names.length, actual_cookies.length);
    actual_cookies.sort(function(a, b) {
        if (a.lastAccessed < b.lastAccessed)
            return -1;
        if (a.lastAccessed > b.lastAccessed)
            return 1;
        return 0;
    });
    for (var i = 0; i < names.length; i++) {
        Assert.equal(names[i], actual_cookies[i].name);
        Assert.equal(names[i].startsWith("session"), actual_cookies[i].isSession);
    }
}

var lastValue = 0;
function setCookie(name, domain, path, maxAge, url) {
    let value = name + "=" + ++lastValue;
    var s = "setting cookie " + value;
    if (domain) {
        value += "; Domain=" + domain;
        s += " (d=" + domain + ")";
    }
    if (path) {
        value += "; Path=" + path;
        s += " (p=" + path + ")";
    }
    if (maxAge) {
        value += "; Max-Age=" + maxAge;
        s += " (non-session)";
    } else {
        s += " (session)";
    }
    s += " for " + url.spec;
    info(s);
    cs.setCookieStringFromHttp(url, null, null, value, null, null);
    return new Promise(function(resolve) {
        // Windows XP has low precision timestamps that cause our cookie eviction
        // algorithm to produce different results from other platforms. We work around
        // this by ensuring that there's a clear gap between each cookie update.
        do_timeout(10, resolve);
    });
}
