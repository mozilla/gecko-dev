/**
 * NOTE: Do not modify this file by hand.
 * Content was generated from source .webidl files.
 */

/// <reference no-default-lib="true" />
/// <reference lib="es2023" />

interface Principal extends nsIPrincipal {}
interface URI extends nsIURI {}
interface WindowProxy extends Window {}

type HTMLCollectionOf<T> = any;
type IsInstance<T> = (obj: any) => obj is T;
type NodeListOf<T> = any;

/////////////////////////////
/// Window APIs
/////////////////////////////

interface DictWithAllowSharedBufferSource {
    allowSharedArrayBuffer?: ArrayBuffer;
    allowSharedArrayBufferView?: ArrayBufferView;
    arrayBuffer?: ArrayBuffer;
    arrayBufferView?: ArrayBufferView;
}

interface ObservableArrayCallbacks {
    deleteBooleanCallback?: SetDeleteBooleanCallback;
    deleteInterfaceCallback?: SetDeleteInterfaceCallback;
    deleteObjectCallback?: SetDeleteObjectCallback;
    setBooleanCallback?: SetDeleteBooleanCallback;
    setInterfaceCallback?: SetDeleteInterfaceCallback;
    setObjectCallback?: SetDeleteObjectCallback;
}

interface TestInterfaceAsyncIterableSingleOptions {
    failToInit?: boolean;
}

interface TestInterfaceAsyncIteratorOptions {
    blockingPromises?: Promise<any>[];
    failNextAfter?: number;
    multiplier?: number;
    throwFromNext?: boolean;
    throwFromReturn?: TestThrowingCallback;
}

interface TestInterfaceJSDictionary {
    anyMember?: any;
    anySequenceMember?: any[];
    innerDictionary?: TestInterfaceJSDictionary2;
    objectMember?: any;
    objectOrStringMember?: any;
    objectRecordMember?: Record<string, any>;
}

interface TestInterfaceJSDictionary2 {
    innerObject?: any;
}

interface TestInterfaceJSUnionableDictionary {
    anyMember?: any;
    objectMember?: any;
}

interface AnyCallback {
}


interface Location {
}

interface Node {
}

interface TestFunctions {
    allowSharedArrayBuffer: ArrayBuffer;
    allowSharedArrayBufferView: ArrayBufferView;
    arrayBuffer: ArrayBuffer;
    arrayBufferView: ArrayBufferView;
    clampedNullableOctet: number | null;
    enforcedNullableOctet: number | null;
    readonly one: number;
    sequenceOfAllowSharedArrayBuffer: ArrayBuffer[];
    sequenceOfAllowSharedArrayBufferView: ArrayBufferView[];
    sequenceOfArrayBuffer: ArrayBuffer[];
    sequenceOfArrayBufferView: ArrayBufferView[];
    readonly two: number;
    readonly wrapperCachedNonISupportsObject: WrapperCachedNonISupportsTestInterface;
    getLongLiteralString(): string;
    getMediumLiteralString(): string;
    getShortLiteralString(): string;
    getStringDataAsAString(): string;
    getStringDataAsAString(length: number): string;
    getStringDataAsDOMString(length?: number): string;
    getStringType(str: string): StringType;
    getStringbufferString(input: string): string;
    setStringData(arg: string): void;
    staticAndNonStaticOverload(): boolean;
    staticAndNonStaticOverload(foo?: number): boolean;
    stringbufferMatchesStored(str: string): boolean;
    testAllowShared(buffer: ArrayBufferView): void;
    testAllowShared(buffer: ArrayBuffer): void;
    testDictWithAllowShared(buffer?: DictWithAllowSharedBufferSource): void;
    testNotAllowShared(buffer: ArrayBufferView): void;
    testNotAllowShared(buffer: ArrayBuffer): void;
    testNotAllowShared(buffer: string): void;
    testThrowNsresult(): void;
    testThrowNsresultFromNative(): void;
    testUnionOfAllowSharedBuffferSource(foo: ArrayBuffer | ArrayBufferView): void;
    testUnionOfBuffferSource(foo: ArrayBuffer | ArrayBufferView | string): void;
    toJSON(): any;
}

declare var TestFunctions: {
    prototype: TestFunctions;
    new(): TestFunctions;
    isInstance: IsInstance<TestFunctions>;
    passThroughCallbackPromise(callback: PromiseReturner): Promise<any>;
    passThroughPromise(arg: any): Promise<any>;
    throwToRejectPromise(): Promise<any>;
    throwUncatchableException(): void;
};

interface TestInterfaceAsyncIterableDouble {
}

declare var TestInterfaceAsyncIterableDouble: {
    prototype: TestInterfaceAsyncIterableDouble;
    new(): TestInterfaceAsyncIterableDouble;
    isInstance: IsInstance<TestInterfaceAsyncIterableDouble>;
};

