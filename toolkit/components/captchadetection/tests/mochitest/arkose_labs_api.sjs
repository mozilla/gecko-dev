function handleRequest(request, response) {
  response.setHeader("Content-Type", "application/json");
  response.write(
    JSON.stringify({
      response: "something",
      solved: request.queryString === "solved" ? true : null,
    })
  );
}
