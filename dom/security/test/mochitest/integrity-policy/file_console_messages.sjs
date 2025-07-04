const html = `
<!DOCTYPE html>
<html>
<body>
<script src="file_console_messages.sjs?${encodeURIComponent(JSON.stringify({ res: "js" }))}"></script>
<link rel="stylesheet" href="file_console_messages.sjs?${encodeURIComponent(JSON.stringify({ res: "css" }))}">
</body>
</html>
`;

function handleRequest(request, response) {
  const payload = JSON.parse(decodeURIComponent(request.queryString));

  if (!payload || !payload.res) {
    response.setStatusLine(request.httpVersion, 400, "Bad Request");
    return;
  }

  let body = "";
  switch (payload.res) {
    case "html":
      body = html;
      response.setHeader("Content-Type", "text/html;charset=UTF-8");
      break;
    case "js":
      response.setHeader(
        "Content-Type",
        "application/javascript;charset=UTF-8"
      );
      break;
    case "css":
      response.setHeader("Content-Type", "text/css;charset=UTF-8");
      break;
    default:
      response.setStatusLine(request.httpVersion, 400, "Bad Request");
      return;
  }

  for (const [name, value] of payload.headers ?? []) {
    response.setHeader(name, value);
  }

  response.write(body);
}
