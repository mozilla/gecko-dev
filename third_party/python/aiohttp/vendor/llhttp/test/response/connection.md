Connection header
=================

## Proxy-Connection

<!-- meta={"type": "response"} -->
```http
HTTP/1.1 200 OK
Content-Type: text/html; charset=UTF-8
Content-Length: 11
Proxy-Connection: close
Date: Thu, 31 Dec 2009 20:55:48 +0000

hello world
```

```log
off=0 message begin
off=5 len=3 span[version]="1.1"
off=8 version complete
off=13 len=2 span[status]="OK"
off=17 status complete
off=17 len=12 span[header_field]="Content-Type"
off=30 header_field complete
off=31 len=24 span[header_value]="text/html; charset=UTF-8"
off=57 header_value complete
off=57 len=14 span[header_field]="Content-Length"
off=72 header_field complete
off=73 len=2 span[header_value]="11"
off=77 header_value complete
off=77 len=16 span[header_field]="Proxy-Connection"
off=94 header_field complete
off=95 len=5 span[header_value]="close"
off=102 header_value complete
off=102 len=4 span[header_field]="Date"
off=107 header_field complete
off=108 len=31 span[header_value]="Thu, 31 Dec 2009 20:55:48 +0000"
off=141 header_value complete
off=143 headers complete status=200 v=1/1 flags=22 content_length=11
off=143 len=11 span[body]="hello world"
off=154 message complete
```

## HTTP/1.0 with keep-alive and EOF-terminated 200 status

There is no `Content-Length` in this response, so even though the
`keep-alive` is on - it should read until EOF.

<!-- meta={"type": "response"} -->
```http
HTTP/1.0 200 OK
Connection: keep-alive

HTTP/1.0 200 OK
```

```log
off=0 message begin
off=5 len=3 span[version]="1.0"
off=8 version complete
off=13 len=2 span[status]="OK"
off=17 status complete
off=17 len=10 span[header_field]="Connection"
off=28 header_field complete
off=29 len=10 span[header_value]="keep-alive"
off=41 header_value complete
off=43 headers complete status=200 v=1/0 flags=1 content_length=0
off=43 len=15 span[body]="HTTP/1.0 200 OK"
```

## HTTP/1.0 with keep-alive and 204 status

Responses with `204` status cannot have a body.

<!-- meta={"type": "response"} -->
```http
HTTP/1.0 204 No content
Connection: keep-alive

HTTP/1.0 200 OK
```

```log
off=0 message begin
off=5 len=3 span[version]="1.0"
off=8 version complete
off=13 len=10 span[status]="No content"
off=25 status complete
off=25 len=10 span[header_field]="Connection"
off=36 header_field complete
off=37 len=10 span[header_value]="keep-alive"
off=49 header_value complete
off=51 headers complete status=204 v=1/0 flags=1 content_length=0
off=51 message complete
off=51 reset
off=51 message begin
off=56 len=3 span[version]="1.0"
off=59 version complete
off=64 len=2 span[status]="OK"
```

## HTTP/1.1 with EOF-terminated 200 status

There is no `Content-Length` in this response, so even though the
`keep-alive` is on (implicitly in HTTP 1.1) - it should read until EOF.

<!-- meta={"type": "response"} -->
```http
HTTP/1.1 200 OK

HTTP/1.1 200 OK
```

```log
off=0 message begin
off=5 len=3 span[version]="1.1"
off=8 version complete
off=13 len=2 span[status]="OK"
off=17 status complete
off=19 headers complete status=200 v=1/1 flags=0 content_length=0
off=19 len=15 span[body]="HTTP/1.1 200 OK"
```

## HTTP/1.1 with 204 status

Responses with `204` status cannot have a body.

<!-- meta={"type": "response"} -->
```http
HTTP/1.1 204 No content

HTTP/1.1 200 OK
```

```log
off=0 message begin
off=5 len=3 span[version]="1.1"
off=8 version complete
off=13 len=10 span[status]="No content"
off=25 status complete
off=27 headers complete status=204 v=1/1 flags=0 content_length=0
off=27 message complete
off=27 reset
off=27 message begin
off=32 len=3 span[version]="1.1"
off=35 version complete
off=40 len=2 span[status]="OK"
```

## HTTP/1.1 with keep-alive disabled and 204 status

<!-- meta={"type": "response" } -->
```http
HTTP/1.1 204 No content
Connection: close

HTTP/1.1 200 OK
```

