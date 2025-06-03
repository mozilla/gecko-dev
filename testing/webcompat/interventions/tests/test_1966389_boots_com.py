import pytest

URL = "https://www.boots.com/opticiansappointments"
FRAME_CSS = "#boots-opticians"
ELEMS_CSS = ".Address input, .RebookAddress input, .Age select, .AppointmentType select"


async def are_key_elements_same_width(client):
    client.set_screen_size(1224, 500)
    await client.navigate(URL, wait="none")
    client.switch_to_frame(client.await_css(FRAME_CSS, is_displayed=True))
    elems = client.await_css(ELEMS_CSS, is_displayed=True, all=True)
    unique_widths = client.execute_script(
        "return arguments[0].map(e => e.getBoundingClientRect().width)", elems
    )
    return len(list(set(unique_widths))) == 1


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await are_key_elements_same_width(client)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert not await are_key_elements_same_width(client)
