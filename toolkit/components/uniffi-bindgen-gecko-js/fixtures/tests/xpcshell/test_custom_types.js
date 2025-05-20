/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const CustomTypes = ChromeUtils.importESModule(
  "resource://gre/modules/RustCustomTypes.sys.mjs"
);

add_task(async function testCustomTypes() {
  // JS right now doesn't treat custom types as anything but it's native counterparts
  let demo = await CustomTypes.getCustomTypesDemo();
  Assert.equal(demo.url, "http://example.com/");
  Assert.equal(demo.handle, 123);
});

add_task(async function testExplicitEnumValues() {
  // Test that the enum values are preserved correctly, including gaps
  Assert.equal(CustomTypes.ExplicitValuedEnum.FIRST, 1);
  Assert.equal(CustomTypes.ExplicitValuedEnum.SECOND, 2);
  Assert.equal(CustomTypes.ExplicitValuedEnum.FOURTH, 4);
  Assert.equal(CustomTypes.ExplicitValuedEnum.TENTH, 10);
  Assert.equal(CustomTypes.ExplicitValuedEnum.ELEVENTH, 11);
  Assert.equal(CustomTypes.ExplicitValuedEnum.THIRTEENTH, 13);

  // Test that the discriminant function returns the expected values
  Assert.equal(
    await CustomTypes.getExplicitDiscriminant(
      CustomTypes.ExplicitValuedEnum.FIRST
    ),
    1
  );
  Assert.equal(
    await CustomTypes.getExplicitDiscriminant(
      CustomTypes.ExplicitValuedEnum.SECOND
    ),
    2
  );
  Assert.equal(
    await CustomTypes.getExplicitDiscriminant(
      CustomTypes.ExplicitValuedEnum.FOURTH
    ),
    4
  );
  Assert.equal(
    await CustomTypes.getExplicitDiscriminant(
      CustomTypes.ExplicitValuedEnum.TENTH
    ),
    10
  );
  Assert.equal(
    await CustomTypes.getExplicitDiscriminant(
      CustomTypes.ExplicitValuedEnum.ELEVENTH
    ),
    11
  );
  Assert.equal(
    await CustomTypes.getExplicitDiscriminant(
      CustomTypes.ExplicitValuedEnum.THIRTEENTH
    ),
    13
  );

  // Test that the enum values work correctly when passed back to Rust
  Assert.equal(
    await CustomTypes.echoExplicitValue(CustomTypes.ExplicitValuedEnum.FIRST),
    CustomTypes.ExplicitValuedEnum.FIRST
  );
  Assert.equal(
    await CustomTypes.echoExplicitValue(CustomTypes.ExplicitValuedEnum.FOURTH),
    CustomTypes.ExplicitValuedEnum.FOURTH
  );
  Assert.equal(
    await CustomTypes.echoExplicitValue(
      CustomTypes.ExplicitValuedEnum.THIRTEENTH
    ),
    CustomTypes.ExplicitValuedEnum.THIRTEENTH
  );
});

add_task(async function testGappedEnumValues() {
  // Import UniFFITypeError to check the error type
  const { UniFFITypeError } = ChromeUtils.importESModule(
    "resource://gre/modules/UniFFI.sys.mjs"
  );

  // Test that the enum values are preserved correctly for mixed sequential/explicit values
  Assert.equal(CustomTypes.GappedEnum.ONE, 10);
  Assert.equal(CustomTypes.GappedEnum.TWO, 11); // Sequential value after ONE (10+1)
  Assert.equal(CustomTypes.GappedEnum.THREE, 14); // Explicit value again

  // Verify with values from Rust
  const valueRecord = await CustomTypes.getGappedEnumValues();
  Assert.equal(valueRecord.One, CustomTypes.GappedEnum.ONE);
  Assert.equal(valueRecord.Two, CustomTypes.GappedEnum.TWO);
  Assert.equal(valueRecord.Three, CustomTypes.GappedEnum.THREE);

  // Test discriminant function
  Assert.equal(
    await CustomTypes.getGappedDiscriminant(CustomTypes.GappedEnum.ONE),
    10
  );
  Assert.equal(
    await CustomTypes.getGappedDiscriminant(CustomTypes.GappedEnum.TWO),
    11
  );
  Assert.equal(
    await CustomTypes.getGappedDiscriminant(CustomTypes.GappedEnum.THREE),
    14
  );

  // Test echo function
  Assert.equal(
    await CustomTypes.echoGappedValue(CustomTypes.GappedEnum.ONE),
    CustomTypes.GappedEnum.ONE
  );
  Assert.equal(
    await CustomTypes.echoGappedValue(CustomTypes.GappedEnum.TWO),
    CustomTypes.GappedEnum.TWO
  );
  Assert.equal(
    await CustomTypes.echoGappedValue(CustomTypes.GappedEnum.THREE),
    CustomTypes.GappedEnum.THREE
  );

  // Test validation for non-existent values
  // Values in gaps should be rejected
  await Assert.rejects(
    CustomTypes.getGappedDiscriminant(0),
    UniFFITypeError,
    "Should reject value 0 which is below the lowest enum variant"
  );

  await Assert.rejects(
    CustomTypes.getGappedDiscriminant(9),
    UniFFITypeError,
    "Should reject value 9 which is just below a valid enum variant"
  );

  await Assert.rejects(
    CustomTypes.getGappedDiscriminant(12),
    UniFFITypeError,
    "Should reject value 12 which is in a gap between valid enum variants"
  );

  await Assert.rejects(
    CustomTypes.getGappedDiscriminant(13),
    UniFFITypeError,
    "Should reject value 13 which is in a gap between valid enum variants"
  );

  await Assert.rejects(
    CustomTypes.getGappedDiscriminant(15),
    UniFFITypeError,
    "Should reject value 15 which is above the highest enum variant"
  );

  // Test non-integer values
  await Assert.rejects(
    CustomTypes.getGappedDiscriminant("ONE"),
    UniFFITypeError,
    "Should reject string value instead of enum"
  );

  await Assert.rejects(
    CustomTypes.getGappedDiscriminant(null),
    UniFFITypeError,
    "Should reject null value instead of enum"
  );

  await Assert.rejects(
    CustomTypes.getGappedDiscriminant(undefined),
    UniFFITypeError,
    "Should reject undefined value instead of enum"
  );
});