```log
off=0 message begin
off=5 len=3 span[version]="1.1"
off=8 version complete
off=13 len=10 span[status]="No content"
off=25 status complete
off=25 len=10 span[header_field]="Connection"
off=36 header_field complete
off=37 len=5 span[header_value]="close"
off=44 header_value complete
off=46 headers complete status=204 v=1/1 flags=2 content_length=0
off=46 message complete
off=47 error code=5 reason="Data after `Connection: close`"
```

## HTTP/1.1 with keep-alive disabled, content-length (lenient)

Parser should discard extra request in lenient mode.

<!-- meta={"type": "response-lenient-data-after-close" } -->
```http
HTTP/1.1 200 No content
Content-Length: 5
Connection: close

2ad731e3-4dcd-4f70-b871-0ad284b29ffc
```

```log
off=0 message begin
off=5 len=3 span[version]="1.1"
off=8 version complete
off=13 len=10 span[status]="No content"
off=25 status complete
off=25 len=14 span[header_field]="Content-Length"
off=40 header_field complete
off=41 len=1 span[header_value]="5"
off=44 header_value complete
off=44 len=10 span[header_field]="Connection"
off=55 header_field complete
off=56 len=5 span[header_value]="close"
off=63 header_value complete
off=65 headers complete status=200 v=1/1 flags=22 content_length=5
off=65 len=5 span[body]="2ad73"
off=70 message complete
```

## HTTP/1.1 with keep-alive disabled, content-length

Parser should discard extra request in strict mode.

<!-- meta={"type": "response" } -->
```http
HTTP/1.1 200 No content
Content-Length: 5
Connection: close

2ad731e3-4dcd-4f70-b871-0ad284b29ffc
```

```log
off=0 message begin
off=5 len=3 span[version]="1.1"
off=8 version complete
off=13 len=10 span[status]="No content"
off=25 status complete
off=25 len=14 span[header_field]="Content-Length"
off=40 header_field complete
off=41 len=1 span[header_value]="5"
off=44 header_value complete
off=44 len=10 span[header_field]="Connection"
off=55 header_field complete
off=56 len=5 span[header_value]="close"
off=63 header_value complete
off=65 headers complete status=200 v=1/1 flags=22 content_length=5
off=65 len=5 span[body]="2ad73"
off=70 message complete
off=71 error code=5 reason="Data after `Connection: close`"
```

## HTTP/1.1 with keep-alive disabled and 204 status (lenient)

<!-- meta={"type": "response-lenient-keep-alive"} -->
```http
HTTP/1.1 204 No content
Connection: close

HTTP/1.1 200 OK
```

```log
off=0 message begin
off=5 len=3 span[version]="1.1"
off=8 version complete
off=13 len=10 span[status]="No content"
off=25 status complete
off=25 len=10 span[header_field]="Connection"
off=36 header_field complete
off=37 len=5 span[header_value]="close"
off=44 header_value complete
off=46 headers complete status=204 v=1/1 flags=2 content_length=0
off=46 message complete
off=46 reset
off=46 message begin
off=51 len=3 span[version]="1.1"
off=54 version complete
off=59 len=2 span[status]="OK"
```

## HTTP 101 response with Upgrade and Content-Length header

<!-- meta={"type": "response"} -->
```http
HTTP/1.1 101 Switching Protocols
Connection: upgrade
Upgrade: h2c
Content-Length: 4

body\
proto
```

```log
off=0 message begin
off=5 len=3 span[version]="1.1"
off=8 version complete
off=13 len=19 span[status]="Switching Protocols"
off=34 status complete
off=34 len=10 span[header_field]="Connection"
off=45 header_field complete
off=46 len=7 span[header_value]="upgrade"
off=55 header_value complete
off=55 len=7 span[header_field]="Upgrade"
off=63 header_field complete
off=64 len=3 span[header_value]="h2c"
off=69 header_value complete
off=69 len=14 span[header_field]="Content-Length"
off=84 header_field complete
off=85 len=1 span[header_value]="4"
off=88 header_value complete
off=90 headers complete status=101 v=1/1 flags=34 content_length=4
off=90 message complete
off=90 error code=22 reason="Pause on CONNECT/Upgrade"
```

## HTTP 101 response with Upgrade and Transfer-Encoding header

<!-- meta={"type": "response"} -->
```http
HTTP/1.1 101 Switching Protocols
Connection: upgrade
Upgrade: h2c
Transfer-Encoding: chunked

2
bo
2
dy
0

proto
```

