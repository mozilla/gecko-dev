function handleRequest(request, response) {
  response.setHeader("Content-Type", "application/json");
  response.write(
    JSON.stringify({
      success: true,
      num_solutions_required: 2,
    })
  );
}
