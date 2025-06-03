import pytest

URL = "https://teikiyoyaku.tokyu.co.jp/trw/input?kbn=1&languageCode=ja"


async def is_fastclick_active(client):
    async with client.ensure_fastclick_activates():
        await client.navigate(URL)
        # The service is down overnight, making it harder to test in other time zones,
        # but we can just activate FastClick on our own select.
        select = client.execute_script(
            """
            const s = document.createElement("select");
            s.innerHTML = "<option>1</option>";
            document.body.appendChild(s);
            FastClick.attach(document.body);
            return s;
        """
        )
        return client.test_for_fastclick(select)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert not await is_fastclick_active(client)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert await is_fastclick_active(client)
