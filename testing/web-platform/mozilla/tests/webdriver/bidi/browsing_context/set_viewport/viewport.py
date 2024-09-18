import asyncio

import pytest
from tests.bidi import get_viewport_dimensions

pytestmark = pytest.mark.asyncio


@pytest.mark.parametrize(
    "viewport",
    [
        {"width": 10000000, "height": 100},
        {"width": 100, "height": 10000000},
        {"width": 10000000, "height": 10000000},
    ],
    ids=["maximal width", "maximal height", "maximal width and height"],
)
async def test_params_viewport_max_value(bidi_session, new_tab, viewport):
    await bidi_session.browsing_context.set_viewport(
        context=new_tab["context"], viewport=viewport
    )


@pytest.mark.parametrize("type_hint", ["tab", "window"])
async def test_when_context_created(
    bidi_session, wait_for_event, wait_for_future_safe, type_hint
):
    test_viewport = {"width": 499, "height": 599}

    await bidi_session.session.subscribe(events=["browsingContext.contextCreated"])
    on_context_created = wait_for_event("browsingContext.contextCreated")

    # Save the task to await for it later.
    task = asyncio.create_task(
        bidi_session.browsing_context.create(type_hint=type_hint)
    )

    context_info = await wait_for_future_safe(on_context_created)

    # Bug 1918287 - Don't fail the command when the initial about:blank page is loaded
    await bidi_session.browsing_context.set_viewport(
        context=context_info["context"], viewport=test_viewport
    )

    new_tab = await task

    assert await get_viewport_dimensions(bidi_session, new_tab) == test_viewport

    await bidi_session.browsing_context.close(context=new_tab["context"])
