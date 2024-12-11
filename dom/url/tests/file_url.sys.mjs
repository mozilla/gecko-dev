export function checkFromESM(ok, is) {
  var url = new URL("https://www.example.com");
  is(url.href, "https://www.example.com/", "ESM should have URL");

  var url2 = new URL("/foobar", url);
  is(
    url2.href,
    "https://www.example.com/foobar",
    "ESM should have URL - based on another URL"
  );

  var blob = new Blob(["a"]);
  url = URL.createObjectURL(blob);
  ok(url, "URL is created!");

  var u = new URL(url);
  ok(u, "URL created");
  is(u.origin, "null", "Url doesn't have an origin if created in a ESM");

  URL.revokeObjectURL(url);
  ok(true, "URL is revoked");
}
