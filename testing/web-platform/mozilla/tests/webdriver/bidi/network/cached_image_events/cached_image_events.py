import pytest
from tests.bidi.network import (
    BEFORE_REQUEST_SENT_EVENT,
    IMAGE_RESPONSE_BODY,
    RESPONSE_COMPLETED_EVENT,
    RESPONSE_STARTED_EVENT,
    assert_before_request_sent_event,
    assert_response_event,
    get_cached_url,
)
from tests.support.sync import AsyncPoll


@pytest.mark.asyncio
async def test_page_with_cached_image(
    bidi_session,
    url,
    inline,
    setup_network_test,
    top_context,
):
    network_events = await setup_network_test(
        events=[
            BEFORE_REQUEST_SENT_EVENT,
            RESPONSE_COMPLETED_EVENT,
            RESPONSE_STARTED_EVENT,
        ]
    )
    before_request_sent_events = network_events[BEFORE_REQUEST_SENT_EVENT]
    response_started_events = network_events[RESPONSE_STARTED_EVENT]
    response_completed_events = network_events[RESPONSE_COMPLETED_EVENT]

    # Small assert helper to assert events in the various event arrays
    def assert_events_at(index, expected_request, expected_response):
        assert_before_request_sent_event(
            before_request_sent_events[index],
            expected_request=expected_request,
        )
        assert_response_event(
            response_started_events[index],
            expected_request=expected_request,
            expected_response=expected_response,
        )
        assert_response_event(
            response_completed_events[index],
            expected_request=expected_request,
            expected_response=expected_response,
        )

    cached_image_url = url(get_cached_url("img/png", IMAGE_RESPONSE_BODY))
    page_with_cached_image = inline(
        f"""
        <body>
            test page with cached image
            <img src="{cached_image_url}">
        </body>
        """,
    )

    await bidi_session.browsing_context.navigate(
        context=top_context["context"],
        url=page_with_cached_image,
        wait="complete",
    )

    # Expect two events, one for the document and one for the image.
    wait = AsyncPoll(bidi_session, timeout=2)
    await wait.until(lambda _: len(response_completed_events) >= 2)
    assert len(before_request_sent_events) == 2
    assert len(response_started_events) == 2
    assert len(response_completed_events) == 2

    assert_events_at(
        0,
        expected_request={"method": "GET", "url": page_with_cached_image},
        expected_response={"url": page_with_cached_image, "fromCache": False},
    )
    assert_events_at(
        1,
        expected_request={"method": "GET", "url": cached_image_url},
        expected_response={"url": cached_image_url, "fromCache": False},
    )

    # Reload the page.
    await bidi_session.browsing_context.reload(context=top_context["context"])

    # Expect two events, one for the document and one for the image.
    wait = AsyncPoll(bidi_session, timeout=2)
    await wait.until(lambda _: len(response_completed_events) >= 4)
    assert len(before_request_sent_events) == 4
    assert len(response_started_events) == 4
    assert len(response_completed_events) == 4

    assert_events_at(
        2,
        expected_request={"method": "GET", "url": page_with_cached_image},
        expected_response={"url": page_with_cached_image, "fromCache": False},
    )
    assert_events_at(
        3,
        expected_request={"method": "GET", "url": cached_image_url},
        expected_response={"url": cached_image_url, "fromCache": True},
    )

    page_with_2_cached_images = inline(
        f"""
        <body>
            test page with 2 cached images
            <img src="{cached_image_url}">
            <img src="{cached_image_url}">
        </body>
        """,
    )

    await bidi_session.browsing_context.navigate(
        context=top_context["context"],
        url=page_with_2_cached_images,
        wait="complete",
    )

    # Only expect one event for the document here, as Firefox should reuse the
    # existing images from the previous document load.
    wait = AsyncPoll(bidi_session, timeout=2)
    await wait.until(lambda _: len(response_completed_events) >= 5)
    assert len(before_request_sent_events) == 5
    assert len(response_started_events) == 5
    assert len(response_completed_events) == 5

    assert_events_at(
        4,
        expected_request={"method": "GET", "url": page_with_2_cached_images},
        expected_response={"url": page_with_2_cached_images, "fromCache": False},
    )
