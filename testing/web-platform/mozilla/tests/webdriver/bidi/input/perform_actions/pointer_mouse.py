import pytest
from webdriver.bidi.modules.input import Actions, get_element_origin

pytestmark = pytest.mark.asyncio


async def test_click_in_display_none_frame(
    bidi_session, top_context, get_element, inline
):
    frame_url = inline(
        """
        <button>click to hide</button>
        <script type="text/javascript">
            const btn = document.querySelector('button');
            btn.addEventListener('click', ev => {
                window.parent.postMessage("test");
            });
        </script>
        """
    )

    url = inline(
        f"""
        <div id="content">
            <iframe src='{frame_url}'></iframe>
        </div>
        <script>
            window.addEventListener("message", ev => {{
                document.querySelector("iframe").style.display = "none";
            }}, false);
        </script>
        """
    )

    await bidi_session.browsing_context.navigate(
        context=top_context["context"],
        url=url,
        wait="complete",
    )

    all_contexts = await bidi_session.browsing_context.get_tree(
        root=top_context["context"]
    )
    frame_context = all_contexts[0]["children"][0]

    button = await get_element("button", context=frame_context)

    actions = Actions()
    (
        actions.add_pointer()
        .pointer_move(x=0, y=0, origin=get_element_origin(button))
        .pointer_down(button=0)
        .pointer_up(button=0)
        .pause(100)
    )

    # Firefox bug:
    # - the click will hide the iframe via display none
    # - the last bit of performActions tries to wait for animationFrame in the
    #   iframe browsing context, but since it's hidden there won't be any and
    #   we never resolve
    # Note that the pause(100) is not strictly necessary to reproduce the issue
    # but it makes it fail consistently. Otherwise it's very much intermittent.
    await bidi_session.input.perform_actions(
        actions=actions, context=frame_context["context"]
    )
