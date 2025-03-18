import asyncio

import pytest

URL = "https://rosasthai.com/locations"

NEAR_ME_CSS = "#near-me"
LOADING_CSS = ".loading-location"


async def is_geoloc_checked(client):
    await client.make_preload_script(
        """
        const { prototype } = EventTarget;
        const { addEventListener } = prototype;
        prototype.addEventListener = function (type, b, c, d) {
          if (!b?.toString().includes("Delay loading immediately to prevent booking popup from breaking")) {
            return addEventListener.call(this, type, b, c, d);
          }
        };
        navigator.geolocation.getCurrentPosition = function() {
          window.geolocCalled = true;
        };
    """
    )
    await client.navigate(URL)
    client.await_css(NEAR_ME_CSS, is_displayed=True).click()
    await asyncio.sleep(2)
    return client.execute_script("return !!window.geolocCalled")


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await is_geoloc_checked(client)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert not await is_geoloc_checked(client)
