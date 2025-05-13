/**
 * Types specific to toolkit/extensions code.
 */
declare global {
  type BaseContext = import("../ExtensionCommon.sys.mjs").BaseContext;
  type ExtensionChild = import("../ExtensionChild.sys.mjs").ExtensionChild;
  type Extension = import("../Extension.sys.mjs").Extension;
  type callback = (...any) => any;
  type DOMWindow = Window;

  interface nsIDOMProcessChild {
    getActor(name: "ProcessConduits"): ProcessConduitsChild;
  }

  interface WebExtensionContentScript {
    userScriptOptions: { scriptMetadata: object };
  }

  interface WebExtensionPolicy {
    extension: Extension;
    debugName: string;
    instanceId: string;
    optionalPermissions: string[];
  }

  // Can't define a const generic parameter in jsdocs yet.
  // https://github.com/microsoft/TypeScript/issues/56634
  function ConduitGen<const Send>(_, init: Init<Send>, _actor?): Conduit<Send>;
  type Items<A> = A extends ReadonlyArray<infer U extends string> ? U : never;
}

import { PointConduit, ProcessConduitsChild } from "ConduitsChild.sys.mjs";
import { ConduitAddress } from "ConduitsParent.sys.mjs";

type Conduit<Send> = PointConduit & { [s in `send${Items<Send>}`]: callback };
type Init<Send> = ConduitAddress & { send: Send };

type PreferencesNS =
  typeof import("resource://gre/modules/Preferences.sys.mjs").Preferences;

declare module "resource://gre/modules/Preferences.sys.mjs" {
  class Preferences {
    get: PreferencesNS["get"];
  }
}

declare module "resource://testing-common/Assert.sys.mjs" {
  namespace Assert {
    var equal: Assert["equal"];
    var ok: Assert["ok"];
  }
}

declare module "resource://gre/modules/addons/XPIDatabase.sys.mjs" {
  interface AddonWrapper {
    id: string;
    version: string;
  }
}

declare module "resource://gre/modules/IndexedDB.sys.mjs" {
  interface Cursor extends IDBCursor {}

  interface CursorWithValue {
    value: IDBCursorWithValue["value"];
  }

  interface ObjectStore {
    clear: IDBObjectStore["clear"];
    delete: IDBObjectStore["delete"];
    get: IDBObjectStore["get"];
    getKey: (...args: Parameters<IDBObjectStore["getKey"]>) => Promise<any>;
    put: IDBObjectStore["put"];
  }

  interface Transaction {
    abort: IDBTransaction["abort"];
  }
}
