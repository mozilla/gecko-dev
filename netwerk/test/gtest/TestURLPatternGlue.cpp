/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "gtest/MozGTestBench.h"  // For MOZ_GTEST_BENCH
#include "mozilla/net/URLPatternGlue.h"
#include "mozilla/net/urlpattern_glue.h"

using namespace mozilla::net;

template <typename T>
using Optional = mozilla::Maybe<T>;

// pattern construction from string
TEST(TestURLPatternGlue, PatternFromString)
{
  nsCString str(":café://:foo");
  UrlpPattern pattern{};
  UrlpOptions options = {.ignore_case = false};
  nsCString tmp("https://example.com/");
  bool res = urlp_parse_pattern_from_string(&str, &tmp, options, &pattern);
  ASSERT_TRUE(res);
  ASSERT_TRUE(pattern._0);
}

TEST(TestURLPatternGlue, PatternFromStringOnlyPathname)
{
  nsCString str("/foo/thing");
  UrlpPattern pattern{};
  UrlpOptions options = {.ignore_case = false};
  bool res = urlp_parse_pattern_from_string(&str, nullptr, options, &pattern);
  ASSERT_FALSE(res);
  ASSERT_FALSE(pattern._0);
}

UrlpInit CreateInit(const nsCString& protocol, const nsCString& username,
                    const nsCString& password, const nsCString& hostname,
                    const nsCString& port, const nsCString& pathname,
                    const nsCString& search, const nsCString& hash,
                    const nsCString& baseUrl) {
  return UrlpInit{
      .protocol = CreateMaybeString(protocol, !protocol.IsEmpty()),
      .username = CreateMaybeString(username, !username.IsEmpty()),
      .password = CreateMaybeString(password, !password.IsEmpty()),
      .hostname = CreateMaybeString(hostname, !hostname.IsEmpty()),
      .port = CreateMaybeString(port, !port.IsEmpty()),
      .pathname = CreateMaybeString(pathname, !pathname.IsEmpty()),
      .search = CreateMaybeString(search, !search.IsEmpty()),
      .hash = CreateMaybeString(hash, !hash.IsEmpty()),
      .base_url = CreateMaybeString(baseUrl, !baseUrl.IsEmpty()),
  };
}

UrlpInit CreateSimpleInit(const nsCString& protocol, const nsCString& hostname,
                          const nsCString& pathname) {
  return CreateInit(protocol, ""_ns, ""_ns, hostname, ""_ns, pathname, ""_ns,
                    ""_ns, ""_ns);
}

UrlpInit CreateInit(const char* protocol, const char* username,
                    const char* password, const char* hostname,
                    const char* port, const char* pathname, const char* search,
                    const char* hash, const char* base = "") {
  return CreateInit(nsCString(protocol), nsCString(username),
                    nsCString(password), nsCString(hostname), nsCString(port),
                    nsCString(pathname), nsCString(search), nsCString(hash),
                    nsCString(base));
}

// pattern construction from init
TEST(TestURLPatternGlue, PatternFromInit)
{
  UrlpPattern pattern{};
  UrlpOptions options = {.ignore_case = false};
  UrlpInit init = CreateSimpleInit("https"_ns, "example.com"_ns, "/"_ns);
  printf("created init\n");
  bool res = urlp_parse_pattern_from_init(&init, options, &pattern);
  printf("parsed pattern from init\n");
  ASSERT_TRUE(res);
  ASSERT_TRUE(pattern._0);

  auto proto = UrlpGetProtocol(pattern);
  ASSERT_EQ(proto, "https"_ns);
}

TEST(TestURLPatternGlue, PatternFromInitOnlyPathname)
{
  UrlpPattern pattern{};
  UrlpOptions options = {.ignore_case = false};
  UrlpInit init = CreateSimpleInit(""_ns, ""_ns, "/foo/thing"_ns);
  bool res = urlp_parse_pattern_from_init(&init, options, &pattern);
  ASSERT_TRUE(res);
  ASSERT_TRUE(pattern._0);

  auto proto = UrlpGetProtocol(pattern);
  ASSERT_EQ(proto, nsCString("*"));
  auto host = UrlpGetHostname(pattern);
  ASSERT_EQ(host, nsCString("*"));
  auto path = UrlpGetPathname(pattern);
  ASSERT_EQ(path, nsCString("/foo/thing"));

  Optional<nsAutoCString> execBaseUrl;  // None
  UrlpInput input = CreateUrlpInput(init);
  Optional<UrlpResult> r = UrlpPatternExec(pattern, input, execBaseUrl);
  ASSERT_TRUE(r.isSome());
  ASSERT_TRUE(r->mProtocol.isSome());
  ASSERT_EQ(r->mProtocol.value().mInput, nsCString(""));
  ASSERT_TRUE(r->mPathname.isSome());
  ASSERT_EQ(r->mPathname.value().mInput, "/foo/thing"_ns);
}

