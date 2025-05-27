# Data Collection

Most Firefox features operate entirely on-device. A feature involving one or
more connections to a Mozilla server qualifies as data collection. Even if the
request data is not actually retained by Mozilla, it should be reviewed as if
it might be, so that the privacy properties of Firefox can be verified without
relying on retention commitments.

As part of our overall [vision for privacy][privacy-vision], we hold Firefox to
an unusually high standard with respect to handling user data. Accordingly, any
data collection needs to be carefully vetted for consistency with this standard.
Our engineering processes are designed to ensure this vetting happens
consistently, but browsers are extremely complex and mistakes can happen. As
such, everyone who works on Firefox is responsible for understanding our rules
for data and speaking up if something doesn’t look right.

This document outlines our approach and policies on a few key topics.

## User Control

Users must be able to disable any network connection from the client to Mozilla.
Absent a good reason, this should be possible as a supported configuration in
the browser UI. In the rare situations where we do have a good reason not to
offer a control in Firefox settings (e.g., fetching the malicious add-ons
blocklist), there must still be a [documented mechanism][sumo-stop-connections]
to disable the connection in `about:config`.

## Browsing Data

A longstanding tenet of Firefox development is that _even Mozilla_ shouldn’t be
able to learn what a user does online — sites they visit, what they do on them,
etc. This is different from many other browsers and internet applications, where
the vendor routinely collects and stores sensitive user data on their servers.
Rather than asking users to trust Mozilla with this information, Firefox aims to
provide _verifiable guarantees_ of secrecy: someone should be able to inspect
the source code and verify that it is never revealed in the first place. There
are various edge-case [exceptions](#exceptions) to this posture, but that’s the
big picture.

The simplest guarantee is inspectable source code that never transmits the
data[^1]. This is how Firefox handles browsing data modulo a _very_ small number
of exceptions. Those exceptions are situations where we use some form of
encryption to create a verifiable guarantee for an important online use-case.
For example, the history and bookmark sync feature for Firefox Accounts uses
end-to-end encryption to store browsing history on Mozilla’s servers without
Mozilla learning the contents. The approved technologies for verifiable
guarantees are outlined [below](#verifiable-guarantees).

The consequence of these restrictions on sensitive data is that nearly all of
the data transmitted by Firefox to Mozilla falls into the
not-particularly-sensitive bucket. This includes the data exchanged to power
various cloud-supported features (updates, add-ons, push notifications, etc) as
well as measurement telemetry (described in the next section).

## Telemetry and Experiments

Firefox contains various [measurement probes][gleandict] to help us understand
and improve the browser, loosely known within Mozilla as “Telemetry”[^2]. This
instrumentation is enabled by default, but can be disabled during onboarding, in
settings, or through various other mechanisms (e.g., enterprise policies). Some
representative probes include [OS version][os-version-dict],
[memory usage][memory-dict], [CSS use-counters][usercounter-dict], and
[number of interactions with the bookmarks bar][bookmarks-dict]. In addition to
telemetry, other measurement probes collect data on a more de-identified basis
for measuring daily usage numbers and for some engagement and attribution
purposes.

Sitting atop this infrastructure is an optional experimentation system. This
allows us to deploy features to subsets of our user base to ensure they perform
as expected. For example, we might deploy a new network protocol backend to 1%
of our users to ensure it doesn’t increase average connection times or failure
rates.

Building a full-stack, web-compatible browser is extremely complicated, and
there is no realistic way to do it without representative telemetry and
experimentation. For example, page-load speed depends on many factors like
network conditions and hardware quirks which cannot be exhaustively tested in
automation. Telemetry allows Mozilla to determine how Firefox is performing for
users, and measure whether big changes make things faster or slower before
deploying them to everyone. The browsers that brag about not having telemetry
all use someone else’s engine (generally Chromium), and thus rely on the engine
vendor to collect telemetry and tune the stack correctly. We strive to keep
Firefox independent and competitive, so we need infrastructure to tell us what
is and is not working well.

Ordinary telemetry is associated with a pseudonymous identifier called a client
ID. Our data infrastructure endeavors to make it difficult to associate a client
ID with identifiable data, but this is not a strong guarantee. Therefore,
ordinary telemetry is generally restricted to low-sensitivity technical and
interaction data. Note that “interaction” here refers to interaction with
_Firefox UI_, not web content. The latter would inherently reveal browsing data,
and is thus off-limits.

## Verifiable Guarantees

As discussed above, sensitive information like browsing data must be protected
by a verifiable guarantee of secrecy (modulo the exceptions listed
[below](#exceptions)). This section outlines the current mechanisms Firefox uses
to provide such a guarantee in different situations:

1. **On-Device Processing:** This is the default, and should be used wherever
   possible.
2. **End-to-End Encryption:** This is used for situations where Mozilla needs to
    store user data as an opaque payload. The bookmark, history, and password
    sync feature is the canonical use-case for this feature. To be clear, the
    ‘ends’ of this type of End-to-End encryption are a users’ devices, and
    exclude Mozilla.
3. **Oblivious HTTP:** OHTTP is an [IETF standard][ietf-ohttp] for concealing
   the IP address in HTTPS transactions which can be used to create a verifiable
   guarantee that a network service cannot link a request to a client. It does
   this by routing the request through an independently-operated relay (in our
   case, [Fastly][dap-ohttp-partners]). The protocol ensures that the relay
   provider sees the source of the request but not the contents, and the
   endpoint sees the contents but not the source (more explanation
   [here][sumo-ohttp]). For this to work, the payload must be carefully vetted
   to ensure that its contents are non-identifying. There are obvious ways to
   get this wrong (e.g., including any sort of personal identifier), but subtler
   ones as well (e.g., a set of innocuous values that could be jointly unique
   to a user). For this reason, any usage of OHTTP requires careful analysis
   from a privacy expert as part of data review.
4. **DAP/Prio:** [DAP][ietf-dap] is a standards-track Multi-Party Computation
   (MPC) aggregate measurement protocol with formally verifiable privacy
   guarantees. It allows computing aggregate statistics across a population
   (e.g., how many users visit this page with a known web-compat issue) without
   the individual data points being revealed to any party off the device. There
   are a lot of [complicated details][dap-explainer], but an important upshot is
   that the protocol incorporates differential privacy guards to make it
   virtually impossible to inadvertently leak individual information with too
   small of a sample (it does this by automatically adding noise whose magnitude
   is inversely proportional to the sample size). Firefox’s DAP node is
   [operated by ISRG][dap-ohttp-partners], who also operates Let’s Encrypt.

### Exceptions

There are a few exceptional cases where information related to a website visited
by the user is sent to Mozilla without a verifiable guarantee. These are
generally unsurprising and self-explanatory, but it’s worth writing them down.
If you discover one that isn’t listed here, please flag it to the
[Firefox Technical Leadership Committee][fx-tlc] so that it can be either
addressed or added to this list:

- **Specific opt-in consent:** For example, submitting a crash report with a
  memory dump (which, depending on the crash location and the compiler memory
  layout, could include data like URLs).
- **Explicit user action:** For example, submitting a report to us that a site
  is broken.
- **Site-specific feature integrations for widely-used sites:** For example, we
  learn users visit Google to search, and we learned users visited Facebook when
  they received the contextual prompt to install the Facebook container.
- **Visiting a Mozilla-operated website:** Mozilla, like any website operator,
  has the technical capability to observe which websites are loaded by a given
  IP address. Some sites, like [addons.mozilla.org](http://addons.mozilla.org),
  also have special hooks to deliver browser functionality.
- **The New-Tab Content Feed:** Firefox provides an optional feed of news
  articles and other content on the Home and New Tab pages. This was originally
  designed to operate somewhat like a website, so the server is notified when a
  story is clicked. We are investigating routing these notifications through
  OHTTP in order to remove this exception.

## Data Review

Any data collection introduced to Firefox requires careful review. Our code
review system automatically detects the most common patterns (e.g., new or
modified [glean probes][gleandict]) and flags any matching changesets for
classification. However, these heuristics may not catch unusual patterns, and so
code reviewers are responsible for manually flagging anything that slips through
the cracks.

The details of the data review process for Firefox patches are documented
[here][data-review].

[^1]: To Mozilla. To state the obvious, the architecture of the web platform
means that interactions with a website are generally observable to the operator
of that website.

[^2]: This is referred to in documentation and settings as “technical and
interaction data”. People often mistakenly equate this with “data collection in
Firefox”, but the latter is a broader category. For example, Firefox also has a
separate [daily usage ping][usage-ping] to count users, and the content feed on
New Tab maintains its own separate communication channel. These are all
optional, but the features are controlled separately. Disabling Telemetry does
not disable the New Tab content, and vice-versa.

[privacy-vision]: https://www.mozilla.org/en-US/about/webvision/full/#privacy
[sumo-stop-connections]: https://support.mozilla.org/en-US/kb/how-stop-firefox-making-automatic-connections
[gleandict]: https://dictionary.telemetry.mozilla.org/
[os-version-dict]: https://dictionary.telemetry.mozilla.org/apps/firefox_desktop/metrics/os_version
[memory-dict]: https://dictionary.telemetry.mozilla.org/apps/firefox_desktop/metrics/memory_heap_allocated
[usercounter-dict]: https://dictionary.telemetry.mozilla.org/apps/firefox_desktop?page=1&search=use.counter.css
[bookmarks-dict]: https://dictionary.telemetry.mozilla.org/apps/firefox_desktop/metrics/browser_ui_interaction_bookmarks_bar
[ietf-ohttp]: https://datatracker.ietf.org/doc/rfc9458/
[dap-ohttp-partners]: https://blog.mozilla.org/en/products/firefox/partnership-ohttp-prio/
[sumo-ohttp]: https://support.mozilla.org/en-US/kb/ohttp-explained
[ietf-dap]: https://datatracker.ietf.org/doc/html/draft-ietf-ppm-dap-11
[dap-explainer]: https://educatedguesswork.org/posts/ppm-prio/
[fx-tlc]: https://wiki.mozilla.org/Modules/Firefox_Technical_Leadership
[data-review]: ./data-review
[usage-ping]: https://support.mozilla.org/en-US/kb/usage-ping-settings