interface TestInterfaceAsyncIterableDoubleUnion {
}

declare var TestInterfaceAsyncIterableDoubleUnion: {
    prototype: TestInterfaceAsyncIterableDoubleUnion;
    new(): TestInterfaceAsyncIterableDoubleUnion;
    isInstance: IsInstance<TestInterfaceAsyncIterableDoubleUnion>;
};

interface TestInterfaceAsyncIterableSingle {
}

declare var TestInterfaceAsyncIterableSingle: {
    prototype: TestInterfaceAsyncIterableSingle;
    new(options?: TestInterfaceAsyncIterableSingleOptions): TestInterfaceAsyncIterableSingle;
    isInstance: IsInstance<TestInterfaceAsyncIterableSingle>;
};

interface TestInterfaceAsyncIterableSingleWithArgs {
    readonly returnCallCount: number;
    readonly returnLastCalledWith: any;
}

declare var TestInterfaceAsyncIterableSingleWithArgs: {
    prototype: TestInterfaceAsyncIterableSingleWithArgs;
    new(): TestInterfaceAsyncIterableSingleWithArgs;
    isInstance: IsInstance<TestInterfaceAsyncIterableSingleWithArgs>;
};

interface TestInterfaceIterableDouble {
    forEach(callbackfn: (value: string, key: string, parent: TestInterfaceIterableDouble) => void, thisArg?: any): void;
}

declare var TestInterfaceIterableDouble: {
    prototype: TestInterfaceIterableDouble;
    new(): TestInterfaceIterableDouble;
    isInstance: IsInstance<TestInterfaceIterableDouble>;
};

interface TestInterfaceIterableDoubleUnion {
    forEach(callbackfn: (value: string | number, key: string, parent: TestInterfaceIterableDoubleUnion) => void, thisArg?: any): void;
}

declare var TestInterfaceIterableDoubleUnion: {
    prototype: TestInterfaceIterableDoubleUnion;
    new(): TestInterfaceIterableDoubleUnion;
    isInstance: IsInstance<TestInterfaceIterableDoubleUnion>;
};

interface TestInterfaceIterableSingle {
    readonly length: number;
    forEach(callbackfn: (value: number, key: number, parent: TestInterfaceIterableSingle) => void, thisArg?: any): void;
    [index: number]: number;
}

declare var TestInterfaceIterableSingle: {
    prototype: TestInterfaceIterableSingle;
    new(): TestInterfaceIterableSingle;
    isInstance: IsInstance<TestInterfaceIterableSingle>;
};

interface TestInterfaceJSEventMap {
    "something": Event;
}