// pattern getters
TEST(TestURLPatternGlue, UrlPatternGetters)
{
  UrlpPattern pattern{};
  UrlpOptions options = {.ignore_case = false};

  UrlpInit init =
      CreateInit("https"_ns, "user"_ns, "passw"_ns, "example.com"_ns, "66"_ns,
                 "/"_ns, "find"_ns, "anchor"_ns, ""_ns);
  bool rv = urlp_parse_pattern_from_init(&init, options, &pattern);
  ASSERT_TRUE(rv);
  ASSERT_TRUE(pattern._0);

  nsAutoCString res;
  res = UrlpGetProtocol(pattern);
  ASSERT_EQ(res, nsCString("https"));
  res = UrlpGetUsername(pattern);
  ASSERT_EQ(res, nsCString("user"));
  res = UrlpGetPassword(pattern);
  ASSERT_EQ(res, nsCString("passw"));
  res = UrlpGetHostname(pattern);
  ASSERT_EQ(res, nsCString("example.com"));
  res = UrlpGetPort(pattern);
  ASSERT_EQ(res, nsCString("66"));
  res = UrlpGetPathname(pattern);
  ASSERT_EQ(res, nsCString("/"));
  res = UrlpGetSearch(pattern);
  ASSERT_EQ(res, nsCString("find"));
  res = UrlpGetHash(pattern);
  ASSERT_EQ(res, nsCString("anchor"));
  // neither lib or quirks URLPattern has base_url so nothing to check
}

