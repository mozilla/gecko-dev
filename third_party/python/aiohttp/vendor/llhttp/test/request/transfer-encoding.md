Transfer-Encoding header
========================

## `chunked`

### Parsing and setting flag

<!-- meta={"type": "request"} -->
```http
PUT /url HTTP/1.1
Transfer-Encoding: chunked


```

```log
off=0 message begin
off=0 len=3 span[method]="PUT"
off=3 method complete
off=4 len=4 span[url]="/url"
off=9 url complete
off=14 len=3 span[version]="1.1"
off=17 version complete
off=19 len=17 span[header_field]="Transfer-Encoding"
off=37 header_field complete
off=38 len=7 span[header_value]="chunked"
off=47 header_value complete
off=49 headers complete method=4 v=1/1 flags=208 content_length=0
```

### Parse chunks with lowercase size

<!-- meta={"type": "request"} -->
```http
PUT /url HTTP/1.1
Transfer-Encoding: chunked

a
0123456789
0


```

```log
off=0 message begin
off=0 len=3 span[method]="PUT"
off=3 method complete
off=4 len=4 span[url]="/url"
off=9 url complete
off=14 len=3 span[version]="1.1"
off=17 version complete
off=19 len=17 span[header_field]="Transfer-Encoding"
off=37 header_field complete
off=38 len=7 span[header_value]="chunked"
off=47 header_value complete
off=49 headers complete method=4 v=1/1 flags=208 content_length=0
off=52 chunk header len=10
off=52 len=10 span[body]="0123456789"
off=64 chunk complete
off=67 chunk header len=0
off=69 chunk complete
off=69 message complete
```

### Parse chunks with uppercase size

<!-- meta={"type": "request"} -->
```http
PUT /url HTTP/1.1
Transfer-Encoding: chunked

A
0123456789
0


```

```log
off=0 message begin
off=0 len=3 span[method]="PUT"
off=3 method complete
off=4 len=4 span[url]="/url"
off=9 url complete
off=14 len=3 span[version]="1.1"
off=17 version complete
off=19 len=17 span[header_field]="Transfer-Encoding"
off=37 header_field complete
off=38 len=7 span[header_value]="chunked"
off=47 header_value complete
off=49 headers complete method=4 v=1/1 flags=208 content_length=0
off=52 chunk header len=10
off=52 len=10 span[body]="0123456789"
off=64 chunk complete
off=67 chunk header len=0
off=69 chunk complete
off=69 message complete
```

### POST with `Transfer-Encoding: chunked`

<!-- meta={"type": "request"} -->
```http
POST /post_chunked_all_your_base HTTP/1.1
Transfer-Encoding: chunked

1e
all your base are belong to us
0


```

```log
off=0 message begin
off=0 len=4 span[method]="POST"
off=4 method complete
off=5 len=27 span[url]="/post_chunked_all_your_base"
off=33 url complete
off=38 len=3 span[version]="1.1"
off=41 version complete
off=43 len=17 span[header_field]="Transfer-Encoding"
off=61 header_field complete
off=62 len=7 span[header_value]="chunked"
off=71 header_value complete
off=73 headers complete method=3 v=1/1 flags=208 content_length=0
off=77 chunk header len=30
off=77 len=30 span[body]="all your base are belong to us"
off=109 chunk complete
off=112 chunk header len=0
off=114 chunk complete
off=114 message complete
```

### Two chunks and triple zero prefixed end chunk

<!-- meta={"type": "request"} -->
```http
POST /two_chunks_mult_zero_end HTTP/1.1
Transfer-Encoding: chunked

5
hello
6
 world
000


```

```log
off=0 message begin
off=0 len=4 span[method]="POST"
off=4 method complete
off=5 len=25 span[url]="/two_chunks_mult_zero_end"
off=31 url complete
off=36 len=3 span[version]="1.1"
off=39 version complete
off=41 len=17 span[header_field]="Transfer-Encoding"
off=59 header_field complete
off=60 len=7 span[header_value]="chunked"
off=69 header_value complete
off=71 headers complete method=3 v=1/1 flags=208 content_length=0
off=74 chunk header len=5
off=74 len=5 span[body]="hello"
off=81 chunk complete
off=84 chunk header len=6
off=84 len=6 span[body]=" world"
off=92 chunk complete
off=97 chunk header len=0
off=99 chunk complete
off=99 message complete
```

