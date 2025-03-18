import mozunit

LINTER = "lintpref"


def test_lintpref(lint, paths):
    results = lint(paths())
    assert len(results) == 3

    assert results[0].level == "error"
    assert 'pref("dom.webidl.test1", true);' in results[0].message
    assert "bad.js" in results[0].relpath
    assert results[0].lineno == 2

    assert results[1].level == "error"
    assert results[1].message == 'pref("browser.theme.content-theme", 2, sticky);\n'
    assert results[1].lineno == 8

    assert results[2].level == "error"
    assert results[2].message == 'pref("browser.theme.toolbar-theme", 2, locked);\n'
    assert results[2].lineno == 11


if __name__ == "__main__":
    mozunit.main()
