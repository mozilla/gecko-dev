import pytest

URL = "https://www.unimedlitoral.com.br/agendaonline/index.php?c=Authentication"
FILE_NOT_FOUND_MSG = "File not found."

# Note this page is now a 404, but this appears to not be what the site intends,
# so in case it ever returns we should confirm whether Firefox is still blocked.


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert client.await_text(FILE_NOT_FOUND_MSG)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert client.await_text(FILE_NOT_FOUND_MSG)
