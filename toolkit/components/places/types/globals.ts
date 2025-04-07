export {};

declare global {

  // We use `Extension` from the extensions code, but importing that requires
  // a lot of set-up, and so we skip it for now.
  type Extension = any;

}
