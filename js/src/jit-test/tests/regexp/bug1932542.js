let src = "(?=g)" + "()?".repeat(32767);
try {
  "".match(src);
} catch {}
