import pytest

MOBILE_ORIGIN = "https://m.rbi.org.in"
REDIRECT_DESKTOP_ORIGIN = "https://rbi.org.in"


def get_origin(client):
    return client.session.execute_script(
        """
       return location.origin;
    """
    )


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(MOBILE_ORIGIN, wait="complete", timeout=20)
    assert get_origin(client) == MOBILE_ORIGIN


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(MOBILE_ORIGIN, wait="complete", timeout=20)
    assert get_origin(client) == REDIRECT_DESKTOP_ORIGIN
