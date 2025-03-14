import pytest

URL = "https://zengzhi.ipmph.com/#/bookPreview?eyJlbmNyeXB0IjoiRDB6eGU1dnNHMVJMVXBXMWlxQnMxc2EvR00zL2dIVTZPVDVCQy9UU3ZxRmsvSjNkQkwrVmxBdU54K1lYZGE4NjNIS1JkM2U3TkRYWGQycTV4dG1xRWIxMVloMmVuVEw5SlU1c3NmS2xMcDhYZ1JqRjF5WGl6SEQrVWRwNC9hL0VSU0VMcUZCcTM0YWVRODNQRHB1TXJWd3RERUVvN1lTVlNodXVpUFViVnM4PSIsIml2Ijp7IndvcmRzIjpbMTE5MjI3OTM5LDE4ODE0MDk1NCwyODMyOTMwMTE3LDE0MzQyMjIyNzBdLCJzaWdCeXRlcyI6MTZ9fQ=="

SUPPORTED_CSS = "video#myVideo_html5_api"
UNSUPPORTED_CSS = ".noflash"
IFRAME_CSS = "iframe[src*=videoM3u8]"


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    client.switch_frame(client.await_css(IFRAME_CSS, timeout=120))
    client.await_css(SUPPORTED_CSS, is_displayed=True, timeout=60)
    assert not client.find_css(UNSUPPORTED_CSS, is_displayed=True)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    client.await_css(UNSUPPORTED_CSS, is_displayed=True, timeout=60)
    assert not client.find_css(SUPPORTED_CSS, is_displayed=True)
