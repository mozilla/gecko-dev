import asyncio
import pytest
from webdriver.error import TimeoutException

from tests.bidi import wait_for_bidi_events
from .. import assert_navigation_info


pytestmark = pytest.mark.asyncio

NAVIGATION_ABORTED_EVENT = "browsingContext.navigationAborted"
NAVIGATION_FAILED_EVENT = "browsingContext.navigationFailed"
NAVIGATION_STARTED_EVENT = "browsingContext.navigationStarted"
USER_PROMPT_OPENED_EVENT = "browsingContext.userPromptOpened"


async def test_unsubscribe(bidi_session, inline, new_tab):
    await bidi_session.session.subscribe(events=[NAVIGATION_FAILED_EVENT])
    await bidi_session.session.unsubscribe(events=[NAVIGATION_FAILED_EVENT])

    # Track all received browsingContext.navigationFailed events in the events array.
    events = []

    async def on_event(method, data):
        events.append(data)

    remove_listener = bidi_session.add_event_listener(NAVIGATION_FAILED_EVENT, on_event)

    iframe_url = inline("<div>foo</div>", domain="alt")
    page_url = inline(
        f"""<iframe src={iframe_url}></iframe>""",
        parameters={"pipe": "header(Content-Security-Policy, default-src 'self')"},
    )

    await bidi_session.browsing_context.navigate(
        context=new_tab["context"], url=page_url, wait="none"
    )

    with pytest.raises(TimeoutException):
        await wait_for_bidi_events(bidi_session, events, 1, timeout=0.5)

    remove_listener()


async def test_with_csp_meta_tag(
    bidi_session,
    subscribe_events,
    inline,
    new_tab,
    wait_for_event,
    wait_for_future_safe,
):
    iframe_url = inline("<div>foo</div>", domain="alt")
    page_url = inline(
        f"""
<!DOCTYPE html>
<html>
    <head>
        <meta
  http-equiv="Content-Security-Policy"
  content="default-src 'self'" />
    </head>
    <body><iframe src="{iframe_url}"></iframe></body>
</html>
"""
    )
    await subscribe_events(events=[NAVIGATION_FAILED_EVENT, NAVIGATION_STARTED_EVENT])

    # Track all received browsingContext.navigationStarted events in the events array.
    events = []

    async def on_event(method, data):
        events.append(data)

    remove_listener = bidi_session.add_event_listener(
        NAVIGATION_STARTED_EVENT, on_event
    )

    on_navigation_failed = wait_for_event(NAVIGATION_FAILED_EVENT)
    await bidi_session.browsing_context.navigate(
        context=new_tab["context"], url=page_url, wait="complete"
    )
    event = await wait_for_future_safe(on_navigation_failed)

    contexts = await bidi_session.browsing_context.get_tree(root=new_tab["context"])
    iframe_context = contexts[0]["children"][0]["context"]

    started_event_for_iframe = next(
        event for event in events if event["context"] == iframe_context
    )

    # Make sure that the iframe navigation was blocked.
    assert_navigation_info(
        event,
        {
            "context": iframe_context,
            "navigation": started_event_for_iframe["navigation"],
            "url": iframe_url,
        },
    )

    remove_listener()


@pytest.mark.parametrize(
    "header",
    [
        "Content-Security-Policy, default-src 'self'",
        "Cross-Origin-Embedder-Policy, require-corp",
    ],
)
async def test_with_content_blocking_header_in_top_context(
    bidi_session,
    subscribe_events,
    inline,
    new_tab,
    wait_for_event,
    wait_for_future_safe,
    header,
):
    iframe_url = inline("<div>foo</div>", domain="alt")
    page_url = inline(
        f"""<iframe src={iframe_url}></iframe>""",
        parameters={"pipe": f"header({header})"},
    )
    await subscribe_events(events=[NAVIGATION_FAILED_EVENT, NAVIGATION_STARTED_EVENT])

    # Track all received browsingContext.navigationStarted events in the events array.
    events = []

    async def on_event(method, data):
        events.append(data)

    remove_listener = bidi_session.add_event_listener(
        NAVIGATION_STARTED_EVENT, on_event
    )

    on_navigation_failed = wait_for_event(NAVIGATION_FAILED_EVENT)
    await bidi_session.browsing_context.navigate(
        context=new_tab["context"], url=page_url, wait="none"
    )
    event = await wait_for_future_safe(on_navigation_failed)

    contexts = await bidi_session.browsing_context.get_tree(root=new_tab["context"])
    iframe_context = contexts[0]["children"][0]["context"]

    started_event_for_iframe = next(
        event for event in events if event["context"] == iframe_context
    )

    # Make sure that the iframe navigation was blocked.
    assert_navigation_info(
        event,
        {
            "context": iframe_context,
            "navigation": started_event_for_iframe["navigation"],
            "url": iframe_url,
        },
    )

    remove_listener()