// UrlPattern.test() from_init
TEST(TestURLPatternGlue, UrlPatternTestInit)
{
  // check basic literal matching (minimal fields)
  {
    UrlpPattern pattern{};
    UrlpOptions options = {.ignore_case = false};
    UrlpInit init = CreateSimpleInit("https"_ns, "example.com"_ns, "/"_ns);
    bool res = urlp_parse_pattern_from_init(&init, options, &pattern);
    ASSERT_TRUE(res);

    Optional<nsCString> base;
    {  // path not fixed up (?)
      auto test = CreateUrlpInput(
          CreateSimpleInit("https"_ns, "example.com"_ns, ""_ns));
      ASSERT_FALSE(UrlpPatternTest(pattern, test, base));
    }
    {
      auto test = CreateUrlpInput(
          CreateSimpleInit("https"_ns, "example.com"_ns, "/"_ns));
      ASSERT_TRUE(UrlpPatternTest(pattern, test, base));
    }
    {  // unspecified user and password is fine
      auto test = CreateUrlpInput(
          CreateInit("https", "user", "pass", "example.com", "", "/", "", ""));
      ASSERT_TRUE(UrlpPatternTest(pattern, test, base));
    }
    {  // unspecified port is fine
      auto test = CreateUrlpInput(
          CreateInit("https", "", "", "example.com", "444", "/", "", ""));
      ASSERT_TRUE(UrlpPatternTest(pattern, test, base));
    }
    {  // unspecified search is fine
      auto test = CreateUrlpInput(
          CreateInit("https", "", "", "example.com", "", "/", "thisok", ""));
      ASSERT_TRUE(UrlpPatternTest(pattern, test, base));
    }
    {  // unspecified hash is fine
      auto test = CreateUrlpInput(
          CreateInit("https", "", "", "example.com", "", "/", "", "thisok"));
      ASSERT_TRUE(UrlpPatternTest(pattern, test, base));
    }
    {  // pathname different
      auto test = CreateUrlpInput(
          CreateSimpleInit("https"_ns, "example.com"_ns, "/a"_ns));
      ASSERT_FALSE(UrlpPatternTest(pattern, test, base));
    }
    {  // scheme different
      auto test = CreateUrlpInput(
          CreateSimpleInit("http"_ns, "example.com"_ns, "/"_ns));
      ASSERT_FALSE(UrlpPatternTest(pattern, test, base));
    }
    {  // domain different
      auto test = CreateUrlpInput(
          CreateSimpleInit("https"_ns, "example.org"_ns, "/"_ns));
      ASSERT_FALSE(UrlpPatternTest(pattern, test, base));
    }
  }

  // check basic literal matching
  {
    UrlpPattern pattern{};
    UrlpOptions options = {.ignore_case = false};
    auto init =
        CreateInit("https"_ns, "user"_ns, "anything"_ns, "example.com"_ns,
                   "444"_ns, "/"_ns, "query"_ns, "frag"_ns, ""_ns);
    bool res = urlp_parse_pattern_from_init(&init, options, &pattern);
    ASSERT_TRUE(res);

    nsCString anything("anything");
    Optional<nsCString> base;
    {  // exact match
      auto test = CreateUrlpInput(init);
      ASSERT_TRUE(UrlpPatternTest(pattern, test, base));
    }
    {  // missing protocol
      auto test =
          CreateUrlpInput(CreateInit("", "user", anything.get(), "example.com",
                                     "444", "/", "query", "frag"));
      ASSERT_FALSE(UrlpPatternTest(pattern, test, base));
    }
    {  // missing user
      auto test =
          CreateUrlpInput(CreateInit("https", "", anything.get(), "example.com",
                                     "444", "/", "query", "frag"));
      ASSERT_FALSE(UrlpPatternTest(pattern, test, base));
    }
    {  // missing password
      auto test = CreateUrlpInput(CreateInit("https", "user", "", "example.com",
                                             "444", "/", "query", "frag"));
      ASSERT_FALSE(UrlpPatternTest(pattern, test, base));
    }
    {  // missing hostname
      auto test = CreateUrlpInput(CreateInit("https", "user", anything.get(),
                                             "", "444", "/", "query", "frag"));
      ASSERT_FALSE(UrlpPatternTest(pattern, test, base));
    }
    {  // missing port
      auto test =
          CreateUrlpInput(CreateInit("https", "user", anything.get(),
                                     "example.com", "", "/", "query", "frag"));
      ASSERT_FALSE(UrlpPatternTest(pattern, test, base));
    }
    {  // missing query
      auto test =
          CreateUrlpInput(CreateInit("https", "user", anything.get(),
                                     "example.com", "444", "/", "", "frag"));
      ASSERT_FALSE(UrlpPatternTest(pattern, test, base));
    }
    {  // missing frag
      auto test =
          CreateUrlpInput(CreateInit("https", "user", anything.get(),
                                     "example.com", "444", "/", "query", ""));
      ASSERT_FALSE(UrlpPatternTest(pattern, test, base));
    }
  }

  // check basic url with wildcard
  {
    UrlpPattern pattern{};
    UrlpOptions options = {.ignore_case = false};
    auto init = CreateSimpleInit("https"_ns, "example.com"_ns, "/*"_ns);
    bool res = urlp_parse_pattern_from_init(&init, options, &pattern);
    ASSERT_TRUE(res);

    Optional<nsCString> base;
    {  // root path matches wildcard
      auto test = CreateUrlpInput(
          CreateSimpleInit("https"_ns, "example.com"_ns, "/"_ns));
      ASSERT_TRUE(UrlpPatternTest(pattern, test, base));
    }
    {  // filename matches wildcard
      auto test = CreateUrlpInput(
          CreateSimpleInit("https"_ns, "example.com"_ns, "/thing"_ns));
      ASSERT_TRUE(UrlpPatternTest(pattern, test, base));
    }
    {  // dir/filename matches wildcard
      auto test = CreateUrlpInput(
          CreateSimpleInit("https"_ns, "example.com"_ns, "/dir/thing"_ns));
      ASSERT_TRUE(UrlpPatternTest(pattern, test, base));
    }
  }

  // check matching in pathname (needs to be at least two slashes)
  {
    UrlpPattern pattern{};
    UrlpOptions options = {.ignore_case = false};
    auto init =
        CreateSimpleInit("https"_ns, "example.com"_ns, "/:category/*"_ns);
    bool res = urlp_parse_pattern_from_init(&init, options, &pattern);
    ASSERT_TRUE(res);

    Optional<nsCString> base;
    {  // no directory and not enough slashes
      auto test = CreateUrlpInput(
          CreateSimpleInit("https"_ns, "example.com"_ns, "/"_ns));
      ASSERT_FALSE(UrlpPatternTest(pattern, test, base));
    }
    {  // no directory
      auto test = CreateUrlpInput(
          CreateSimpleInit("https"_ns, "example.com"_ns, "//"_ns));
      ASSERT_FALSE(UrlpPatternTest(pattern, test, base));
    }
    {  // not enough slashes
      auto test = CreateUrlpInput(
          CreateSimpleInit("https"_ns, "example.com"_ns, "/products"_ns));
      ASSERT_FALSE(UrlpPatternTest(pattern, test, base));
    }
    {  // dir/ works
      auto test = CreateUrlpInput(
          CreateSimpleInit("https"_ns, "example.com"_ns, "/products/"_ns));
      ASSERT_TRUE(UrlpPatternTest(pattern, test, base));
    }
    {  // diretory/filename
      auto test = CreateUrlpInput(
          CreateSimpleInit("https"_ns, "example.com"_ns, "/blog/thing"_ns));
      ASSERT_TRUE(UrlpPatternTest(pattern, test, base));
    }
    {  // nested directory
      auto test = CreateUrlpInput(
          CreateSimpleInit("https"_ns, "example.com"_ns, "/blog/thing/"_ns));
      ASSERT_TRUE(UrlpPatternTest(pattern, test, base));
    }
  }

  // check optional `s` in protocol
  {
    UrlpPattern pattern{};
    UrlpOptions options = {.ignore_case = false};
    auto init = CreateSimpleInit("http{s}?"_ns, "example.com"_ns, "/"_ns);
    bool res = urlp_parse_pattern_from_init(&init, options, &pattern);
    ASSERT_TRUE(res);

    Optional<nsCString> base;
    {  // insecure matches
      auto test = CreateUrlpInput(
          CreateSimpleInit("http"_ns, "example.com"_ns, "/"_ns));
      ASSERT_TRUE(UrlpPatternTest(pattern, test, base));
    }
    {  // secure matches
      auto test = CreateUrlpInput(
          CreateSimpleInit("https"_ns, "example.com"_ns, "/"_ns));
      ASSERT_TRUE(UrlpPatternTest(pattern, test, base));
    }
  }

  // basic relative wildcard path with base domain
  {
    UrlpPattern pattern{};
    UrlpOptions options = {.ignore_case = false};
    auto init = CreateInit(""_ns, ""_ns, ""_ns, ""_ns, ""_ns, "/admin/*"_ns,
                           ""_ns, ""_ns, "https://example.com"_ns);
    bool res = urlp_parse_pattern_from_init(&init, options, &pattern);
    ASSERT_TRUE(res);

    Optional<nsCString> base;
    {
      auto test = CreateUrlpInput(
          CreateSimpleInit("https"_ns, "example.com"_ns, "/admin/"_ns));
      ASSERT_TRUE(UrlpPatternTest(pattern, test, base));
    }
    {
      auto test = CreateUrlpInput(
          CreateSimpleInit("https"_ns, "example.com"_ns, "/admin/thing"_ns));
      ASSERT_TRUE(UrlpPatternTest(pattern, test, base));
    }
    {  // incorrect relative path doesn't match
      //
      auto test = CreateUrlpInput(
          CreateSimpleInit("https"_ns, "example.com"_ns, "/nonadmin/"_ns));
      ASSERT_FALSE(UrlpPatternTest(pattern, test, base));
    }
    {  // root path not matching relative path doesn't match
      auto test = CreateUrlpInput(
          CreateSimpleInit("https"_ns, "example.com"_ns, "/"_ns));
      ASSERT_FALSE(UrlpPatternTest(pattern, test, base));
    }
  }
}