### Trailing headers

<!-- meta={"type": "request"} -->
```http
POST /chunked_w_trailing_headers HTTP/1.1
Transfer-Encoding: chunked

5
hello
6
 world
0
Vary: *
Content-Type: text/plain


```

```log
off=0 message begin
off=0 len=4 span[method]="POST"
off=4 method complete
off=5 len=27 span[url]="/chunked_w_trailing_headers"
off=33 url complete
off=38 len=3 span[version]="1.1"
off=41 version complete
off=43 len=17 span[header_field]="Transfer-Encoding"
off=61 header_field complete
off=62 len=7 span[header_value]="chunked"
off=71 header_value complete
off=73 headers complete method=3 v=1/1 flags=208 content_length=0
off=76 chunk header len=5
off=76 len=5 span[body]="hello"
off=83 chunk complete
off=86 chunk header len=6
off=86 len=6 span[body]=" world"
off=94 chunk complete
off=97 chunk header len=0
off=97 len=4 span[header_field]="Vary"
off=102 header_field complete
off=103 len=1 span[header_value]="*"
off=106 header_value complete
off=106 len=12 span[header_field]="Content-Type"
off=119 header_field complete
off=120 len=10 span[header_value]="text/plain"
off=132 header_value complete
off=134 chunk complete
off=134 message complete
```

### Chunk extensions

<!-- meta={"type": "request"} -->
```http
POST /chunked_w_unicorns_after_length HTTP/1.1
Transfer-Encoding: chunked

5;ilovew3;somuchlove=aretheseparametersfor;another=withvalue
hello
6;blahblah;blah
 world
0

```

```log
off=0 message begin
off=0 len=4 span[method]="POST"
off=4 method complete
off=5 len=32 span[url]="/chunked_w_unicorns_after_length"
off=38 url complete
off=43 len=3 span[version]="1.1"
off=46 version complete
off=48 len=17 span[header_field]="Transfer-Encoding"
off=66 header_field complete
off=67 len=7 span[header_value]="chunked"
off=76 header_value complete
off=78 headers complete method=3 v=1/1 flags=208 content_length=0
off=80 len=7 span[chunk_extension_name]="ilovew3"
off=88 chunk_extension_name complete
off=88 len=10 span[chunk_extension_name]="somuchlove"
off=99 chunk_extension_name complete
off=99 len=21 span[chunk_extension_value]="aretheseparametersfor"
off=121 chunk_extension_value complete
off=121 len=7 span[chunk_extension_name]="another"
off=129 chunk_extension_name complete
off=129 len=9 span[chunk_extension_value]="withvalue"
off=139 chunk_extension_value complete
off=140 chunk header len=5
off=140 len=5 span[body]="hello"
off=147 chunk complete
off=149 len=8 span[chunk_extension_name]="blahblah"
off=158 chunk_extension_name complete
off=158 len=4 span[chunk_extension_name]="blah"
off=163 chunk_extension_name complete
off=164 chunk header len=6
off=164 len=6 span[body]=" world"
off=172 chunk complete
off=175 chunk header len=0
```

### No semicolon before chunk extensions

<!-- meta={"type": "request"} -->
```http
POST /chunked_w_unicorns_after_length HTTP/1.1
Host: localhost
Transfer-encoding: chunked

2 erfrferferf
aa
0 rrrr


```

```log
off=0 message begin
off=0 len=4 span[method]="POST"
off=4 method complete
off=5 len=32 span[url]="/chunked_w_unicorns_after_length"
off=38 url complete
off=43 len=3 span[version]="1.1"
off=46 version complete
off=48 len=4 span[header_field]="Host"
off=53 header_field complete
off=54 len=9 span[header_value]="localhost"
off=65 header_value complete
off=65 len=17 span[header_field]="Transfer-encoding"
off=83 header_field complete
off=84 len=7 span[header_value]="chunked"
off=93 header_value complete
off=95 headers complete method=3 v=1/1 flags=208 content_length=0
off=97 error code=12 reason="Invalid character in chunk size"
```

