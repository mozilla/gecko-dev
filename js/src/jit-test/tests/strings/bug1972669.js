let bigstr = "A".repeat(715827882);
bigstr = bigstr.slice(0, -1) + "\\";

try {
  JSON.stringify(bigstr);
} catch {}