```log
off=0 message begin
off=5 len=3 span[version]="1.1"
off=8 version complete
off=13 len=19 span[status]="Switching Protocols"
off=34 status complete
off=34 len=10 span[header_field]="Connection"
off=45 header_field complete
off=46 len=7 span[header_value]="upgrade"
off=55 header_value complete
off=55 len=7 span[header_field]="Upgrade"
off=63 header_field complete
off=64 len=3 span[header_value]="h2c"
off=69 header_value complete
off=69 len=17 span[header_field]="Transfer-Encoding"
off=87 header_field complete
off=88 len=7 span[header_value]="chunked"
off=97 header_value complete
off=99 headers complete status=101 v=1/1 flags=21c content_length=0
off=99 message complete
off=99 error code=22 reason="Pause on CONNECT/Upgrade"
```

## HTTP 200 response with Upgrade header

<!-- meta={"type": "response"} -->
```http
HTTP/1.1 200 OK
Connection: upgrade
Upgrade: h2c

body
```

```log
off=0 message begin
off=5 len=3 span[version]="1.1"
off=8 version complete
off=13 len=2 span[status]="OK"
off=17 status complete
off=17 len=10 span[header_field]="Connection"
off=28 header_field complete
off=29 len=7 span[header_value]="upgrade"
off=38 header_value complete
off=38 len=7 span[header_field]="Upgrade"
off=46 header_field complete
off=47 len=3 span[header_value]="h2c"
off=52 header_value complete
off=54 headers complete status=200 v=1/1 flags=14 content_length=0
off=54 len=4 span[body]="body"
```

## HTTP 200 response with Upgrade header and Content-Length

<!-- meta={"type": "response"} -->
```http
HTTP/1.1 200 OK
Connection: upgrade
Upgrade: h2c
Content-Length: 4

body
```

```log
off=0 message begin
off=5 len=3 span[version]="1.1"
off=8 version complete
off=13 len=2 span[status]="OK"
off=17 status complete
off=17 len=10 span[header_field]="Connection"
off=28 header_field complete
off=29 len=7 span[header_value]="upgrade"
off=38 header_value complete
off=38 len=7 span[header_field]="Upgrade"
off=46 header_field complete
off=47 len=3 span[header_value]="h2c"
off=52 header_value complete
off=52 len=14 span[header_field]="Content-Length"
off=67 header_field complete
off=68 len=1 span[header_value]="4"
off=71 header_value complete
off=73 headers complete status=200 v=1/1 flags=34 content_length=4
off=73 len=4 span[body]="body"
off=77 message complete
```

## HTTP 200 response with Upgrade header and Transfer-Encoding

<!-- meta={"type": "response"} -->
```http
HTTP/1.1 200 OK
Connection: upgrade
Upgrade: h2c
Transfer-Encoding: chunked

2
bo
2
dy
0


```

```log
off=0 message begin
off=5 len=3 span[version]="1.1"
off=8 version complete
off=13 len=2 span[status]="OK"
off=17 status complete
off=17 len=10 span[header_field]="Connection"
off=28 header_field complete
off=29 len=7 span[header_value]="upgrade"
off=38 header_value complete
off=38 len=7 span[header_field]="Upgrade"
off=46 header_field complete
off=47 len=3 span[header_value]="h2c"
off=52 header_value complete
off=52 len=17 span[header_field]="Transfer-Encoding"
off=70 header_field complete
off=71 len=7 span[header_value]="chunked"
off=80 header_value complete
off=82 headers complete status=200 v=1/1 flags=21c content_length=0
off=85 chunk header len=2
off=85 len=2 span[body]="bo"
off=89 chunk complete
off=92 chunk header len=2
off=92 len=2 span[body]="dy"
off=96 chunk complete
off=99 chunk header len=0
off=101 chunk complete
off=101 message complete
```

## HTTP 304 with Content-Length

<!-- meta={"type": "response"} -->
```http
HTTP/1.1 304 Not Modified
Content-Length: 10


HTTP/1.1 200 OK
Content-Length: 5

hello
```

```log
off=0 message begin
off=5 len=3 span[version]="1.1"
off=8 version complete
off=13 len=12 span[status]="Not Modified"
off=27 status complete
off=27 len=14 span[header_field]="Content-Length"
off=42 header_field complete
off=43 len=2 span[header_value]="10"
off=47 header_value complete
off=49 headers complete status=304 v=1/1 flags=20 content_length=10
off=49 message complete
off=51 reset
off=51 message begin
off=56 len=3 span[version]="1.1"
off=59 version complete
off=64 len=2 span[status]="OK"
off=68 status complete
off=68 len=14 span[header_field]="Content-Length"
off=83 header_field complete
off=84 len=1 span[header_value]="5"
off=87 header_value complete
off=89 headers complete status=200 v=1/1 flags=20 content_length=5
off=89 len=5 span[body]="hello"
off=94 message complete
```

## HTTP 304 with Transfer-Encoding

