export {};

declare global {

  type Extension = import("resource://gre/modules/Extension.sys.mjs").Extension;

  type DeferredTask = import("resource://gre/modules/DeferredTask.sys.mjs").DeferredTask;

}
