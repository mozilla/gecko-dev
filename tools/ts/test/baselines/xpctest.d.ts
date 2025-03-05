/**
 * NOTE: Do not modify this file by hand.
 * Content was generated from source XPCOM .idl files.
 * If you're updating some of the sources, see README for instructions.
 */

declare global {

// https://searchfox.org/mozilla-central/source/js/xpconnect/tests/idl/xpctest_attributes.idl

interface nsIXPCTestObjectReadOnly extends nsISupports {
  readonly strReadOnly: string;
  readonly boolReadOnly: boolean;
  readonly shortReadOnly: i16;
  readonly longReadOnly: i32;
  readonly floatReadOnly: float;
  readonly charReadOnly: string;
  readonly timeReadOnly: PRTime;
}

interface nsIXPCTestObjectReadWrite extends nsISupports {
  stringProperty: string;
  booleanProperty: boolean;
  shortProperty: i16;
  longProperty: i32;
  floatProperty: float;
  charProperty: string;
  timeProperty: PRTime;
}

// https://searchfox.org/mozilla-central/source/js/xpconnect/tests/idl/xpctest_bug809674.idl

interface nsIXPCTestBug809674 extends nsISupports {
  addArgs(x: u32, y: u32): u32;
  addSubMulArgs(x: u32, y: u32, subOut: OutParam<u32>, mulOut: OutParam<u32>): u32;
  addVals(x: any, y: any): any;
  methodNoArgs(): u32;
  methodNoArgsNoRetVal(): void;
  addMany(x1: u32, x2: u32, x3: u32, x4: u32, x5: u32, x6: u32, x7: u32, x8: u32): u32;
  valProperty: any;
  uintProperty: u32;
  methodWithOptionalArgc(): void;
}

// https://searchfox.org/mozilla-central/source/js/xpconnect/tests/idl/xpctest_cenums.idl

}  // global

declare enum nsIXPCTestCEnums_testFlagsExplicit {
  shouldBe1Explicit = 1,
  shouldBe2Explicit = 2,
  shouldBe4Explicit = 4,
  shouldBe8Explicit = 8,
  shouldBe12Explicit = 12,
}

declare enum nsIXPCTestCEnums_testFlagsImplicit {
  shouldBe0Implicit = 0,
  shouldBe1Implicit = 1,
  shouldBe2Implicit = 2,
  shouldBe3Implicit = 3,
  shouldBe5Implicit = 5,
  shouldBe6Implicit = 6,
  shouldBe2AgainImplicit = 2,
  shouldBe3AgainImplicit = 3,
}