// UrlPattern.test() from_string
TEST(TestURLPatternGlue, UrlPatternTestString)
{
  // check basic literal matching (minimal fields)
  {
    nsCString str("https://example.com/");
    UrlpPattern pattern{};
    UrlpOptions options = {.ignore_case = false};
    nsCString baseUrl("");
    bool res =
        urlp_parse_pattern_from_string(&str, &baseUrl, options, &pattern);
    ASSERT_TRUE(res);

    Optional<nsCString> base;
    {  // path fixed up "/"
      auto test = CreateUrlpInput("https://example.com"_ns);
      ASSERT_TRUE(UrlpPatternTest(pattern, test, base));
    }
    {
      auto test = CreateUrlpInput("https://example.com/"_ns);
      ASSERT_TRUE(UrlpPatternTest(pattern, test, base));
    }
    {  // unspecified user and password is fine
      auto test = CreateUrlpInput("https://user:passw@example.com"_ns);
      ASSERT_TRUE(UrlpPatternTest(pattern, test, base));
    }
    {  // unspecified port is empty so 444 doesn't match
      auto test = CreateUrlpInput("https://example.com:444/"_ns);
      ASSERT_FALSE(UrlpPatternTest(pattern, test, base));
    }
    {  // unspecified search is fine
      auto test = CreateUrlpInput("https://example.com/?thisok"_ns);
      ASSERT_TRUE(UrlpPatternTest(pattern, test, base));
    }
    {  // unspecified hash is fine
      auto test = CreateUrlpInput("https://example.com/#thisok"_ns);
      ASSERT_TRUE(UrlpPatternTest(pattern, test, base));
    }
    {  // pathname different
      auto test = CreateUrlpInput("https://example.com/a"_ns);
      ASSERT_FALSE(UrlpPatternTest(pattern, test, base));
    }
    {  // scheme different
      auto test = CreateUrlpInput("http://example.com/"_ns);
      ASSERT_FALSE(UrlpPatternTest(pattern, test, base));
    }
    {  // domain different
      auto test = CreateUrlpInput("http://example.org"_ns);
      ASSERT_FALSE(UrlpPatternTest(pattern, test, base));
    }
  }

  // check basic literal matching (all fields, except password)
  // because user:pass is parsed as: `username: user:pass, password: *`
  // when pattern is from_string
  {
    nsCString str("https://user:*@example.com:444/?query#frag");
    UrlpPattern pattern{};
    UrlpOptions options = {.ignore_case = false};
    nsCString baseUrl("");
    bool res =
        urlp_parse_pattern_from_string(&str, &baseUrl, options, &pattern);
    ASSERT_TRUE(res);

    Optional<nsCString> base;
    {  // exact match, except password
      auto test = CreateUrlpInput(
          "https://user:anything@example.com:444/?query#frag"_ns);
      ASSERT_TRUE(UrlpPatternTest(pattern, test, base));
    }
    {  // missing protocol
      auto test =
          CreateUrlpInput("user:anything@example.com:444/?query#frag"_ns);
      ASSERT_FALSE(UrlpPatternTest(pattern, test, base));
    }
    {  // missing user
      auto test =
          CreateUrlpInput("https://:anything@example.com:444/?query#frag"_ns);
      ASSERT_FALSE(UrlpPatternTest(pattern, test, base));
    }
    {  // missing password is fine
      auto test =
          CreateUrlpInput("https://user@example.com:444/?query#frag"_ns);
      ASSERT_TRUE(UrlpPatternTest(pattern, test, base));
    }
    {  // missing password is fine
      auto test =
          CreateUrlpInput("https://user@example.com:444/?query#frag"_ns);
      ASSERT_TRUE(UrlpPatternTest(pattern, test, base));
    }
    {  // missing hostname
      auto test = CreateUrlpInput("https://user:anything@:444/?query#frag"_ns);
      ASSERT_FALSE(UrlpPatternTest(pattern, test, base));
    }
    {  // missing port
      auto test =
          CreateUrlpInput("https://user:anything@example.com/?query#frag"_ns);
      ASSERT_FALSE(UrlpPatternTest(pattern, test, base));
    }
    {  // missing query
      auto test =
          CreateUrlpInput("https://user:anything@example.com:444/#frag"_ns);
      ASSERT_FALSE(UrlpPatternTest(pattern, test, base));
    }
    {  // missing frag
      auto test =
          CreateUrlpInput("https://user:anything@example.com:444/?query"_ns);
      ASSERT_FALSE(UrlpPatternTest(pattern, test, base));
    }
  }

  // check basic url with wildcard
  {
    nsCString str("https://example.com/*");
    UrlpPattern pattern{};
    UrlpOptions options = {.ignore_case = false};
    nsCString baseUrl("");
    bool res =
        urlp_parse_pattern_from_string(&str, &baseUrl, options, &pattern);
    ASSERT_TRUE(res);

    Optional<nsCString> base;
    {
      auto test = CreateUrlpInput("https://example.com/"_ns);
      ASSERT_TRUE(UrlpPatternTest(pattern, test, base));
    }
    {
      auto test = CreateUrlpInput("https://example.com/thing"_ns);
      ASSERT_TRUE(UrlpPatternTest(pattern, test, base));
    }
    {
      auto test = CreateUrlpInput("https://example.com/dir/thing"_ns);
      ASSERT_TRUE(UrlpPatternTest(pattern, test, base));
    }
  }

  // check matching in pathname (needs to be at least two slashes)
  {
    nsCString str("https://example.com/:category/*");
    UrlpPattern pattern{};
    UrlpOptions options = {.ignore_case = false};
    nsCString baseUrl("");
    bool res =
        urlp_parse_pattern_from_string(&str, &baseUrl, options, &pattern);
    ASSERT_TRUE(res);

    Optional<nsCString> base;
    {
      auto test = CreateUrlpInput("https://example.com/"_ns);
      ASSERT_FALSE(UrlpPatternTest(pattern, test, base));
    }
    {  // not enough slashes
      auto test = CreateUrlpInput("https://example.com/products"_ns);
      ASSERT_FALSE(UrlpPatternTest(pattern, test, base));
    }
    {
      auto test = CreateUrlpInput("https://example.com/products/"_ns);
      ASSERT_TRUE(UrlpPatternTest(pattern, test, base));
    }
    {
      auto test = CreateUrlpInput("https://example.com/blog/thing"_ns);
      ASSERT_TRUE(UrlpPatternTest(pattern, test, base));
    }
    {  // 3 slashes
      auto test = CreateUrlpInput("https://example.com/blog/thing/"_ns);
      ASSERT_TRUE(UrlpPatternTest(pattern, test, base));
    }
  }

  // check optional `s` in protocol
  {
    nsCString str("http{s}?://example.com/");
    UrlpPattern pattern{};
    UrlpOptions options = {.ignore_case = false};
    nsCString baseUrl("");
    bool res =
        urlp_parse_pattern_from_string(&str, &baseUrl, options, &pattern);
    ASSERT_TRUE(res);

    Optional<nsCString> base;
    {
      auto test = CreateUrlpInput("http://example.com/"_ns);
      ASSERT_TRUE(UrlpPatternTest(pattern, test, base));
    }
    {
      auto test = CreateUrlpInput("https://example.com/"_ns);
      ASSERT_TRUE(UrlpPatternTest(pattern, test, base));
    }
  }

  // basic relative wildcard path with base domain
  {
    nsCString str("../admin/*");
    UrlpPattern pattern{};
    UrlpOptions options = {.ignore_case = false};
    nsCString baseUrl("https://example.com/forum");
    // MaybeString baseUrl {.string = "https://example.com/forum"_ns, .valid =
    // true };
    bool res =
        urlp_parse_pattern_from_string(&str, &baseUrl, options, &pattern);
    ASSERT_TRUE(res);

    Optional<nsCString> base;
    {
      auto test = CreateUrlpInput("https://example.com/admin/"_ns);
      ASSERT_TRUE(UrlpPatternTest(pattern, test, base));
    }
    {
      auto test = CreateUrlpInput("https://example.com/admin/thing"_ns);
      ASSERT_TRUE(UrlpPatternTest(pattern, test, base));
    }
    {
      auto test = CreateUrlpInput("https://example.com/nonadmin/"_ns);
      ASSERT_FALSE(UrlpPatternTest(pattern, test, base));
    }
    {
      auto test = CreateUrlpInput("https://example.com/"_ns);
      ASSERT_FALSE(UrlpPatternTest(pattern, test, base));
    }
  }
}

