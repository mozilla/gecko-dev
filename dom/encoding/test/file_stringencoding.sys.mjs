export function checkFromESM(is_op) {
  is_op(new TextDecoder().encoding, "utf-8", "ESM should have TextDecoder");
  is_op(new TextEncoder().encoding, "utf-8", "ESM should have TextEncoder");
}
