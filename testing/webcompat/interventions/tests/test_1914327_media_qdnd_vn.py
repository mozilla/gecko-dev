import pytest

URL = "https://media.qdnd.vn/phim-truyen-thong-tai-lieu/nhung-hinh-anh-dau-tien-trong-du-an-phim-ve-bo-doi-cu-ho-nam-2024-59811"
PLAY_BUTTON_CSS = "#hpcplayer .media-icon-play"
ERROR_MSG_CSS = ".jw-error-msg.jw-reset"
EXPECTED_TYPE = "application/vnd.apple.mpegurl"


async def calls_canPlayType(client, type):
    await client.make_preload_script(
        """
      const proto = HTMLMediaElement.prototype;
      const def = Object.getOwnPropertyDescriptor(proto, "canPlayType");
      const orig = def.value;
      window.__cpts = [];
      def.value = function(type) {
        window.__cpts.push(type);
        return orig.apply(this, arguments);
      }
      Object.defineProperty(proto, "canPlayType", def);
    """
    )
    await client.navigate(URL)
    play, err = client.await_first_element_of(
        [
            client.css(PLAY_BUTTON_CSS),
            client.css(ERROR_MSG_CSS),
        ],
        is_displayed=True,
    )
    if play:
        play.click()
        client.await_css(ERROR_MSG_CSS, is_displayed=True)
    return client.execute_script(
        """
      return window.__cpts.includes(arguments[0]);
      """,
        type,
    )


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await calls_canPlayType(client, EXPECTED_TYPE)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert not await calls_canPlayType(client, EXPECTED_TYPE)