TEST(TestURLPatternGlue, MatchInputFromString)
{
  {
    nsCString url("https://example.com/");
    UrlpMatchInputAndInputs matchInputAndInputs;
    bool res = urlp_process_match_input_from_string(&url, nullptr,
                                                    &matchInputAndInputs);
    ASSERT_TRUE(res);
    ASSERT_EQ(matchInputAndInputs.input.protocol, "https"_ns);
    ASSERT_EQ(matchInputAndInputs.input.hostname, "example.com"_ns);
    ASSERT_EQ(matchInputAndInputs.input.pathname, "/"_ns);
    ASSERT_EQ(matchInputAndInputs.input.username, "");
    ASSERT_EQ(matchInputAndInputs.input.password, "");
    ASSERT_EQ(matchInputAndInputs.input.port, "");
    ASSERT_EQ(matchInputAndInputs.input.search, "");
    ASSERT_EQ(matchInputAndInputs.input.hash, "");
    ASSERT_EQ(matchInputAndInputs.inputs.string_or_init_type,
              UrlpStringOrInitType::String);
    ASSERT_EQ(matchInputAndInputs.inputs.str, url);
    ASSERT_EQ(matchInputAndInputs.inputs.base.valid, false);
  }
  {
    nsCString expected("https://example.com/some/dir");
    nsCString base_url("https://example.com");
    nsCString relative_url("/some/dir");
    UrlpMatchInputAndInputs matchInputAndInputs;
    bool res = urlp_process_match_input_from_string(&relative_url, &base_url,
                                                    &matchInputAndInputs);
    ASSERT_TRUE(res);
    ASSERT_EQ(matchInputAndInputs.input.protocol, "https"_ns);
    ASSERT_EQ(matchInputAndInputs.input.hostname, "example.com"_ns);
    ASSERT_EQ(matchInputAndInputs.input.pathname, "/some/dir"_ns);
    ASSERT_EQ(matchInputAndInputs.input.username, "");
    ASSERT_EQ(matchInputAndInputs.input.password, "");
    ASSERT_EQ(matchInputAndInputs.input.port, "");
    ASSERT_EQ(matchInputAndInputs.input.search, "");
    ASSERT_EQ(matchInputAndInputs.input.hash, "");
    ASSERT_EQ(matchInputAndInputs.inputs.string_or_init_type,
              UrlpStringOrInitType::String);
    ASSERT_EQ(matchInputAndInputs.inputs.str, relative_url);
    ASSERT_EQ(matchInputAndInputs.inputs.base.string, base_url);
  }
}

