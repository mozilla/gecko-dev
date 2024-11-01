import pytest

URL = "https://jelly.jd.com/article/61074ef0800f53019cb4dac0"

UNSUPPORTED_TEXT = "前往下载 Chrome 浏览器"


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL, wait="load")
    assert not client.find_text(UNSUPPORTED_TEXT)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL, wait="load")
    assert client.await_text(UNSUPPORTED_TEXT)