### No extension after semicolon

<!-- meta={"type": "request"} -->
```http
POST /chunked_w_unicorns_after_length HTTP/1.1
Host: localhost
Transfer-encoding: chunked

2;
aa
0


```

```log
off=0 message begin
off=0 len=4 span[method]="POST"
off=4 method complete
off=5 len=32 span[url]="/chunked_w_unicorns_after_length"
off=38 url complete
off=43 len=3 span[version]="1.1"
off=46 version complete
off=48 len=4 span[header_field]="Host"
off=53 header_field complete
off=54 len=9 span[header_value]="localhost"
off=65 header_value complete
off=65 len=17 span[header_field]="Transfer-encoding"
off=83 header_field complete
off=84 len=7 span[header_value]="chunked"
off=93 header_value complete
off=95 headers complete method=3 v=1/1 flags=208 content_length=0
off=98 error code=2 reason="Invalid character in chunk extensions"
```


### Chunk extensions quoting

<!-- meta={"type": "request"} -->
```http
POST /chunked_w_unicorns_after_length HTTP/1.1
Transfer-Encoding: chunked

5;ilovew3="I \"love\"; \\extensions\\";somuchlove="aretheseparametersfor";blah;foo=bar
hello
6;blahblah;blah
 world
0

```

```log
off=0 message begin
off=0 len=4 span[method]="POST"
off=4 method complete
off=5 len=32 span[url]="/chunked_w_unicorns_after_length"
off=38 url complete
off=43 len=3 span[version]="1.1"
off=46 version complete
off=48 len=17 span[header_field]="Transfer-Encoding"
off=66 header_field complete
off=67 len=7 span[header_value]="chunked"
off=76 header_value complete
off=78 headers complete method=3 v=1/1 flags=208 content_length=0
off=80 len=7 span[chunk_extension_name]="ilovew3"
off=88 chunk_extension_name complete
off=88 len=28 span[chunk_extension_value]=""I \"love\"; \\extensions\\""
off=116 chunk_extension_value complete
off=117 len=10 span[chunk_extension_name]="somuchlove"
off=128 chunk_extension_name complete
off=128 len=23 span[chunk_extension_value]=""aretheseparametersfor""
off=151 chunk_extension_value complete
off=152 len=4 span[chunk_extension_name]="blah"
off=157 chunk_extension_name complete
off=157 len=3 span[chunk_extension_name]="foo"
off=161 chunk_extension_name complete
off=161 len=3 span[chunk_extension_value]="bar"
off=165 chunk_extension_value complete
off=166 chunk header len=5
off=166 len=5 span[body]="hello"
off=173 chunk complete
off=175 len=8 span[chunk_extension_name]="blahblah"
off=184 chunk_extension_name complete
off=184 len=4 span[chunk_extension_name]="blah"
off=189 chunk_extension_name complete
off=190 chunk header len=6
off=190 len=6 span[body]=" world"
off=198 chunk complete
off=201 chunk header len=0
```


### Unbalanced chunk extensions quoting

<!-- meta={"type": "request"} -->
```http
POST /chunked_w_unicorns_after_length HTTP/1.1
Transfer-Encoding: chunked

5;ilovew3="abc";somuchlove="def; ghi
hello
6;blahblah;blah
 world
0

```

```log
off=0 message begin
off=0 len=4 span[method]="POST"
off=4 method complete
off=5 len=32 span[url]="/chunked_w_unicorns_after_length"
off=38 url complete
off=43 len=3 span[version]="1.1"
off=46 version complete
off=48 len=17 span[header_field]="Transfer-Encoding"
off=66 header_field complete
off=67 len=7 span[header_value]="chunked"
off=76 header_value complete
off=78 headers complete method=3 v=1/1 flags=208 content_length=0
off=80 len=7 span[chunk_extension_name]="ilovew3"
off=88 chunk_extension_name complete
off=88 len=5 span[chunk_extension_value]=""abc""
off=93 chunk_extension_value complete
off=94 len=10 span[chunk_extension_name]="somuchlove"
off=105 chunk_extension_name complete
off=105 len=9 span[chunk_extension_value]=""def; ghi"
off=115 error code=2 reason="Invalid character in chunk extensions quoted value"
```

