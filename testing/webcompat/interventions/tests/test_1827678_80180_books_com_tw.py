import pytest

URL = "https://viewer-ebook.books.com.tw/viewer/epub/?book_uni_id=0010763244_reflowable_trial"

SUPPORTED_CSS = ".epub-viewer"
UNSUPPORTED_TEXT = "Get Chrome"


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert client.await_css(SUPPORTED_CSS)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.disable_window_alert()
    await client.navigate(URL)
    assert client.await_text(UNSUPPORTED_TEXT)
