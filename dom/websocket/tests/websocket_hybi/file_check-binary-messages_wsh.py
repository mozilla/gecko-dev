from mod_pywebsocket import msgutil


def web_socket_do_extra_handshake(request):
    pass  # Always accept.


def web_socket_transfer_data(request):
    expected_messages = ["Hello, world!", "", all_distinct_bytes()]

    for test_number, expected_message in enumerate(expected_messages):
        expected_message = expected_message.encode("latin-1")
        message = msgutil.receive_message(request)
        if message == expected_message:
            msgutil.send_message(request, f"PASS: Message #{test_number:d}.")
        else:
            msgutil.send_message(
                request,
                f"FAIL: Message #{test_number:d}: Received unexpected message: {message!r}",
            )


def all_distinct_bytes():
    return "".join([chr(i) for i in range(256)])
