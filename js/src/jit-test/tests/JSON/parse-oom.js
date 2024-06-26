// |jit-test| --enable-json-parse-with-source; skip-if: !JSON.hasOwnProperty('isRawJSON')

oomTest(() => {
    JSON.parse('{"a": [1, {"b":2}, "7"], "c": 8}');
});

oomTest(() => {
    JSON.parse('{"a": [1, {"b":2}, "7"], "c": 8, "d": {"e": 9}}', (k, v, {source}) => v);
});
