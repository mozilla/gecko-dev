def install_addon(session, method, value, temp=False, allow_private_browsing=False):
    arg = {"temporary": temp, "allowPrivateBrowsing": allow_private_browsing}
    arg[method] = value
    return session.transport.send(
        "POST",
        f"/session/{session.session_id}/moz/addon/install",
        arg,
    )


def uninstall_addon(session, addon_id):
    return session.transport.send(
        "POST",
        "/session/{session_id}/moz/addon/uninstall".format(
            session_id=session.session_id
        ),
        {"id": addon_id},
    )
