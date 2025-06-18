/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

export function test_event_handler(doc) {
  doc.documentElement.setAttribute("onclick", "run_me()");
  doc.documentElement.removeAttribute("onclick");
}

export function test_inline_script(doc) {
  let script = doc.createElement("script");
  script.textContent = `throw new Error("unreachable code");`;
  doc.documentElement.append(script);
  script.remove();
}

export function test_data_url_script(doc) {
  let script = doc.createElement("script");
  script.src = `data:text/javascript,throw new Error("unreachable code");`;
  doc.documentElement.append(script);
  script.remove();
}

export function test_eval() {
  // eslint-disable-next-line no-eval
  return eval("1 + 1");
}