@pytest.mark.parametrize(
    "header_value",
    [
        "SAMEORIGIN",
        "DENY",
    ],
)
async def test_with_x_frame_options_header(
    bidi_session,
    subscribe_events,
    inline,
    new_tab,
    wait_for_event,
    wait_for_future_safe,
    header_value
):
    iframe_url = inline(
        "<div>foo</div>",
        parameters={"pipe": f"header(X-Frame-Options, {header_value})"},
    )
    page_url = inline(f"""<iframe src={iframe_url}></iframe>""", domain="alt")
    await subscribe_events(events=[NAVIGATION_FAILED_EVENT, NAVIGATION_STARTED_EVENT])

    # Track all received browsingContext.navigationStarted events in the events array.
    events = []

    async def on_event(method, data):
        events.append(data)

    remove_listener = bidi_session.add_event_listener(
        NAVIGATION_STARTED_EVENT, on_event
    )

    on_navigation_failed = wait_for_event(NAVIGATION_FAILED_EVENT)
    await bidi_session.browsing_context.navigate(
        context=new_tab["context"], url=page_url, wait="none"
    )
    event = await wait_for_future_safe(on_navigation_failed)

    contexts = await bidi_session.browsing_context.get_tree(root=new_tab["context"])
    iframe_context = contexts[0]["children"][0]["context"]

    started_event_for_iframe = next(
        event for event in events if event["context"] == iframe_context
    )

    # Make sure that the iframe navigation was blocked.
    assert_navigation_info(
        event,
        {
            "context": iframe_context,
            "navigation": started_event_for_iframe["navigation"],
            "url": iframe_url,
        },
    )

    remove_listener()


async def test_with_new_navigation(
    bidi_session,
    subscribe_events,
    inline,
    url,
    new_tab,
    wait_for_event,
    wait_for_future_safe,
):
    slow_page_url = url(
        "/webdriver/tests/bidi/browsing_context/support/empty.html?pipe=trickle(d10)"
    )
    # Depending on implementation, the `trickle(d10)` page can or can not yet
    # create a new document. Depending on this, `aborted` or `failed` event
    # should be emitted.
    await subscribe_events(
        events=[NAVIGATION_ABORTED_EVENT, NAVIGATION_FAILED_EVENT])

    result = await bidi_session.browsing_context.navigate(
        context=new_tab["context"], url=slow_page_url, wait="none"
    )

    events = []

    async def on_event(method, data):
        events.append(data)

    remove_listener_1 = bidi_session.add_event_listener(
        NAVIGATION_ABORTED_EVENT, on_event)
    remove_listener_2 = bidi_session.add_event_listener(NAVIGATION_FAILED_EVENT,
                                                        on_event)

    second_url = inline("<div>foo</div>")

    # Trigger the second navigation which should fail the first one.
    await bidi_session.browsing_context.navigate(
        context=new_tab["context"], url=second_url, wait="none"
    )

    await wait_for_bidi_events(bidi_session, events, 1, timeout=1)

    # Make sure that the first navigation failed or aborted.
    assert_navigation_info(
        events[0],
        {
            "context": new_tab["context"],
            "navigation": result["navigation"],
            "url": slow_page_url,
        },
    )

    remove_listener_1()
    remove_listener_2()


async def test_with_new_navigation_inside_page(
    bidi_session,
    subscribe_events,
    inline,
    new_tab,
    wait_for_event,
    wait_for_future_safe,
):
    second_url = inline("<div>foo</div>")
    slow_page_url = inline(
        f"""
<!DOCTYPE html>
<html>
    <body>
        <img src="/webdriver/tests/bidi/browsing_context/support/empty.svg?pipe=trickle(d10)" />
        <script>
            location.href = "{second_url}"
        </script>
        <img src="/webdriver/tests/bidi/browsing_context/support/empty.svg?pipe=trickle(d10)" />
    </body>
</html>
"""
    )

    # Depending on implementation, the `trickle(d10)` page can or can not yet
    # create a new document. Depending on this, `aborted` or `failed` event
    # should be emitted.
    await subscribe_events(
        events=[NAVIGATION_ABORTED_EVENT, NAVIGATION_FAILED_EVENT])

    events = []

    async def on_event(method, data):
        events.append(data)

    remove_listener_1 = bidi_session.add_event_listener(
        NAVIGATION_ABORTED_EVENT, on_event)
    remove_listener_2 = bidi_session.add_event_listener(NAVIGATION_FAILED_EVENT,
                                                        on_event)

    result = await bidi_session.browsing_context.navigate(
        context=new_tab["context"], url=slow_page_url, wait="none"
    )

    await wait_for_bidi_events(bidi_session, events, 1, timeout=1)

    # Make sure that the first navigation failed.
    assert_navigation_info(
        events[0],
        {
            "context": new_tab["context"],
            "navigation": result["navigation"],
            "url": slow_page_url,
        },
    )

    remove_listener_1()
    remove_listener_2()