## Ignoring `pigeons`

Requests cannot have invalid `Transfer-Encoding`. It is impossible to determine
their body size. Not erroring would make HTTP smuggling attacks possible.

<!-- meta={"type": "request", "noScan": true} -->
```http
PUT /url HTTP/1.1
Transfer-Encoding: pigeons


```

```log
off=0 message begin
off=0 len=3 span[method]="PUT"
off=3 method complete
off=4 len=4 span[url]="/url"
off=9 url complete
off=14 len=3 span[version]="1.1"
off=17 version complete
off=19 len=17 span[header_field]="Transfer-Encoding"
off=37 header_field complete
off=38 len=7 span[header_value]="pigeons"
off=47 header_value complete
off=49 headers complete method=4 v=1/1 flags=200 content_length=0
off=49 error code=15 reason="Request has invalid `Transfer-Encoding`"
```

## POST with `Transfer-Encoding` and `Content-Length`

<!-- meta={"type": "request"} -->
```http
POST /post_identity_body_world?q=search#hey HTTP/1.1
Accept: */*
Transfer-Encoding: identity
Content-Length: 5

World
```

```log
off=0 message begin
off=0 len=4 span[method]="POST"
off=4 method complete
off=5 len=38 span[url]="/post_identity_body_world?q=search#hey"
off=44 url complete
off=49 len=3 span[version]="1.1"
off=52 version complete
off=54 len=6 span[header_field]="Accept"
off=61 header_field complete
off=62 len=3 span[header_value]="*/*"
off=67 header_value complete
off=67 len=17 span[header_field]="Transfer-Encoding"
off=85 header_field complete
off=86 len=8 span[header_value]="identity"
off=96 header_value complete
off=96 len=14 span[header_field]="Content-Length"
off=111 header_field complete
off=111 error code=11 reason="Content-Length can't be present with Transfer-Encoding"
```

## POST with `Transfer-Encoding` and `Content-Length` (lenient)

TODO(indutny): should we allow it even in lenient mode? (Consider disabling
this).

NOTE: `Content-Length` is ignored when `Transfer-Encoding` is present. Messages
(in lenient mode) are read until EOF.

<!-- meta={"type": "request-lenient-chunked-length"} -->
```http
POST /post_identity_body_world?q=search#hey HTTP/1.1
Accept: */*
Transfer-Encoding: identity
Content-Length: 1

World
```

```log
off=0 message begin
off=0 len=4 span[method]="POST"
off=4 method complete
off=5 len=38 span[url]="/post_identity_body_world?q=search#hey"
off=44 url complete
off=49 len=3 span[version]="1.1"
off=52 version complete
off=54 len=6 span[header_field]="Accept"
off=61 header_field complete
off=62 len=3 span[header_value]="*/*"
off=67 header_value complete
off=67 len=17 span[header_field]="Transfer-Encoding"
off=85 header_field complete
off=86 len=8 span[header_value]="identity"
off=96 header_value complete
off=96 len=14 span[header_field]="Content-Length"
off=111 header_field complete
off=112 len=1 span[header_value]="1"
off=115 header_value complete
off=117 headers complete method=3 v=1/1 flags=220 content_length=1
off=117 len=5 span[body]="World"
```

## POST with empty `Transfer-Encoding` and `Content-Length` (lenient)

<!-- meta={"type": "request"} -->
```http
POST / HTTP/1.1
Host: foo
Content-Length: 10
Transfer-Encoding:
Transfer-Encoding:
Transfer-Encoding:

2
AA
0
```

```log
off=0 message begin
off=0 len=4 span[method]="POST"
off=4 method complete
off=5 len=1 span[url]="/"
off=7 url complete
off=12 len=3 span[version]="1.1"
off=15 version complete
off=17 len=4 span[header_field]="Host"
off=22 header_field complete
off=23 len=3 span[header_value]="foo"
off=28 header_value complete
off=28 len=14 span[header_field]="Content-Length"
off=43 header_field complete
off=44 len=2 span[header_value]="10"
off=48 header_value complete
off=48 len=17 span[header_field]="Transfer-Encoding"
off=66 header_field complete
off=66 error code=15 reason="Transfer-Encoding can't be present with Content-Length"
```

