import pytest

URL = "https://appointments.mpi.mb.ca/mpi/main/ReserveTime/StartReservation?pageId=01aa10ce-34d6-45ec-b861-86c8811239ca&buttonId=5367449e-20b9-470a-a022-f9d4df067cd6&culture=en&uiCulture=en"

CALENDAR_CSS = "#rs-calendar"
ERROR_MSG = "getWeekInfo is not a function"


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL, wait="none")
    assert client.await_css(CALENDAR_CSS, is_displayed=True)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL, await_console_message=ERROR_MSG)
