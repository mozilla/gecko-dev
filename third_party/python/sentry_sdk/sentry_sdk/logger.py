# NOTE: this is the logger sentry exposes to users, not some generic logger.
import functools
import time
from typing import Any

from sentry_sdk import get_client
from sentry_sdk.utils import safe_repr

OTEL_RANGES = [
    # ((severity level range), severity text)
    # https://opentelemetry.io/docs/specs/otel/logs/data-model
    ((1, 4), "trace"),
    ((5, 8), "debug"),
    ((9, 12), "info"),
    ((13, 16), "warn"),
    ((17, 20), "error"),
    ((21, 24), "fatal"),
]


def _capture_log(severity_text, severity_number, template, **kwargs):
    # type: (str, int, str, **Any) -> None
    client = get_client()

    attrs = {
        "sentry.message.template": template,
    }  # type: dict[str, str | bool | float | int]
    if "attributes" in kwargs:
        attrs.update(kwargs.pop("attributes"))
    for k, v in kwargs.items():
        attrs[f"sentry.message.parameter.{k}"] = v

    attrs = {
        k: (
            v
            if (
                isinstance(v, str)
                or isinstance(v, int)
                or isinstance(v, bool)
                or isinstance(v, float)
            )
            else safe_repr(v)
        )
        for (k, v) in attrs.items()
    }

    # noinspection PyProtectedMember
    client._capture_experimental_log(
        {
            "severity_text": severity_text,
            "severity_number": severity_number,
            "attributes": attrs,
            "body": template.format(**kwargs),
            "time_unix_nano": time.time_ns(),
            "trace_id": None,
        },
    )


trace = functools.partial(_capture_log, "trace", 1)
debug = functools.partial(_capture_log, "debug", 5)
info = functools.partial(_capture_log, "info", 9)
warning = functools.partial(_capture_log, "warn", 13)
error = functools.partial(_capture_log, "error", 17)
fatal = functools.partial(_capture_log, "fatal", 21)


def _otel_severity_text(otel_severity_number):
    # type: (int) -> str
    for (lower, upper), severity in OTEL_RANGES:
        if lower <= otel_severity_number <= upper:
            return severity

    return "default"


def _log_level_to_otel(level, mapping):
    # type: (int, dict[Any, int]) -> tuple[int, str]
    for py_level, otel_severity_number in sorted(mapping.items(), reverse=True):
        if level >= py_level:
            return otel_severity_number, _otel_severity_text(otel_severity_number)

    return 0, "default"