interface TestInterfaceJS extends EventTarget {
    readonly anyArg: any;
    anyAttr: any;
    readonly objectArg: any;
    objectAttr: any;
    onsomething: ((this: TestInterfaceJS, ev: Event) => any) | null;
    anySequenceLength(seq: any[]): number;
    convertSVS(svs: string): string;
    getCallerPrincipal(): string;
    getDictionaryArg(): TestInterfaceJSDictionary;
    getDictionaryAttr(): TestInterfaceJSDictionary;
    objectSequenceLength(seq: any[]): number;
    pingPongAny(arg: any): any;
    pingPongDictionary(dict?: TestInterfaceJSDictionary): TestInterfaceJSDictionary;
    pingPongDictionaryOrLong(dictOrLong?: TestInterfaceJSUnionableDictionary | number): number;
    pingPongNullableUnion(something: TestInterfaceJS | number | null): TestInterfaceJS | number | null;
    pingPongObject(obj: any): any;
    pingPongObjectOrString(objOrString: any): any;
    pingPongRecord(rec: Record<string, any>): string;
    pingPongUnion(something: TestInterfaceJS | number): TestInterfaceJS | number;
    pingPongUnionContainingNull(something: TestInterfaceJS | string): string | TestInterfaceJS;
    returnBadUnion(): Location | TestInterfaceJS;
    setDictionaryAttr(dict?: TestInterfaceJSDictionary): void;
    testPromiseWithDOMExceptionThrowingPromiseInit(): Promise<void>;
    testPromiseWithDOMExceptionThrowingThenFunction(): Promise<void>;
    testPromiseWithDOMExceptionThrowingThenable(): Promise<void>;
    testPromiseWithThrowingChromePromiseInit(): Promise<void>;
    testPromiseWithThrowingChromeThenFunction(): Promise<void>;
    testPromiseWithThrowingChromeThenable(): Promise<void>;
    testPromiseWithThrowingContentPromiseInit(func: Function): Promise<void>;
    testPromiseWithThrowingContentThenFunction(func: AnyCallback): Promise<void>;
    testPromiseWithThrowingContentThenable(thenable: any): Promise<void>;
    testSequenceOverload(arg: string[]): void;
    testSequenceOverload(arg: string): void;
    testSequenceUnion(arg: string[] | string): void;
    testThrowCallbackError(callback: Function): void;
    testThrowDOMException(): void;
    testThrowError(): void;
    testThrowSelfHosted(): void;
    testThrowTypeError(): void;
    testThrowXraySelfHosted(): void;
    addEventListener<K extends keyof TestInterfaceJSEventMap>(type: K, listener: (this: TestInterfaceJS, ev: TestInterfaceJSEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
    addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
    removeEventListener<K extends keyof TestInterfaceJSEventMap>(type: K, listener: (this: TestInterfaceJS, ev: TestInterfaceJSEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
    removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
}

declare var TestInterfaceJS: {
    prototype: TestInterfaceJS;
    new(anyArg?: any, objectArg?: any, dictionaryArg?: TestInterfaceJSDictionary): TestInterfaceJS;
    isInstance: IsInstance<TestInterfaceJS>;
};

interface TestInterfaceLength {
}

declare var TestInterfaceLength: {
    prototype: TestInterfaceLength;
    new(arg: boolean): TestInterfaceLength;
    isInstance: IsInstance<TestInterfaceLength>;
};

interface TestInterfaceMaplike {
    clearInternal(): void;
    deleteInternal(aKey: string): boolean;
    getInternal(aKey: string): number;
    hasInternal(aKey: string): boolean;
    setInternal(aKey: string, aValue: number): void;
    forEach(callbackfn: (value: number, key: string, parent: TestInterfaceMaplike) => void, thisArg?: any): void;
}

declare var TestInterfaceMaplike: {
    prototype: TestInterfaceMaplike;
    new(): TestInterfaceMaplike;
    isInstance: IsInstance<TestInterfaceMaplike>;
};

interface TestInterfaceMaplikeJSObject {
    clearInternal(): void;
    deleteInternal(aKey: string): boolean;
    getInternal(aKey: string): any;
    hasInternal(aKey: string): boolean;
    setInternal(aKey: string, aObject: any): void;
    forEach(callbackfn: (value: any, key: string, parent: TestInterfaceMaplikeJSObject) => void, thisArg?: any): void;
}

declare var TestInterfaceMaplikeJSObject: {
    prototype: TestInterfaceMaplikeJSObject;
    new(): TestInterfaceMaplikeJSObject;
    isInstance: IsInstance<TestInterfaceMaplikeJSObject>;
};

interface TestInterfaceMaplikeObject {
    clearInternal(): void;
    deleteInternal(aKey: string): boolean;
    getInternal(aKey: string): TestInterfaceMaplike | null;
    hasInternal(aKey: string): boolean;
    setInternal(aKey: string): void;
    forEach(callbackfn: (value: TestInterfaceMaplike, key: string, parent: TestInterfaceMaplikeObject) => void, thisArg?: any): void;
}

declare var TestInterfaceMaplikeObject: {
    prototype: TestInterfaceMaplikeObject;
    new(): TestInterfaceMaplikeObject;
    isInstance: IsInstance<TestInterfaceMaplikeObject>;
};

interface TestInterfaceObservableArray {
    observableArrayBoolean: boolean[];
    observableArrayInterface: TestInterfaceObservableArray[];
    observableArrayObject: any[];
    booleanAppendElementInternal(value: boolean): void;
    booleanElementAtInternal(index: number): boolean;
    booleanLengthInternal(): number;
    booleanRemoveLastElementInternal(): void;
    booleanReplaceElementAtInternal(index: number, value: boolean): void;
    interfaceAppendElementInternal(value: TestInterfaceObservableArray): void;
    interfaceElementAtInternal(index: number): TestInterfaceObservableArray;
    interfaceLengthInternal(): number;
    interfaceRemoveLastElementInternal(): void;
    interfaceReplaceElementAtInternal(index: number, value: TestInterfaceObservableArray): void;
    objectAppendElementInternal(value: any): void;
    objectElementAtInternal(index: number): any;
    objectLengthInternal(): number;
    objectRemoveLastElementInternal(): void;
    objectReplaceElementAtInternal(index: number, value: any): void;
}

declare var TestInterfaceObservableArray: {
    prototype: TestInterfaceObservableArray;
    new(callbacks?: ObservableArrayCallbacks): TestInterfaceObservableArray;
    isInstance: IsInstance<TestInterfaceObservableArray>;
};

interface TestInterfaceSetlike {
    forEach(callbackfn: (value: string, key: string, parent: TestInterfaceSetlike) => void, thisArg?: any): void;
}

declare var TestInterfaceSetlike: {
    prototype: TestInterfaceSetlike;
    new(): TestInterfaceSetlike;
    isInstance: IsInstance<TestInterfaceSetlike>;
};

interface TestInterfaceSetlikeNode {
    forEach(callbackfn: (value: Node, key: Node, parent: TestInterfaceSetlikeNode) => void, thisArg?: any): void;
}

declare var TestInterfaceSetlikeNode: {
    prototype: TestInterfaceSetlikeNode;
    new(): TestInterfaceSetlikeNode;
    isInstance: IsInstance<TestInterfaceSetlikeNode>;
};

interface TestTrialInterface {
}

declare var TestTrialInterface: {
    prototype: TestTrialInterface;
    new(): TestTrialInterface;
    isInstance: IsInstance<TestTrialInterface>;
};

interface WrapperCachedNonISupportsTestInterface {
}

declare var WrapperCachedNonISupportsTestInterface: {
    prototype: WrapperCachedNonISupportsTestInterface;
    new(): WrapperCachedNonISupportsTestInterface;
    isInstance: IsInstance<WrapperCachedNonISupportsTestInterface>;
};

declare namespace TestUtils {
    function gc(): Promise<void>;
}

interface PromiseReturner {
    (): any;
}

interface SetDeleteBooleanCallback {
    (value: boolean, index: number): void;
}

interface SetDeleteInterfaceCallback {
    (value: TestInterfaceObservableArray, index: number): void;
}

interface SetDeleteObjectCallback {
    (value: any, index: number): void;
}

interface TestThrowingCallback {
    (): void;
}

interface HTMLElementTagNameMap {
}

interface HTMLElementDeprecatedTagNameMap {
}

interface SVGElementTagNameMap {
}

interface MathMLElementTagNameMap {
}

/** @deprecated Directly use HTMLElementTagNameMap or SVGElementTagNameMap as appropriate, instead. */
type ElementTagNameMap = HTMLElementTagNameMap & Pick<SVGElementTagNameMap, Exclude<keyof SVGElementTagNameMap, keyof HTMLElementTagNameMap>>;

type StringType = "inline" | "literal" | "other" | "stringbuffer";

/////////////////////////////
/// Window Iterable APIs
/////////////////////////////

interface TestInterfaceIterableDouble {
    [Symbol.iterator](): IterableIterator<[string, string]>;
    entries(): IterableIterator<[string, string]>;
    keys(): IterableIterator<string>;
    values(): IterableIterator<string>;
}

interface TestInterfaceIterableDoubleUnion {
    [Symbol.iterator](): IterableIterator<[string, string | number]>;
    entries(): IterableIterator<[string, string | number]>;
    keys(): IterableIterator<string>;
    values(): IterableIterator<string | number>;
}

interface TestInterfaceIterableSingle {
    [Symbol.iterator](): IterableIterator<number>;
    entries(): IterableIterator<[number, number]>;
    keys(): IterableIterator<number>;
    values(): IterableIterator<number>;
}

interface TestInterfaceJS {
    anySequenceLength(seq: Iterable<any>): number;
    objectSequenceLength(seq: Iterable<any>): number;
    testSequenceOverload(arg: Iterable<string>): void;
    testSequenceUnion(arg: Iterable<string> | string): void;
}

interface TestInterfaceMaplike extends Map<string, number> {
}

interface TestInterfaceMaplikeJSObject extends ReadonlyMap<string, any> {
}

interface TestInterfaceMaplikeObject extends ReadonlyMap<string, TestInterfaceMaplike> {
}

interface TestInterfaceSetlike extends Set<string> {
}

interface TestInterfaceSetlikeNode extends Set<Node> {
}

/////////////////////////////
/// Window Async Iterable APIs
/////////////////////////////

interface TestInterfaceAsyncIterableDouble {
    [Symbol.asyncIterator](): AsyncIterableIterator<[string, string]>;
    entries(): AsyncIterableIterator<[string, string]>;
    keys(): AsyncIterableIterator<string>;
    values(): AsyncIterableIterator<string>;
}

interface TestInterfaceAsyncIterableDoubleUnion {
    [Symbol.asyncIterator](): AsyncIterableIterator<[string, string | number]>;
    entries(): AsyncIterableIterator<[string, string | number]>;
    keys(): AsyncIterableIterator<string>;
    values(): AsyncIterableIterator<string | number>;
}

interface TestInterfaceAsyncIterableSingle {
    [Symbol.asyncIterator](): AsyncIterableIterator<number>;
    values(): AsyncIterableIterator<number>;
}

interface TestInterfaceAsyncIterableSingleWithArgs {
    [Symbol.asyncIterator](options?: TestInterfaceAsyncIteratorOptions): AsyncIterableIterator<number>;
    values(options?: TestInterfaceAsyncIteratorOptions): AsyncIterableIterator<number>;
}
