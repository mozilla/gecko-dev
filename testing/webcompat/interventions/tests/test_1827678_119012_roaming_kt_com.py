import pytest

URL = "https://roaming.kt.com/esim/eng/reserve.asp"

SUPPORTED_ALERT = "collection and use of private information is necessary"
UNSUPPORTED_ALERT = "browser is not supported"


async def wait_for_alert(client, alert):
    # the page loads for a long time; let's not wait unnecessarily
    await client.make_preload_script(
        """
       window.alert = msg => {
          console.error(msg);
       };
       const int = setInterval(() => {
          if (window.esim_pay) {
              clearInterval(int);
              window.esim_pay();
          }
       }, 100);
    """
    )
    await client.navigate(URL, wait="none", await_console_message=alert)


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await wait_for_alert(client, SUPPORTED_ALERT)


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await wait_for_alert(client, UNSUPPORTED_ALERT)
