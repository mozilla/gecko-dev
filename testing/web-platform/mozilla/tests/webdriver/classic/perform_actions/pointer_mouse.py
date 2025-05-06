import pytest

pytestmark = pytest.mark.asyncio


async def test_click_in_display_none_frame(session, inline):
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

    session.url = url

    frame = session.find.css("iframe", all=False)
    session.switch_frame(frame)
    button = session.find.css("button", all=False)

    mouse_chain = session.actions.sequence(
        "pointer", "pointer_id", {"pointerType": "mouse"}
    )

    # Firefox bug:
    # - the click will hide the iframe via display none
    # - the last bit of performActions tries to wait for animationFrame in the
    #   iframe browsing context, but since it's hidden there won't be any and
    #   we never resolve
    # Note that the pause(100) is not strictly necessary to reproduce the issue
    # but it makes it fail consistently. Otherwise it's very much intermittent.
    mouse_chain.pointer_move(0, 0, origin=button).pointer_down().pointer_up().pause(
        100
    ).perform()