@pytest.mark.parametrize("type_hint", ["tab", "window"])
async def test_close_context(
    bidi_session,
    subscribe_events,
    url,
    wait_for_event,
    wait_for_future_safe,
    type_hint,
):
    new_context = await bidi_session.browsing_context.create(type_hint=type_hint)
    slow_page_url = url(
        "/webdriver/tests/bidi/browsing_context/support/empty.html?pipe=trickle(d10)"
    )
    # Depending on implementation, the `trickle(d10)` page can or can not yet
    # create a new document. Depending on this, `aborted` or `failed` event
    # should be emitted.
    await subscribe_events(
        events=[NAVIGATION_ABORTED_EVENT, NAVIGATION_FAILED_EVENT])

    result = await bidi_session.browsing_context.navigate(
        context=new_context["context"], url=slow_page_url, wait="none"
    )

    events = []

    async def on_event(method, data):
        events.append(data)

    remove_listener_1 = bidi_session.add_event_listener(
        NAVIGATION_ABORTED_EVENT, on_event)
    remove_listener_2 = bidi_session.add_event_listener(NAVIGATION_FAILED_EVENT,
                                                        on_event)

    await bidi_session.browsing_context.close(context=new_context["context"])

    await wait_for_bidi_events(bidi_session, events, 1, timeout=1)

    # Make sure that the navigation failed.
    assert_navigation_info(
        events[0],
        {
            "context": new_context["context"],
            "navigation": result["navigation"],
            "url": slow_page_url,
        },
    )

    remove_listener_1()
    remove_listener_2()


async def test_close_iframe(
    bidi_session,
    subscribe_events,
    inline,
    url,
    new_tab,
    wait_for_event,
    wait_for_future_safe,
):
    iframe_url = inline("<div>foo</div>")
    page_url = inline(f"<iframe src={iframe_url}></iframe")

    # Depending on implementation, the `trickle(d10)` page can or can not yet
    # create a new document. Depending on this, `aborted` or `failed` event
    # should be emitted.
    await subscribe_events(
        events=[NAVIGATION_ABORTED_EVENT, NAVIGATION_FAILED_EVENT])

    await bidi_session.browsing_context.navigate(
        context=new_tab["context"], url=page_url, wait="complete"
    )

    contexts = await bidi_session.browsing_context.get_tree(root=new_tab["context"])
    iframe_context = contexts[0]["children"][0]["context"]

    slow_page_url = url(
        "/webdriver/tests/bidi/browsing_context/support/empty.html?pipe=trickle(d10)"
    )
    # Navigate in the iframe.
    result = await bidi_session.browsing_context.navigate(
        context=iframe_context, url=slow_page_url, wait="none"
    )

    events = []

    async def on_event(method, data):
        events.append(data)

    remove_listener_1 = bidi_session.add_event_listener(
        NAVIGATION_ABORTED_EVENT, on_event)
    remove_listener_2 = bidi_session.add_event_listener(NAVIGATION_FAILED_EVENT,
                                                        on_event)

    # Reload the top context to destroy the iframe.
    await bidi_session.browsing_context.reload(context=new_tab["context"], wait="none")

    await wait_for_bidi_events(bidi_session, events, 1, timeout=1)

    # Make sure that the iframe navigation failed.
    assert_navigation_info(
        events[0],
        {
            "context": iframe_context,
            "navigation": result["navigation"],
            "url": slow_page_url,
        },
    )

    remove_listener_1()
    remove_listener_2()


@pytest.mark.capabilities({"unhandledPromptBehavior": {"beforeUnload": "ignore"}})
async def test_with_beforeunload_prompt(
    bidi_session,
    new_tab,
    wait_for_event,
    wait_for_future_safe,
    url,
    subscribe_events,
    setup_beforeunload_page,
):
    await subscribe_events(
        events=[
            NAVIGATION_FAILED_EVENT,
            NAVIGATION_STARTED_EVENT,
            USER_PROMPT_OPENED_EVENT,
        ]
    )
    await setup_beforeunload_page(new_tab)
    target_url = url("/webdriver/tests/support/html/default.html", domain="alt")

    on_navigation_started = wait_for_event(NAVIGATION_STARTED_EVENT)
    on_prompt_opened = wait_for_event(USER_PROMPT_OPENED_EVENT)

    asyncio.ensure_future(
        bidi_session.browsing_context.navigate(
            context=new_tab["context"], url=target_url, wait="none"
        )
    )

    # Wait for the navigation to start.
    navigation_started_event = await wait_for_future_safe(on_navigation_started)

    # Wait for the prompt to open.
    await wait_for_future_safe(on_prompt_opened)

    on_navigation_failed = wait_for_event(NAVIGATION_FAILED_EVENT)
    # Stay on the page to fail the started navigation.
    await bidi_session.browsing_context.handle_user_prompt(
        context=new_tab["context"], accept=False
    )

    event = await wait_for_future_safe(on_navigation_failed)

    # Make sure that the first navigation failed.
    assert_navigation_info(
        event,
        {
            "context": new_tab["context"],
            "navigation": navigation_started_event["navigation"],
            "url": target_url,
        },
    )