void assert_str_ptr_same(const nsCString* s1, const nsCString* s2) {
  if (!s1) {
    ASSERT_EQ(s1, s2);
    return;
  }
  ASSERT_EQ(*(s1), *(s2));
}

void assert_maybe_string_same(const MaybeString& s1, const MaybeString& s2) {
  ASSERT_EQ(s1.valid, s2.valid);
  if (s1.valid) {
    ASSERT_EQ(s1.string, s2.string);
  }
}

void assert_inits_same(const UrlpInit& i1, const UrlpInit& i2) {
  assert_maybe_string_same(i1.protocol, i2.protocol);
  assert_maybe_string_same(i1.username, i2.username);
  assert_maybe_string_same(i1.password, i2.password);
  assert_maybe_string_same(i1.hostname, i2.hostname);
  assert_maybe_string_same(i1.port, i2.port);
  assert_maybe_string_same(i1.pathname, i2.pathname);
  assert_maybe_string_same(i1.search, i2.search);
  assert_maybe_string_same(i1.hash, i2.hash);
  assert_maybe_string_same(i1.base_url, i2.base_url);
}

void assert_match_inputs_same(const UrlpMatchInput& input,
                              const UrlpMatchInput& expected) {
  ASSERT_EQ(input.protocol, expected.protocol);
  ASSERT_EQ(input.hostname, expected.hostname);
  ASSERT_EQ(input.pathname, expected.pathname);
  ASSERT_EQ(input.username, expected.username);
  ASSERT_EQ(input.password, expected.password);
  ASSERT_EQ(input.port, expected.port);
  ASSERT_EQ(input.search, expected.search);
  ASSERT_EQ(input.hash, expected.hash);
}