declare global {

namespace nsIXPCTestCEnums {
  type testFlagsExplicit = nsIXPCTestCEnums_testFlagsExplicit;
  type testFlagsImplicit = nsIXPCTestCEnums_testFlagsImplicit;
}

interface nsIXPCTestCEnums extends nsISupports, Enums<typeof nsIXPCTestCEnums_testFlagsExplicit & typeof nsIXPCTestCEnums_testFlagsImplicit> {
  readonly testConst?: 1;

  testCEnumInput(abc: nsIXPCTestCEnums.testFlagsExplicit): void;
  testCEnumOutput(): nsIXPCTestCEnums.testFlagsExplicit;
}

// https://searchfox.org/mozilla-central/source/js/xpconnect/tests/idl/xpctest_interfaces.idl

interface nsIXPCTestInterfaceA extends nsISupports {
  name: string;
}

interface nsIXPCTestInterfaceB extends nsISupports {
  name: string;
}

interface nsIXPCTestInterfaceC extends nsISupports {
  someInteger: i32;
}

// https://searchfox.org/mozilla-central/source/js/xpconnect/tests/idl/xpctest_params.idl

interface nsIXPCTestParams extends nsISupports {
  testBoolean(a: boolean, b: InOutParam<boolean>): boolean;
  testOctet(a: u8, b: InOutParam<u8>): u8;
  testShort(a: i16, b: InOutParam<i16>): i16;
  testLong(a: i32, b: InOutParam<i32>): i32;
  testLongLong(a: i64, b: InOutParam<i64>): i64;
  testUnsignedShort(a: u16, b: InOutParam<u16>): u16;
  testUnsignedLong(a: u32, b: InOutParam<u32>): u32;
  testUnsignedLongLong(a: u64, b: InOutParam<u64>): u64;
  testFloat(a: float, b: InOutParam<float>): float;
  testDouble(a: double, b: InOutParam<float>): double;
  testChar(a: string, b: InOutParam<string>): string;
  testString(a: string, b: InOutParam<string>): string;
  testWchar(a: string, b: InOutParam<string>): string;
  testWstring(a: string, b: InOutParam<string>): string;
  testAString(a: string, b: InOutParam<string>): string;
  testAUTF8String(a: string, b: InOutParam<string>): string;
  testACString(a: string, b: InOutParam<string>): string;
  testJsval(a: any, b: InOutParam<any>): any;
  testShortSequence(a: i16[], b: InOutParam<i16[]>): i16[];
  testDoubleSequence(a: double[], b: InOutParam<double[]>): double[];
  testInterfaceSequence(a: nsIXPCTestInterfaceA[], b: InOutParam<nsIXPCTestInterfaceA[]>): nsIXPCTestInterfaceA[];
  testAStringSequence(a: string[], b: InOutParam<string[]>): string[];
  testACStringSequence(a: string[], b: InOutParam<string[]>): string[];
  testJsvalSequence(a: any[], b: InOutParam<any[]>): any[];
  testSequenceSequence(a: i16[][], b: InOutParam<i16[][]>): i16[][];
  testOptionalSequence(arr?: u8[]): u8[];
  testShortArray(aLength: u32, a: i16[], bLength: InOutParam<u32>, b: InOutParam<i16[]>, rvLength: OutParam<u32>): OutParam<i16[]>;
  testDoubleArray(aLength: u32, a: double[], bLength: InOutParam<u32>, b: InOutParam<double[]>, rvLength: OutParam<u32>): OutParam<double[]>;
  testStringArray(aLength: u32, a: string[], bLength: InOutParam<u32>, b: InOutParam<string[]>, rvLength: OutParam<u32>): OutParam<string[]>;
  testWstringArray(aLength: u32, a: string[], bLength: InOutParam<u32>, b: InOutParam<string[]>, rvLength: OutParam<u32>): OutParam<string[]>;
  testInterfaceArray(aLength: u32, a: nsIXPCTestInterfaceA[], bLength: InOutParam<u32>, b: InOutParam<nsIXPCTestInterfaceA[]>, rvLength: OutParam<u32>): OutParam<nsIXPCTestInterfaceA[]>;
  testByteArrayOptionalLength(a: u8[], aLength?: u32): u32;
  testSizedString(aLength: u32, a: string, bLength: InOutParam<u32>, b: InOutParam<string>, rvLength: OutParam<u32>): OutParam<string>;
  testSizedWstring(aLength: u32, a: string, bLength: InOutParam<u32>, b: InOutParam<string>, rvLength: OutParam<u32>): OutParam<string>;
  testJsvalArray(aLength: u32, a: any[], bLength: InOutParam<u32>, b: InOutParam<any[]>, rvLength: OutParam<u32>): OutParam<any[]>;
  testOutAString(o: OutParam<string>): void;
  testStringArrayOptionalSize(a: string[], aLength?: u32): string;
  testOmittedOptionalOut(aJSObj: nsIXPCTestParams, aOut?: OutParam<nsIURI>): void;
  readonly testNaN: double;
}

// https://searchfox.org/mozilla-central/source/js/xpconnect/tests/idl/xpctest_returncode.idl

interface nsIXPCTestReturnCodeParent extends nsISupports {
  callChild(childBehavior: i32): nsresult;
}

interface nsIXPCTestReturnCodeChild extends nsISupports {
  readonly CHILD_SHOULD_THROW?: 0;
  readonly CHILD_SHOULD_RETURN_SUCCESS?: 1;
  readonly CHILD_SHOULD_RETURN_RESULTCODE?: 2;
  readonly CHILD_SHOULD_NEST_RESULTCODES?: 3;

  doIt(behavior: i32): void;
}

// https://searchfox.org/mozilla-central/source/js/xpconnect/tests/idl/xpctest_utils.idl

type nsIXPCTestFunctionInterface = Callable<{
  echo(arg: string): string;
}>

interface nsIXPCTestUtils extends nsISupports {
  doubleWrapFunction(f: nsIXPCTestFunctionInterface): nsIXPCTestFunctionInterface;
}

interface nsIXPCTestTypeScript extends nsISupports {
  exposedProp: i32;
  exposedMethod(arg: i32): void;
}

interface nsIXPCComponents_Interfaces {
  nsIXPCTestObjectReadOnly: nsJSIID<nsIXPCTestObjectReadOnly>;
  nsIXPCTestObjectReadWrite: nsJSIID<nsIXPCTestObjectReadWrite>;
  nsIXPCTestBug809674: nsJSIID<nsIXPCTestBug809674>;
  nsIXPCTestCEnums: nsJSIID<nsIXPCTestCEnums, typeof nsIXPCTestCEnums_testFlagsExplicit & typeof nsIXPCTestCEnums_testFlagsImplicit>;
  nsIXPCTestInterfaceA: nsJSIID<nsIXPCTestInterfaceA>;
  nsIXPCTestInterfaceB: nsJSIID<nsIXPCTestInterfaceB>;
  nsIXPCTestInterfaceC: nsJSIID<nsIXPCTestInterfaceC>;
  nsIXPCTestParams: nsJSIID<nsIXPCTestParams>;
  nsIXPCTestReturnCodeParent: nsJSIID<nsIXPCTestReturnCodeParent>;
  nsIXPCTestReturnCodeChild: nsJSIID<nsIXPCTestReturnCodeChild>;
  nsIXPCTestFunctionInterface: nsJSIID<nsIXPCTestFunctionInterface>;
  nsIXPCTestUtils: nsJSIID<nsIXPCTestUtils>;
  nsIXPCTestTypeScript: nsJSIID<nsIXPCTestTypeScript>;
}

}  // global

// Typedefs from xpidl.
type PRTime = i64;

// XPCOM internal utility types.

/** XPCOM inout param is passed in as a js object with a value property. */
type InOutParam<T> = { value: T };

/** XPCOM out param is written to the passed in object's value property. */
type OutParam<T> = { value?: T };

/** Enable interfaces to inherit from enums: pick variants as optional. */
type Enums<enums> = Partial<Pick<enums, keyof enums>>;

/** Callable accepts either form of a [function] interface. */
type Callable<iface> = iface | Extract<iface[keyof iface], Function>

export {};
