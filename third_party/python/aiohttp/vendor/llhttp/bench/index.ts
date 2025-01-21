import * as assert from "assert";
import { spawnSync } from "child_process";
import { existsSync } from "fs";
import { resolve } from "path";

function request(tpl: TemplateStringsArray): string {
  return tpl.raw[0].replace(/^\s+/gm, '').replace(/\n/gm, '').replace(/\\r/gm, '\r').replace(/\\n/gm, '\n')
}

const urlExecutable = resolve(__dirname, "../test/tmp/url-url-c");
const httpExecutable = resolve(__dirname, "../test/tmp/http-request-c");

const httpRequests: Record<string, string> = {
  "seanmonstar/httparse": request`
    GET /wp-content/uploads/2010/03/hello-kitty-darth-vader-pink.jpg HTTP/1.1\r\n
    Host: www.kittyhell.com\r\n
    User-Agent: Mozilla/5.0 (Macintosh; U; Intel Mac OS X 10.6; ja-JP-mac; rv:1.9.2.3) Gecko/20100401 Firefox/3.6.3 Pathtraq/0.9\r\n
    Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n
    Accept-Language: ja,en-us;q=0.7,en;q=0.3\r\n
    Accept-Encoding: gzip,deflate\r\n
    Accept-Charset: Shift_JIS,utf-8;q=0.7,*;q=0.7\r\n
    Keep-Alive: 115\r\n
    Connection: keep-alive\r\n
    Cookie: wp_ozh_wsa_visits=2; wp_ozh_wsa_visit_lasttime=xxxxxxxxxx; __utma=xxxxxxxxx.xxxxxxxxxx.xxxxxxxxxx.xxxxxxxxxx.xxxxxxxxxx.x; __utmz=xxxxxxxxx.xxxxxxxxxx.x.x.utmccn=(referral)|utmcsr=reader.livedoor.com|utmcct=/reader/|utmcmd=referral\r\n\r\n
  `,
  "nodejs/http-parser": request` 
    POST /joyent/http-parser HTTP/1.1\r\n
    Host: github.com\r\n
    DNT: 1\r\n
    Accept-Encoding: gzip, deflate, sdch\r\n
    Accept-Language: ru-RU,ru;q=0.8,en-US;q=0.6,en;q=0.4\r\n
    User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_10_1) 
    AppleWebKit/537.36 (KHTML, like Gecko) 
    Chrome/39.0.2171.65 Safari/537.36\r\n
    Accept: text/html,application/xhtml+xml,application/xml;q=0.9,
    image/webp,*/*;q=0.8\r\n
    Referer: https://github.com/joyent/http-parser\r\n
    Connection: keep-alive\r\n
    Transfer-Encoding: chunked\r\n
    Cache-Control: max-age=0\r\n\r\nb\r\nhello world\r\n0\r\n\r\n
  `
}
const urlRequest = "http://example.com/path/to/file?query=value#fragment";

if (!existsSync(urlExecutable) || !existsSync(urlExecutable)) {
  console.error(
    "\x1b[31m\x1b[1mPlease run npm test in order to create required executables."
  );
  process.exit(1);
}

if (process.argv[2] === "loop") {
  const reqName = process.argv[3];
  const request = httpRequests[reqName]!;
  
  assert(request, `Unknown request name: "${reqName}"`);
  spawnSync(httpExecutable, ["loop", request], { stdio: "inherit" });
  process.exit(0);
}

if (!process.argv[2] || process.argv[2] === "url") {
  console.log("url (C)");
  spawnSync(urlExecutable, ["bench", urlRequest], { stdio: "inherit" });
}

if (!process.argv[2] || process.argv[2] === "http") {
  for (const [name, request] of Object.entries(httpRequests)) {
    console.log('http: "%s" (C)', name);
    spawnSync(httpExecutable, ["bench", request], { stdio: "inherit" });
  }
}
