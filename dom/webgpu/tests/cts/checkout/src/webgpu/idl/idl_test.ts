import { Fixture } from '../../common/framework/fixture.js';
import { getGPU } from '../../common/util/navigator_gpu.js';
import { assert } from '../../common/util/util.js';

interface UnknownObject {
  [k: string]: unknown;
}

/**
 * Base fixture for testing the exposed interface is correct (without actually using WebGPU).
 */
export class IDLTest extends Fixture {
  override init(): Promise<void> {
    // Ensure the GPU provider is initialized
    getGPU(this.rec);
    return Promise.resolve();
  }

  /**
   * Asserts that a member of an IDL interface has the expected value.
   */
  assertMember(act: object, exp: object, key: string) {
    assert(key in act, () => `Expected key ${key} missing`);
    const actValue = (act as UnknownObject)[key];
    const expValue = (exp as UnknownObject)[key];
    assert(actValue === expValue, () => `Value of [${key}] was ${actValue}, expected ${expValue}`);
  }

  /**
   * Asserts that an IDL interface has the same number of keys as the
   *
   * MAINTENANCE_TODO: add a way to check for the types of keys with unknown values, like methods and attributes
   * MAINTENANCE_TODO: handle extensions
   */
  assertMemberCount(act: object, exp: object) {
    const expKeys = Object.keys(exp);
    const actKeys = Object.keys(act);
    assert(
      actKeys.length === expKeys.length,
      () => `Had ${actKeys.length} keys, expected ${expKeys.length}`
    );
  }
}