UrlpMatchInput createMatchInputHelper(const nsCString& proto,
                                      const nsCString& host,
                                      const nsCString& path) {
  return {
      .protocol = proto,
      .username = ""_ns,
      .password = ""_ns,
      .hostname = host,
      .port = ""_ns,
      .pathname = path,
      .search = ""_ns,
      .hash = ""_ns,
  };
}

TEST(TestURLPatternGlue, MatchInputFromInit)
{
  {  // no base init
    UrlpMatchInputAndInputs matchInputAndInputs;
    auto init = CreateSimpleInit("https"_ns, "example.com"_ns, "/"_ns);
    auto expected = CreateSimpleInit("https"_ns, "example.com"_ns, "/"_ns);
    bool res = urlp_process_match_input_from_init(&init, nullptr,
                                                  &matchInputAndInputs);
    ASSERT_TRUE(res);

    UrlpMatchInput expected_match_input =
        createMatchInputHelper("https"_ns, "example.com"_ns, "/"_ns);
    assert_match_inputs_same(matchInputAndInputs.input, expected_match_input);
    ASSERT_EQ(matchInputAndInputs.inputs.string_or_init_type,
              UrlpStringOrInitType::Init);
    assert_inits_same(matchInputAndInputs.inputs.init, init);
    ASSERT_EQ(matchInputAndInputs.inputs.str, ""_ns);
    ASSERT_EQ(matchInputAndInputs.inputs.base.valid, false);
  }
  {  // base + relative url produces expected match input
    nsCString expected_base_url("https://example.com");

    auto init = CreateInit("", "", "", "", "", "/some/dir", "", "",
                           "https://example.com");
    UrlpMatchInputAndInputs matchInputAndInputs;
    bool res = urlp_process_match_input_from_init(&init, nullptr,
                                                  &matchInputAndInputs);
    ASSERT_TRUE(res);

    UrlpMatchInput expected_match_input =
        createMatchInputHelper("https"_ns, "example.com"_ns, "/some/dir"_ns);
    assert_match_inputs_same(matchInputAndInputs.input, expected_match_input);
    ASSERT_EQ(matchInputAndInputs.inputs.string_or_init_type,
              UrlpStringOrInitType::Init);
    assert_inits_same(matchInputAndInputs.inputs.init, init);
    ASSERT_EQ(matchInputAndInputs.inputs.str, ""_ns);
    ASSERT_EQ(matchInputAndInputs.inputs.base.valid, false);
  }
}

void assert_matcher_same(UrlpMatcher& componentMatcher, UrlpMatcher& expected) {
  ASSERT_EQ(componentMatcher.prefix, expected.prefix);
  ASSERT_EQ(componentMatcher.suffix, expected.suffix);
  ASSERT_EQ(componentMatcher.inner.inner_type, expected.inner.inner_type);
  ASSERT_EQ(componentMatcher.inner.literal, expected.inner.literal);
  ASSERT_EQ(componentMatcher.inner.allow_empty, expected.inner.allow_empty);
  ASSERT_EQ(componentMatcher.inner.filter_exists, expected.inner.filter_exists);
  ASSERT_EQ(componentMatcher.inner.filter, expected.inner.filter);
  ASSERT_EQ(componentMatcher.inner.regexp, expected.inner.regexp);
  ASSERT_TRUE(componentMatcher == expected);
}

