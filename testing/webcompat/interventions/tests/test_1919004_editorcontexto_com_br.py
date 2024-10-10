import pytest

URL = "https://www.editoracontexto.com.br/"
TOP_BAR_CSS = ".ui.container.QDheader"


def top_bar_is_at_top(client):
    top_bar = client.await_css(TOP_BAR_CSS, is_displayed=True)
    assert top_bar
    return client.execute_script(
        """
        return arguments[0].getBoundingClientRect().top == 0;
    """,
        top_bar,
    )


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert top_bar_is_at_top(client)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert not top_bar_is_at_top(client)