## POST with `chunked` before other transfer coding names

<!-- meta={"type": "request", "noScan": true} -->
```http
POST /post_identity_body_world?q=search#hey HTTP/1.1
Accept: */*
Transfer-Encoding: chunked, deflate

World
```

```log
off=0 message begin
off=0 len=4 span[method]="POST"
off=4 method complete
off=5 len=38 span[url]="/post_identity_body_world?q=search#hey"
off=44 url complete
off=49 len=3 span[version]="1.1"
off=52 version complete
off=54 len=6 span[header_field]="Accept"
off=61 header_field complete
off=62 len=3 span[header_value]="*/*"
off=67 header_value complete
off=67 len=17 span[header_field]="Transfer-Encoding"
off=85 header_field complete
off=86 len=7 span[header_value]="chunked"
off=94 error code=15 reason="Invalid `Transfer-Encoding` header value"
```

## POST with `chunked` and duplicate transfer-encoding

<!-- meta={"type": "request", "noScan": true} -->
```http
POST /post_identity_body_world?q=search#hey HTTP/1.1
Accept: */*
Transfer-Encoding: chunked
Transfer-Encoding: deflate

World
```

```log
off=0 message begin
off=0 len=4 span[method]="POST"
off=4 method complete
off=5 len=38 span[url]="/post_identity_body_world?q=search#hey"
off=44 url complete
off=49 len=3 span[version]="1.1"
off=52 version complete
off=54 len=6 span[header_field]="Accept"
off=61 header_field complete
off=62 len=3 span[header_value]="*/*"
off=67 header_value complete
off=67 len=17 span[header_field]="Transfer-Encoding"
off=85 header_field complete
off=86 len=7 span[header_value]="chunked"
off=95 header_value complete
off=95 len=17 span[header_field]="Transfer-Encoding"
off=113 header_field complete
off=114 len=0 span[header_value]=""
off=115 error code=15 reason="Invalid `Transfer-Encoding` header value"
```

## POST with `chunked` before other transfer-coding (lenient)

<!-- meta={"type": "request-lenient-transfer-encoding"} -->
```http
POST /post_identity_body_world?q=search#hey HTTP/1.1
Accept: */*
Transfer-Encoding: chunked, deflate

World
```

```log
off=0 message begin
off=0 len=4 span[method]="POST"
off=4 method complete
off=5 len=38 span[url]="/post_identity_body_world?q=search#hey"
off=44 url complete
off=49 len=3 span[version]="1.1"
off=52 version complete
off=54 len=6 span[header_field]="Accept"
off=61 header_field complete
off=62 len=3 span[header_value]="*/*"
off=67 header_value complete
off=67 len=17 span[header_field]="Transfer-Encoding"
off=85 header_field complete
off=86 len=16 span[header_value]="chunked, deflate"
off=104 header_value complete
off=106 headers complete method=3 v=1/1 flags=200 content_length=0
off=106 len=5 span[body]="World"
```

## POST with `chunked` and duplicate transfer-encoding (lenient)

<!-- meta={"type": "request-lenient-transfer-encoding"} -->
```http
POST /post_identity_body_world?q=search#hey HTTP/1.1
Accept: */*
Transfer-Encoding: chunked
Transfer-Encoding: deflate

World
```

```log
off=0 message begin
off=0 len=4 span[method]="POST"
off=4 method complete
off=5 len=38 span[url]="/post_identity_body_world?q=search#hey"
off=44 url complete
off=49 len=3 span[version]="1.1"
off=52 version complete
off=54 len=6 span[header_field]="Accept"
off=61 header_field complete
off=62 len=3 span[header_value]="*/*"
off=67 header_value complete
off=67 len=17 span[header_field]="Transfer-Encoding"
off=85 header_field complete
off=86 len=7 span[header_value]="chunked"
off=95 header_value complete
off=95 len=17 span[header_field]="Transfer-Encoding"
off=113 header_field complete
off=114 len=7 span[header_value]="deflate"
off=123 header_value complete
off=125 headers complete method=3 v=1/1 flags=200 content_length=0
off=125 len=5 span[body]="World"
```

## POST with `chunked` as last transfer-encoding

