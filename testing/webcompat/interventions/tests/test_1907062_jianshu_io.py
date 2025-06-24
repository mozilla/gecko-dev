import pytest

URL = "https://www.jianshu.com/p/68ea4f004c8a"


IMG_CSS = "img[data-original-src='//upload-images.jianshu.io/upload_images/27952728-939c10fd072ed9e3.png']"
BROKEN_CSS = f".image-view-error {IMG_CSS}"


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.make_preload_script(
        f"""
      window.__promise = new Promise(done => {{
        document.documentElement.addEventListener("load", ({{target}}) => {{
          if (target?.matches("{IMG_CSS}")) {{
            done(true);
          }}
        }}, true);
      }});
    """
    )
    await client.navigate(URL, wait="none")
    client.scroll_into_view(client.await_css(IMG_CSS))
    assert client.execute_async_script("window.__promise.then(arguments[0])")


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL, wait="none")
    client.scroll_into_view(client.await_css(IMG_CSS))
    assert client.await_css(BROKEN_CSS, timeout=40)
