import pytest

URL = "https://forms.ontariodrugbenefit.ca/portal/EFormUpdate.aspx?usid=e00cfc92-af3c-4de2-b355-83cc83a62aa3&eFormId=600a8fe9-059f-4688-a358-3e9af217b5bc&DomainID=e98ae7fc-3a6c-42aa-8845-b6b165a4dd9c"

IFRAME_CSS = "#ifload0"
SUPPORTED_CSS = "#btn_tdp_apply"
UNSUPPORTED_CSS = "a[href*='google.com/chrome']"


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL, wait="none")
    client.switch_to_frame(client.await_css(IFRAME_CSS))
    assert client.await_css(SUPPORTED_CSS, is_displayed=True)
    assert not client.find_css(UNSUPPORTED_CSS, is_displayed=True)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL, wait="none")
    client.switch_to_frame(client.await_css(IFRAME_CSS))
    assert client.await_css(UNSUPPORTED_CSS, is_displayed=True)
    assert not client.find_css(SUPPORTED_CSS, is_displayed=True)
