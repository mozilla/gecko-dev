
// Exports for all modules redirected here by a catch-all rule in tsconfig.json.
export var
  AddonWrapper, AppConstants, GeckoViewConnection, GeckoViewWebExtension,
  IndexedDB, JSONFile, Log, UrlbarUtils, WebExtensionDescriptorActor;

/**
 * A stub type for the "class" from EventEmitter.sys.mjs.
 * TODO: Convert EventEmitter.sys.mjs into a proper class.
 */
export declare class EventEmitter {
  emit(event: string, ...args: any[]): void;
  on(event: string, listener: callback): void;
  once(event: string, listener: callback): Promise<any>;
  off(event: string, listener: callback): void;
}
