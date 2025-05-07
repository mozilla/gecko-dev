import asyncio

import pytest

URL = "https://g1.globo.com/jogos/caca-palavras/"

FULLSIZE_BUTTON_CSS = ".full-size-button"
INTRO_BUTTON_CSS = "button.intro-button"
TUTORIAL_BUTTON_CSS = ".tutorial button"
TUTORIAL_OVERLAY_CSS = ".drawer-overlay"
CLIPPED_BUTTON_CSS = ".subheader .dropdown button:has(svg)"
FIRST_LETTER_CSS = ".letter-box svg g g:first-child"
SELECTION_CSS = ".letter-box svg g path"


async def are_buttons_clipped_or_selection_is_misaligned(client):
    client.set_screen_size(300, 500)
    await client.navigate(URL, wait="none")

    fullsize, intro = client.await_first_element_of(
        [
            client.css(FULLSIZE_BUTTON_CSS),
            client.css(INTRO_BUTTON_CSS),
        ],
        is_displayed=True,
    )
    if fullsize:
        client.soft_click(fullsize)
        client.soft_click(client.await_css(INTRO_BUTTON_CSS, is_displayed=True))
    else:
        client.soft_click(intro)

    client.soft_click(client.await_css(TUTORIAL_BUTTON_CSS, is_displayed=True))
    client.await_element_hidden(client.css(TUTORIAL_OVERLAY_CSS))

    # check that the rightmost button isn't clipped away (isn't just the bgcolor)
    clipped_button = client.await_css(CLIPPED_BUTTON_CSS, is_displayed=True)
    if client.is_one_solid_color(clipped_button):
        return True

    letter = client.await_css(FIRST_LETTER_CSS, is_displayed=True)
    coords = client.get_element_screen_position(letter)
    coords = [coords[0] + 10, coords[1] + 10]
    await client.send_apz_mouse_event("down", coords=coords)
    for i in range(25):
        await asyncio.sleep(0.01)
        coords[0] += 5
        await client.send_apz_mouse_event("move", coords=coords)

    selection = client.await_css(SELECTION_CSS, is_displayed=True)
    return client.execute_script(
        """
        // The letter's CSS boxes are always bigger than their text, and also the selection.
        // we check that it's more or less centered (possibly off by a pixel).
        const letter = arguments[0].getBoundingClientRect();
        const selection = arguments[1].getBoundingClientRect();
        const halfHeightDiff = ((letter.height - selection.height) / 2) + 1; // can be off by one
        return Math.abs(selection.bottom - letter.bottom) > halfHeightDiff ||
               Math.abs(selection.top - letter.top) > halfHeightDiff;
    """,
        letter,
        selection,
    )


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert not await are_buttons_clipped_or_selection_is_misaligned(client)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert await are_buttons_clipped_or_selection_is_misaligned(client)
