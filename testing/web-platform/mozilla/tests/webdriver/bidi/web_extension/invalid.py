import pytest
from webdriver.bidi import error

pytestmark = pytest.mark.asyncio


@pytest.mark.parametrize("value", ["", 42, [], {}])
async def test_params_moz_permanent_invalid_type(bidi_session, extension_data, value):
    kwargs = {"moz:permanent": value}

    with pytest.raises(error.InvalidArgumentException):
        await bidi_session.web_extension.install(
            extension_data={
                "type": "base64",
                "value": extension_data["base64"],
            },
            **kwargs
        )