TEST(TestURLPatternGlue, UrlPatternGetComponentBasic)
{
  nsCString str(":café://:foo");
  UrlpOptions options = {.ignore_case = false};
  nsCString tmp("https://example.com/");
  UrlpPattern pattern{};
  bool res = urlp_parse_pattern_from_string(&str, &tmp, options, &pattern);
  ASSERT_TRUE(res);
  ASSERT_TRUE(pattern._0);

  UrlpInnerMatcher expectedInnerMatcher = {
      .inner_type = UrlpInnerMatcherType::SingleCapture,
      .literal = ""_ns,
      .allow_empty = false,
      .filter_exists = true,
      .filter = 'x',
      .regexp = ""_ns,
  };
  UrlpMatcher expectedMatcher = {
      .prefix = ""_ns,
      .suffix = ""_ns,
      .inner = expectedInnerMatcher,
  };
  nsCString expectedPatternString(":café");
  nsCString expectedRegexp("^(.+?)$");

  UrlpComponent componentProtocol;
  urlp_get_protocol_component(pattern, &componentProtocol);
  assert_str_ptr_same(&componentProtocol.pattern_string,
                      &expectedPatternString);
  assert_str_ptr_same(&componentProtocol.regexp_string, &expectedRegexp);
  assert_matcher_same(componentProtocol.matcher, expectedMatcher);
  ASSERT_EQ(componentProtocol.group_name_list[0], "café"_ns);

  UrlpComponent componentHostname;
  urlp_get_hostname_component(pattern, &componentHostname);
  nsCString expectedHostnamePatternString(":foo");
  nsCString expectedHostnameRegexp("^([^\\.]+?)$");
  expectedMatcher.inner.filter = '.';
  assert_str_ptr_same(&componentHostname.pattern_string,
                      &expectedHostnamePatternString);
  assert_str_ptr_same(&componentHostname.regexp_string,
                      &expectedHostnameRegexp);
  assert_matcher_same(componentHostname.matcher, expectedMatcher);
  ASSERT_EQ(componentHostname.group_name_list[0], "foo"_ns);

  UrlpComponent componentPathname;
  urlp_get_pathname_component(pattern, &componentPathname);
  nsCString expectedPathnamePatternString("*");
  nsCString expectedPathnameRegexp("^(.*)$");
  expectedMatcher.inner.filter = 'x';
  expectedMatcher.inner.allow_empty = true;
  assert_str_ptr_same(&componentPathname.pattern_string,
                      &expectedPathnamePatternString);
  assert_str_ptr_same(&componentPathname.regexp_string,
                      &expectedPathnameRegexp);
  assert_matcher_same(componentPathname.matcher, expectedMatcher);
  ASSERT_EQ(componentPathname.group_name_list[0], "0"_ns);
}

void assert_pattern_result(UrlpResult& res) {
  ASSERT_TRUE(res.mProtocol.isSome());
  ASSERT_TRUE(res.mUsername.isSome());
  ASSERT_TRUE(res.mPassword.isSome());
  ASSERT_TRUE(res.mHostname.isSome());
  ASSERT_TRUE(res.mPort.isSome());
  ASSERT_TRUE(res.mPathname.isSome());
  ASSERT_TRUE(res.mSearch.isSome());
  ASSERT_TRUE(res.mHash.isSome());
  ASSERT_TRUE(res.mInputs.Length() == 1);
}

TEST(TestURLPatternGlue, UrlPatternExecFromString)
{
  nsCString str(":café://:foo");
  UrlpOptions options = {.ignore_case = false};
  nsCString baseUrl("https://example.com/");
  UrlpPattern pattern{};
  bool res = urlp_parse_pattern_from_string(&str, &baseUrl, options, &pattern);
  ASSERT_TRUE(res);
  ASSERT_TRUE(pattern._0);

  nsCString inputString("https://example.com/");
  UrlpInput input = CreateUrlpInput(inputString);
  Optional<nsAutoCString> execBaseUrl;
  Optional<UrlpResult> res2 = UrlpPatternExec(pattern, input, execBaseUrl);

  ASSERT_TRUE(res2.isNothing());
}

TEST(TestURLPatternGlue, UrlPatternExecFromInit)
{
  UrlpPattern pattern{};
  auto init = CreateSimpleInit("https"_ns, "example.com"_ns, "/"_ns);
  UrlpOptions options = {.ignore_case = false};
  bool res = urlp_parse_pattern_from_init(&init, options, &pattern);
  ASSERT_TRUE(res);
  ASSERT_TRUE(pattern._0);

  UrlpInput input = CreateUrlpInput(init);
  Optional<nsAutoCString> execBaseUrl;
  Optional<UrlpResult> res2 = UrlpPatternExec(pattern, input, execBaseUrl);
  ASSERT_TRUE(res2.isSome());
  assert_pattern_result(*res2);
  ASSERT_EQ(res2->mProtocol->mInput, "https");
  ASSERT_EQ(res2->mUsername->mInput, ""_ns);
  ASSERT_EQ(res2->mPassword->mInput, ""_ns);
  ASSERT_EQ(res2->mHostname->mInput, "example.com");
  ASSERT_EQ(res2->mPort->mInput, ""_ns);
  ASSERT_EQ(res2->mPathname->mInput, "/"_ns);
  ASSERT_EQ(res2->mSearch->mInput, ""_ns);
  ASSERT_EQ(res2->mHash->mInput, ""_ns);
}
