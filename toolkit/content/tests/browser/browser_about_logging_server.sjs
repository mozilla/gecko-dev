function handleRequest(request, response) {
  // Allow cross-origin, so you can XHR to it!
  response.setHeader("Access-Control-Allow-Origin", "*", false);
  // Avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "text/plain", false);

  // A real JWT token as received by api.profiler.firefox.com
  // The profile token contained inside this JWT is
  // "24j1wmckznh8sj22zg1tsmg47dyfdtprj0g41s8".
  response.write(
    "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJwcm9maWxlVG9rZW4iOiIyNGoxd21ja3puaDhzajIyemcxdHNtZzQ3ZHlmZHRwcmowZzQxczgiLCJpYXQiOjE3NDM0MjM1NDV9.XDZtFK4WACo1x3px0zCRNL0gCjWBrYMqPGM-vMG11CE"
  );
}