<!-- meta={"type": "request"} -->
```http
POST /post_identity_body_world?q=search#hey HTTP/1.1
Accept: */*
Transfer-Encoding: deflate, chunked

5
World
0


```

```log
off=0 message begin
off=0 len=4 span[method]="POST"
off=4 method complete
off=5 len=38 span[url]="/post_identity_body_world?q=search#hey"
off=44 url complete
off=49 len=3 span[version]="1.1"
off=52 version complete
off=54 len=6 span[header_field]="Accept"
off=61 header_field complete
off=62 len=3 span[header_value]="*/*"
off=67 header_value complete
off=67 len=17 span[header_field]="Transfer-Encoding"
off=85 header_field complete
off=86 len=16 span[header_value]="deflate, chunked"
off=104 header_value complete
off=106 headers complete method=3 v=1/1 flags=208 content_length=0
off=109 chunk header len=5
off=109 len=5 span[body]="World"
off=116 chunk complete
off=119 chunk header len=0
off=121 chunk complete
off=121 message complete
```

## POST with `chunked` as last transfer-encoding (multiple headers)

<!-- meta={"type": "request"} -->
```http
POST /post_identity_body_world?q=search#hey HTTP/1.1
Accept: */*
Transfer-Encoding: deflate
Transfer-Encoding: chunked

5
World
0


```

```log
off=0 message begin
off=0 len=4 span[method]="POST"
off=4 method complete
off=5 len=38 span[url]="/post_identity_body_world?q=search#hey"
off=44 url complete
off=49 len=3 span[version]="1.1"
off=52 version complete
off=54 len=6 span[header_field]="Accept"
off=61 header_field complete
off=62 len=3 span[header_value]="*/*"
off=67 header_value complete
off=67 len=17 span[header_field]="Transfer-Encoding"
off=85 header_field complete
off=86 len=7 span[header_value]="deflate"
off=95 header_value complete
off=95 len=17 span[header_field]="Transfer-Encoding"
off=113 header_field complete
off=114 len=7 span[header_value]="chunked"
off=123 header_value complete
off=125 headers complete method=3 v=1/1 flags=208 content_length=0
off=128 chunk header len=5
off=128 len=5 span[body]="World"
off=135 chunk complete
off=138 chunk header len=0
off=140 chunk complete
off=140 message complete
```

## POST with `chunkedchunked` as transfer-encoding

<!-- meta={"type": "request"} -->
```http
POST /post_identity_body_world?q=search#hey HTTP/1.1
Accept: */*
Transfer-Encoding: chunkedchunked

5
World
0


```

```log
off=0 message begin
off=0 len=4 span[method]="POST"
off=4 method complete
off=5 len=38 span[url]="/post_identity_body_world?q=search#hey"
off=44 url complete
off=49 len=3 span[version]="1.1"
off=52 version complete
off=54 len=6 span[header_field]="Accept"
off=61 header_field complete
off=62 len=3 span[header_value]="*/*"
off=67 header_value complete
off=67 len=17 span[header_field]="Transfer-Encoding"
off=85 header_field complete
off=86 len=14 span[header_value]="chunkedchunked"
off=102 header_value complete
off=104 headers complete method=3 v=1/1 flags=200 content_length=0
off=104 error code=15 reason="Request has invalid `Transfer-Encoding`"
```

## Missing last-chunk

<!-- meta={"type": "request"} -->
```http
PUT /url HTTP/1.1
Transfer-Encoding: chunked

3
foo


```

```log
off=0 message begin
off=0 len=3 span[method]="PUT"
off=3 method complete
off=4 len=4 span[url]="/url"
off=9 url complete
off=14 len=3 span[version]="1.1"
off=17 version complete
off=19 len=17 span[header_field]="Transfer-Encoding"
off=37 header_field complete
off=38 len=7 span[header_value]="chunked"
off=47 header_value complete
off=49 headers complete method=4 v=1/1 flags=208 content_length=0
off=52 chunk header len=3
off=52 len=3 span[body]="foo"
off=57 chunk complete
off=57 error code=12 reason="Invalid character in chunk size"
```

## Validate chunk parameters

<!-- meta={"type": "request" } -->
```http
PUT /url HTTP/1.1
Transfer-Encoding: chunked

3 \n  \r\n\
foo


```