<!-- meta={"type": "response"} -->
```http
HTTP/1.1 304 Not Modified
Transfer-Encoding: chunked

HTTP/1.1 200 OK
Transfer-Encoding: chunked

5
hello
0

```

```log
off=0 message begin
off=5 len=3 span[version]="1.1"
off=8 version complete
off=13 len=12 span[status]="Not Modified"
off=27 status complete
off=27 len=17 span[header_field]="Transfer-Encoding"
off=45 header_field complete
off=46 len=7 span[header_value]="chunked"
off=55 header_value complete
off=57 headers complete status=304 v=1/1 flags=208 content_length=0
off=57 message complete
off=57 reset
off=57 message begin
off=62 len=3 span[version]="1.1"
off=65 version complete
off=70 len=2 span[status]="OK"
off=74 status complete
off=74 len=17 span[header_field]="Transfer-Encoding"
off=92 header_field complete
off=93 len=7 span[header_value]="chunked"
off=102 header_value complete
off=104 headers complete status=200 v=1/1 flags=208 content_length=0
off=107 chunk header len=5
off=107 len=5 span[body]="hello"
off=114 chunk complete
off=117 chunk header len=0
```

## HTTP 100 first, then 400

<!-- meta={"type": "response"} -->
```http
HTTP/1.1 100 Continue


HTTP/1.1 404 Not Found
Content-Type: text/plain; charset=utf-8
Content-Length: 14
Date: Fri, 15 Sep 2023 19:47:23 GMT
Server: Python/3.10 aiohttp/4.0.0a2.dev0

404: Not Found
```

```log
off=0 message begin
off=5 len=3 span[version]="1.1"
off=8 version complete
off=13 len=8 span[status]="Continue"
off=23 status complete
off=25 headers complete status=100 v=1/1 flags=0 content_length=0
off=25 message complete
off=27 reset
off=27 message begin
off=32 len=3 span[version]="1.1"
off=35 version complete
off=40 len=9 span[status]="Not Found"
off=51 status complete
off=51 len=12 span[header_field]="Content-Type"
off=64 header_field complete
off=65 len=25 span[header_value]="text/plain; charset=utf-8"
off=92 header_value complete
off=92 len=14 span[header_field]="Content-Length"
off=107 header_field complete
off=108 len=2 span[header_value]="14"
off=112 header_value complete
off=112 len=4 span[header_field]="Date"
off=117 header_field complete
off=118 len=29 span[header_value]="Fri, 15 Sep 2023 19:47:23 GMT"
off=149 header_value complete
off=149 len=6 span[header_field]="Server"
off=156 header_field complete
off=157 len=32 span[header_value]="Python/3.10 aiohttp/4.0.0a2.dev0"
off=191 header_value complete
off=193 headers complete status=404 v=1/1 flags=20 content_length=14
off=193 len=14 span[body]="404: Not Found"
off=207 message complete
```

## HTTP 103 first, then 200

<!-- meta={"type": "response"} -->
```http
HTTP/1.1 103 Early Hints
Link: </styles.css>; rel=preload; as=style

HTTP/1.1 200 OK
Date: Wed, 13 Sep 2023 11:09:41 GMT
Connection: keep-alive
Keep-Alive: timeout=5
Content-Length: 17

response content
```

```log
off=0 message begin
off=5 len=3 span[version]="1.1"
off=8 version complete
off=13 len=11 span[status]="Early Hints"
off=26 status complete
off=26 len=4 span[header_field]="Link"
off=31 header_field complete
off=32 len=36 span[header_value]="</styles.css>; rel=preload; as=style"
off=70 header_value complete
off=72 headers complete status=103 v=1/1 flags=0 content_length=0
off=72 message complete
off=72 reset
off=72 message begin
off=77 len=3 span[version]="1.1"
off=80 version complete
off=85 len=2 span[status]="OK"
off=89 status complete
off=89 len=4 span[header_field]="Date"
off=94 header_field complete
off=95 len=29 span[header_value]="Wed, 13 Sep 2023 11:09:41 GMT"
off=126 header_value complete
off=126 len=10 span[header_field]="Connection"
off=137 header_field complete
off=138 len=10 span[header_value]="keep-alive"
off=150 header_value complete
off=150 len=10 span[header_field]="Keep-Alive"
off=161 header_field complete
off=162 len=9 span[header_value]="timeout=5"
off=173 header_value complete
off=173 len=14 span[header_field]="Content-Length"
off=188 header_field complete
off=189 len=2 span[header_value]="17"
off=193 header_value complete
off=195 headers complete status=200 v=1/1 flags=21 content_length=17
off=195 len=16 span[body]="response content"
```