from tests.support.asserts import assert_success
from tests.support.helpers import is_element_in_viewport


def element_send_keys(session, element, text):
    return session.transport.send(
        "POST",
        f"/session/{session.session_id}/element/{element.id}/value",
        {"text": text},
    )


def test_option_select_container_outside_of_scrollable_viewport(session, inline):
    session.url = inline(
        """
        <select style="margin-top: 102vh;">
          <option value="foo">foo</option>
          <option value="bar" id="bar">bar</option>
        </select>
    """
    )
    element = session.find.css("option#bar", all=False)
    select = session.find.css("select", all=False)

    response = element_send_keys(session, element, "bar")
    assert_success(response)

    assert is_element_in_viewport(session, select)
    assert is_element_in_viewport(session, element)


def test_option_stays_outside_of_scrollable_viewport(session, inline):
    session.url = inline(
        """
        <select multiple style="height: 105vh; margin-top: 100vh;">
          <option value="foo" id="foo" style="height: 100vh;">foo</option>
          <option value="bar" id="bar" style="background-color: yellow;">bar</option>
        </select>
    """
    )
    select = session.find.css("select", all=False)
    option_bar = session.find.css("option#bar", all=False)

    response = element_send_keys(session, option_bar, "bar")
    assert_success(response)

    assert is_element_in_viewport(session, select)
    assert is_element_in_viewport(session, option_bar)