```log
off=0 message begin
off=0 len=3 span[method]="PUT"
off=3 method complete
off=4 len=4 span[url]="/url"
off=9 url complete
off=14 len=3 span[version]="1.1"
off=17 version complete
off=19 len=17 span[header_field]="Transfer-Encoding"
off=37 header_field complete
off=38 len=7 span[header_value]="chunked"
off=47 header_value complete
off=49 headers complete method=4 v=1/1 flags=208 content_length=0
off=51 error code=12 reason="Invalid character in chunk size"
```

## Invalid OBS fold after chunked value

<!-- meta={"type": "request-lenient-headers" } -->
```http
PUT /url HTTP/1.1
Transfer-Encoding: chunked
  abc

5
World
0


```

```log
off=0 message begin
off=0 len=3 span[method]="PUT"
off=3 method complete
off=4 len=4 span[url]="/url"
off=9 url complete
off=14 len=3 span[version]="1.1"
off=17 version complete
off=19 len=17 span[header_field]="Transfer-Encoding"
off=37 header_field complete
off=38 len=7 span[header_value]="chunked"
off=47 len=5 span[header_value]="  abc"
off=54 header_value complete
off=56 headers complete method=4 v=1/1 flags=200 content_length=0
off=56 error code=15 reason="Request has invalid `Transfer-Encoding`"
```

### Chunk header not terminated by CRLF

<!-- meta={"type": "request" } -->

```http
GET / HTTP/1.1
Host: a
Connection: close 
Transfer-Encoding: chunked 

5\r\r;ABCD
34
E
0

GET / HTTP/1.1 
Host: a
Content-Length: 5

0

```

```log
off=0 message begin
off=0 len=3 span[method]="GET"
off=3 method complete
off=4 len=1 span[url]="/"
off=6 url complete
off=11 len=3 span[version]="1.1"
off=14 version complete
off=16 len=4 span[header_field]="Host"
off=21 header_field complete
off=22 len=1 span[header_value]="a"
off=25 header_value complete
off=25 len=10 span[header_field]="Connection"
off=36 header_field complete
off=37 len=6 span[header_value]="close "
off=45 header_value complete
off=45 len=17 span[header_field]="Transfer-Encoding"
off=63 header_field complete
off=64 len=8 span[header_value]="chunked "
off=74 header_value complete
off=76 headers complete method=1 v=1/1 flags=20a content_length=0
off=78 error code=2 reason="Expected LF after chunk size"
```

### Chunk header not terminated by CRLF (lenient)

<!-- meta={"type": "request-lenient-optional-lf-after-cr" } -->

```http
GET / HTTP/1.1
Host: a
Connection: close 
Transfer-Encoding: chunked 

6\r\r;ABCD
33
E
0

GET / HTTP/1.1 
Host: a
Content-Length: 5
0


```

```log
off=0 message begin
off=0 len=3 span[method]="GET"
off=3 method complete
off=4 len=1 span[url]="/"
off=6 url complete
off=11 len=3 span[version]="1.1"
off=14 version complete
off=16 len=4 span[header_field]="Host"
off=21 header_field complete
off=22 len=1 span[header_value]="a"
off=25 header_value complete
off=25 len=10 span[header_field]="Connection"
off=36 header_field complete
off=37 len=6 span[header_value]="close "
off=45 header_value complete
off=45 len=17 span[header_field]="Transfer-Encoding"
off=63 header_field complete
off=64 len=8 span[header_value]="chunked "
off=74 header_value complete
off=76 headers complete method=1 v=1/1 flags=20a content_length=0
off=78 chunk header len=6
off=78 len=1 span[body]=cr
off=79 len=5 span[body]=";ABCD"
off=86 chunk complete
off=90 chunk header len=51
off=90 len=1 span[body]="E"
off=91 len=1 span[body]=cr
off=92 len=1 span[body]=lf
off=93 len=1 span[body]="0"
off=94 len=1 span[body]=cr
off=95 len=1 span[body]=lf
off=96 len=1 span[body]=cr
off=97 len=1 span[body]=lf
off=98 len=15 span[body]="GET / HTTP/1.1 "
off=113 len=1 span[body]=cr
off=114 len=1 span[body]=lf
off=115 len=7 span[body]="Host: a"
off=122 len=1 span[body]=cr
off=123 len=1 span[body]=lf
off=124 len=17 span[body]="Content-Length: 5"
off=143 chunk complete
off=146 chunk header len=0
off=148 chunk complete
off=148 message complete
```

