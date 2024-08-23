import pytest

pytestmark = pytest.mark.asyncio


@pytest.mark.parametrize(
    "name, original_value, modified_value",
    [
        ("Content-Encoding", "foo", "bar"),
        ("CONTENT-LENGTH", "10", "20"),
        ("content-type", "text/html", "text/javascript"),
        ("trailer", "foo", "bar"),
        ("transfer-encoding", "foo", "bar"),
    ],
)
async def test_immutable_headers_do_not_throw(
    setup_blocked_request,
    subscribe_events,
    bidi_session,
    top_context,
    wait_for_event,
    wait_for_future_safe,
    url,
    name,
    original_value,
    modified_value,
):
    request = await setup_blocked_request(
        phase="responseStarted",
        blocked_url=url(
            f"/webdriver/tests/support/http_handlers/headers.py?header={name}:{original_value}"
        ),
    )
    await subscribe_events(
        events=[
            "network.responseCompleted",
        ]
    )

    on_response_completed = wait_for_event("network.responseCompleted")

    header = {"name": name, "value": {"type": "string", "value": modified_value}}

    await bidi_session.network.continue_response(
        request=request,
        headers=[header],
    )

    response_completed_event = await wait_for_future_safe(on_response_completed)

    # Assert that the immutable response header was not modified, but the
    # continueResponse command still succeeded.
    # After Bug 1914351 will be fixed, this test should immediately fail, and
    # be updated to check that the header was actually modified.
    response_headers = response_completed_event["response"]["headers"]
    immutable_headers = [
        h for h in response_headers if h["name"].lower() == name.lower()
    ]
    assert len(immutable_headers) == 1
    assert immutable_headers[0]["value"]["value"] == original_value
