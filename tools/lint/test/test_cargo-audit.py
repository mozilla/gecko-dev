import mozunit

LINTER = "cargo-audit"


def is_error(r):
    if r.level == "error":
        return True
    return False


def verify_vulnerabilities(vulnerabilities, results):
    for result in results:
        found = None
        for index, vulnerability in enumerate(vulnerabilities):
            if vulnerability[1] == result.level and vulnerability[0] in result.message:
                found = index
                break
        if found is not None:
            del vulnerabilities[found]

    return vulnerabilities


def test_lint_cargo_audit_errors(lint, paths):
    error = "error"
    warning = "warning"

    test_file = "error.lock"
    results = lint(paths(test_file))
    assert len(results) == 6

    expected_vulnerabilities = [
        ("RUSTSEC-2019-0014", error),
        ("RUSTSEC-2020-0144", warning),
        ("RUSTSEC-2020-0073", warning),
        ("RUSTSEC-2022-0004", error),
        ("yanked version of libc", warning),  # This one lacks an ID
    ]
    assert verify_vulnerabilities(expected_vulnerabilities, results) == []

    for result in results:
        assert result.relpath == test_file


def test_lint_cargo_audit_clean(lint, paths):
    test_file = "clean.lock"
    results = lint(paths(test_file))
    assert len(results) == 0


if __name__ == "__main__":
    mozunit.main()