### Chunk data not terminated by CRLF

<!-- meta={"type": "request" } -->

```http
GET / HTTP/1.1
Host: a
Connection: close 
Transfer-Encoding: chunked 

5
ABCDE0

```

```log
off=0 message begin
off=0 len=3 span[method]="GET"
off=3 method complete
off=4 len=1 span[url]="/"
off=6 url complete
off=11 len=3 span[version]="1.1"
off=14 version complete
off=16 len=4 span[header_field]="Host"
off=21 header_field complete
off=22 len=1 span[header_value]="a"
off=25 header_value complete
off=25 len=10 span[header_field]="Connection"
off=36 header_field complete
off=37 len=6 span[header_value]="close "
off=45 header_value complete
off=45 len=17 span[header_field]="Transfer-Encoding"
off=63 header_field complete
off=64 len=8 span[header_value]="chunked "
off=74 header_value complete
off=76 headers complete method=1 v=1/1 flags=20a content_length=0
off=79 chunk header len=5
off=79 len=5 span[body]="ABCDE"
off=84 error code=2 reason="Expected LF after chunk data"
```

### Chunk data not terminated by CRLF (lenient)

<!-- meta={"type": "request-lenient-optional-crlf-after-chunk" } -->

```http
GET / HTTP/1.1
Host: a
Connection: close 
Transfer-Encoding: chunked 

5
ABCDE0

```

```log
off=0 message begin
off=0 len=3 span[method]="GET"
off=3 method complete
off=4 len=1 span[url]="/"
off=6 url complete
off=11 len=3 span[version]="1.1"
off=14 version complete
off=16 len=4 span[header_field]="Host"
off=21 header_field complete
off=22 len=1 span[header_value]="a"
off=25 header_value complete
off=25 len=10 span[header_field]="Connection"
off=36 header_field complete
off=37 len=6 span[header_value]="close "
off=45 header_value complete
off=45 len=17 span[header_field]="Transfer-Encoding"
off=63 header_field complete
off=64 len=8 span[header_value]="chunked "
off=74 header_value complete
off=76 headers complete method=1 v=1/1 flags=20a content_length=0
off=79 chunk header len=5
off=79 len=5 span[body]="ABCDE"
off=84 chunk complete
off=87 chunk header len=0
```

## Space after chunk header

<!-- meta={"type": "request"} -->
```http
PUT /url HTTP/1.1
Transfer-Encoding: chunked

a \r\n0123456789
0


```

```log
off=0 message begin
off=0 len=3 span[method]="PUT"
off=3 method complete
off=4 len=4 span[url]="/url"
off=9 url complete
off=14 len=3 span[version]="1.1"
off=17 version complete
off=19 len=17 span[header_field]="Transfer-Encoding"
off=37 header_field complete
off=38 len=7 span[header_value]="chunked"
off=47 header_value complete
off=49 headers complete method=4 v=1/1 flags=208 content_length=0
off=51 error code=12 reason="Invalid character in chunk size"
```

## Space after chunk header (lenient)

<!-- meta={"type": "request-lenient-spaces-after-chunk-size"} -->
```http
PUT /url HTTP/1.1
Transfer-Encoding: chunked

a \r\n0123456789
0


```

```log
off=0 message begin
off=0 len=3 span[method]="PUT"
off=3 method complete
off=4 len=4 span[url]="/url"
off=9 url complete
off=14 len=3 span[version]="1.1"
off=17 version complete
off=19 len=17 span[header_field]="Transfer-Encoding"
off=37 header_field complete
off=38 len=7 span[header_value]="chunked"
off=47 header_value complete
off=49 headers complete method=4 v=1/1 flags=208 content_length=0
off=53 chunk header len=10
off=53 len=10 span[body]="0123456789"
off=65 chunk complete
off=68 chunk header len=0
off=70 chunk complete
off=70 message complete
```
