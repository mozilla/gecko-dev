import pytest

URL = "https://personalizedplates.revenue.tn.gov/#/"
TYPE_OPTION_XPATH = "//select[@class='swal2-select']/option[2]"
CONFIRM_CSS = ".swal2-confirm"
PLATE_CSS = ".plateCont"
PLATE_DETAILS_CSS = "#plateDetails"


async def select_plate(client):
    client.await_xpath(TYPE_OPTION_XPATH).click()
    client.await_css(CONFIRM_CSS).click()
    client.await_css(PLATE_CSS).click()


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    await select_plate(client)
    dialog = client.await_css(PLATE_DETAILS_CSS)
    assert client.is_displayed(dialog)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    await select_plate(client)
    assert not client.find_css(PLATE_DETAILS_CSS)
