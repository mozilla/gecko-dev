export const description = `
Validate that various GPU objects behave correctly in JavaScript.

Examples:
  - GPUTexture, GPUBuffer, GPUQuerySet, GPUDevice, GPUAdapter, GPUAdapter.limits, GPUDevice.limits
    - return nothing for Object.keys()
    - adds no properties for {...object}
    - iterate over keys for (key in object)
    - throws for (key of object)
    - return nothing from Object.getOwnPropertyDescriptors
 - GPUAdapter.limits, GPUDevice.limits
     - can not be passed to requestAdapter as requiredLimits
 - GPUAdapter.features, GPUDevice.features
    - do spread to array
    - can be copied to set
    - can be passed to requestAdapter as requiredFeatures
`;

import { makeTestGroup } from '../../common/framework/test_group.js';
import { keysOf } from '../../common/util/data_tables.js';
import { getGPU } from '../../common/util/navigator_gpu.js';
import {
  assert,
  objectEquals,
  raceWithRejectOnTimeout,
  unreachable,
} from '../../common/util/util.js';
import { getDefaultLimitsForDevice, kLimits } from '../capability_info.js';
import { AllFeaturesMaxLimitsGPUTest, GPUTest } from '../gpu_test.js';

// MAINTENANCE_TODO: Remove this filter when these limits are added to the spec.
const isUnspecifiedLimit = (limit: string) =>
  /maxStorage(Buffer|Texture)sIn(Vertex|Fragment)Stage/.test(limit);

const kSpecifiedLimits = kLimits.filter(s => !isUnspecifiedLimit(s));

type ResourceInfo = {
  create: (t: GPUTest) => Object;
  requiredKeys: readonly string[];
  getters: readonly string[];
  settable: readonly string[];
  sameObject: readonly string[];
  // Note: some of the tests need to be skipped in compatibility mode related to 'gpu'.
  // This is because getGPU() doesn't return the actual `navigator.gpu` in compatibility mode.
  // Instead it returns something that's wrapped to request compatibility mode.
  skipInCompatibility?: boolean;
};
const kResourceInfo: { [key: string]: ResourceInfo } = {
  gpu: {
    create(t: GPUTest) {
      return getGPU(null);
    },
    requiredKeys: ['wgslLanguageFeatures', 'requestAdapter'],
    getters: ['wgslLanguageFeatures'],
    settable: [],
    sameObject: ['wgslLanguageFeatures'],
    skipInCompatibility: true,
  },
  buffer: {
    create(t: GPUTest) {
      return t.createBufferTracked({ size: 16, usage: GPUBufferUsage.UNIFORM });
    },
    requiredKeys: [
      'destroy',
      'getMappedRange',
      'label',
      'mapAsync',
      'mapState',
      'size',
      'unmap',
      'usage',
    ],
    getters: ['label', 'mapState', 'size', 'usage'],
    settable: ['label'],
    sameObject: [],
  },
  texture: {
    create(t: GPUTest) {
      return t.createTextureTracked({
        size: [2, 3],
        format: 'r8unorm',
        usage: GPUTextureUsage.TEXTURE_BINDING,
      });
    },
    requiredKeys: [
      'createView',
      'depthOrArrayLayers',
      'destroy',
      'dimension',
      'format',
      'height',
      'label',
      'mipLevelCount',
      'sampleCount',
      'usage',
      'width',
    ],
    getters: [
      'depthOrArrayLayers',
      'dimension',
      'format',
      'height',
      'label',
      'mipLevelCount',
      'sampleCount',
      'usage',
      'width',
    ],
    settable: ['label'],
    sameObject: [],
  },
  querySet: {
    create(t: GPUTest) {
      return t.createQuerySetTracked({
        type: 'occlusion',
        count: 2,
      });
    },
    requiredKeys: ['count', 'destroy', 'label', 'type'],
    getters: ['count', 'label', 'type'],
    settable: ['label'],
    sameObject: [],
  },
  adapter: {
    create(t: GPUTest) {
      return t.adapter;
    },
    requiredKeys: ['features', 'info', 'limits', 'requestDevice'],
    getters: ['features', 'info', 'limits'],
    settable: [],
    sameObject: ['features', 'info', 'limits'],
  },
  device: {
    create(t: GPUTest) {
      return t.device;
    },
    requiredKeys: [
      'adapterInfo',
      'addEventListener',
      'createBindGroup',
      'createBindGroupLayout',
      'createBuffer',
      'createCommandEncoder',
      'createComputePipeline',
      'createComputePipelineAsync',
      'createPipelineLayout',
      'createQuerySet',
      'createRenderBundleEncoder',
      'createRenderPipeline',
      'createRenderPipelineAsync',
      'createSampler',
      'createShaderModule',
      'createTexture',
      'destroy',
      'dispatchEvent',
      'features',
      'importExternalTexture',
      'label',
      'limits',
      'lost',
      'onuncapturederror',
      'popErrorScope',
      'pushErrorScope',
      'queue',
      'removeEventListener',
    ],
    getters: ['adapterInfo', 'features', 'label', 'limits', 'lost', 'onuncapturederror', 'queue'],
    settable: ['label', 'onuncapturederror'],
    sameObject: ['adapterInfo', 'features', 'limits', 'queue'],
  },
  'adapter.limits': {
    create(t: GPUTest) {
      return t.adapter.limits;
    },
    requiredKeys: kSpecifiedLimits,
    getters: kSpecifiedLimits,
    settable: [],
    sameObject: [],
  },
  'device.limits': {
    create(t: GPUTest) {
      return t.device.limits;
    },
    requiredKeys: kSpecifiedLimits,
    getters: kSpecifiedLimits,
    settable: [],
    sameObject: [],
  },
} as const;
const kResources = keysOf(kResourceInfo);
type ResourceName = (typeof kResources)[number];

function createResource(t: GPUTest, type: ResourceName) {
  return kResourceInfo[type].create(t);
}

type Resource = ReturnType<typeof createResource>;

function forOfIterations(obj: Resource) {
  let count = 0;
  for (const _ of obj as unknown as []) {
    ++count;
  }
  return count;
}

function hasRequiredKeys(t: GPUTest, obj: Resource, requiredKeys: readonly string[]) {
  for (const requiredKey of requiredKeys) {
    t.expect(requiredKey in obj, `${requiredKey} in ${obj.constructor.name} exists`);
  }
}

function aHasBElements(
  t: GPUTest,
  a: GPUSupportedFeatures | string[],
  b: GPUSupportedFeatures | string[]
) {
  for (const elem of b) {
    if (Array.isArray(a)) {
      t.expect(a.includes(elem), `missing ${elem}`);
    } else if (a.has) {
      t.expect(a.has(elem), `missing ${elem}`);
    } else {
      unreachable();
    }
  }
}

export const g = makeTestGroup(AllFeaturesMaxLimitsGPUTest);
g.test('obj,Object_keys')
  .desc('tests returns nothing for Object.keys()')
  .params(u => u.combine('type', kResources))
  .fn(t => {
    const { type } = t.params;
    const { skipInCompatibility } = kResourceInfo[type];
    t.skipIf(t.isCompatibility && !!skipInCompatibility, 'skipped in compatibility mode');

    const obj = createResource(t, type);
    t.expect(objectEquals([...Object.keys(obj)], []), `[...Object.keys(${type})] === []`);
  });

g.test('obj,spread')
  .desc('does not spread')
  .params(u => u.combine('type', kResources))
  .fn(t => {
    const { type } = t.params;
    const { skipInCompatibility } = kResourceInfo[type];
    t.skipIf(t.isCompatibility && !!skipInCompatibility, 'skipped in compatibility mode');

    const obj = createResource(t, type);
    t.expect(objectEquals({ ...obj }, {}), `{ ...${type} ] === {}`);
  });

g.test('obj,for_in')
  .desc('iterates over keys - for (key in object)')
  .params(u => u.combine('type', kResources))
  .fn(t => {
    const { type } = t.params;
    const obj = createResource(t, type);
    hasRequiredKeys(t, obj, kResourceInfo[type].requiredKeys);
  });

g.test('obj,for_of')
  .desc('throws TypeError - for (key of object')
  .params(u => u.combine('type', kResources))
  .fn(t => {
    const { type } = t.params;
    const obj = createResource(t, type);
    t.shouldThrow('TypeError', () => forOfIterations(obj), {
      message: `for (const key of ${type} } throws TypeError`,
    });
  });

g.test('obj,getOwnPropertyDescriptors')
  .desc('Object.getOwnPropertyDescriptors returns {}')
  .params(u => u.combine('type', kResources))
  .fn(t => {
    const { type } = t.params;
    const { skipInCompatibility } = kResourceInfo[type];
    t.skipIf(t.isCompatibility && !!skipInCompatibility, 'skipped in compatibility mode');

    const obj = createResource(t, type);
    t.expect(
      objectEquals(Object.getOwnPropertyDescriptors(obj), {}),
      `Object.getOwnPropertyDescriptors(${type}} === {}`
    );
  });

const kSetLikeFeaturesInfo: { [key: string]: (t: GPUTest) => GPUSupportedFeatures } = {
  adapterFeatures(t: GPUTest) {
    return t.adapter.features;
  },
  deviceFeatures(t: GPUTest) {
    return t.device.features;
  },
};
const kSetLikeFeatures = keysOf(kSetLikeFeaturesInfo);

const kSetLikeInfo: { [key: string]: (t: GPUTest) => ReadonlySet<string> } = {
  ...kSetLikeFeaturesInfo,
  wgslLanguageFeatures() {
    return getGPU(null).wgslLanguageFeatures;
  },
};
const kSetLikes = keysOf(kSetLikeInfo);

g.test('setlike,spread')
  .desc('obj spreads')
  .params(u => u.combine('type', kSetLikes))
  .fn(t => {
    const { type } = t.params;
    const setLike = kSetLikeInfo[type](t);
    const copy = [...setLike];
    aHasBElements(t, copy, setLike);
    aHasBElements(t, setLike, copy);
  });

g.test('setlike,set')
  .desc('obj copies to set')
  .params(u => u.combine('type', kSetLikes))
  .fn(t => {
    const { type } = t.params;
    const setLike = kSetLikeInfo[type](t);
    const copy = new Set(setLike);
    aHasBElements(t, copy, setLike);
    aHasBElements(t, setLike, copy);
  });

g.test('setlike,requiredFeatures')
  .desc('can be passed as required features')
  .params(u => u.combine('type', kSetLikeFeatures))
  .fn(async t => {
    const { type } = t.params;
    const features = kSetLikeFeaturesInfo[type](t);

    const gpu = getGPU(null);
    const adapter = await gpu.requestAdapter();
    const device = await t.requestDeviceTracked(adapter!, {
      requiredFeatures: features as Iterable<GPUFeatureName>,
    });
    aHasBElements(t, device.features, features);
    aHasBElements(t, features, device.features);
  });

g.test('limits')
  .desc('adapter/device.limits can not be passed as requiredLimits')
  .params(u => u.combine('type', ['adapter', 'device'] as const))
  .fn(async t => {
    const { type } = t.params;
    const obj = type === 'adapter' ? t.adapter : t.device;

    const gpu = getGPU(null);
    const adapter = await gpu.requestAdapter();
    assert(!!adapter);
    const device = await t.requestDeviceTracked(adapter, {
      requiredLimits: obj.limits as unknown as Record<string, number>,
    });
    const defaultLimits = getDefaultLimitsForDevice(device);
    for (const [key, { default: defaultLimit }] of Object.entries(defaultLimits)) {
      if (isUnspecifiedLimit(key)) {
        continue;
      }
      const actual = (device.limits as unknown as Record<string, number>)[key];
      t.expect(
        actual === defaultLimit,
        `expected device.limits.${key}(${actual}) === ${defaultLimit}`
      );
    }
  });

g.test('readonly_properties')
  .desc(
    `
    Test that setting a property with no setter throws.
    `
  )
  .params(u => u.combine('type', kResources))
  .fn(t => {
    'use strict'; // This makes setting a readonly property produce a TypeError.
    const { type } = t.params;
    const { getters, settable } = kResourceInfo[type];
    const obj = createResource(t, type);
    for (const getter of getters) {
      const origValue = (obj as unknown as Record<string, unknown>)[getter];

      // try setting it.
      const isSettable = settable.includes(getter);
      t.shouldThrow(
        isSettable ? false : 'TypeError',
        () => {
          (obj as unknown as Record<string, string>)[getter] = 'test value';
          // If we were able to set it, restore it.
          (obj as unknown as Record<string, unknown>)[getter] = origValue;
        },
        {
          message: `instanceof ${type}.${getter} = value ${
            isSettable ? 'does not throw' : 'throws'
          }`,
        }
      );
    }
  });

g.test('getter_replacement')
  .desc(
    `
    Test that replacing getters on class prototypes works

    This is a common pattern for shims and debugging libraries so make sure this pattern works.
    `
  )
  .params(u => u.combine('type', kResources))
  .fn(t => {
    const { type } = t.params;
    const { getters } = kResourceInfo[type];

    const obj = createResource(t, type);
    for (const getter of getters) {
      // Check it's not 'ownProperty`
      const properties = Object.getOwnPropertyDescriptor(obj, getter);
      t.expect(
        properties === undefined,
        `Object.getOwnPropertyDescriptor(instance of ${type}, '${getter}') === undefined`
      );

      // Check it's actually a getter that returns a non-function value.
      const origValue = (obj as unknown as Record<string, () => unknown>)[getter];
      t.expect(typeof origValue !== 'function', `instance of ${type}.${getter} !== 'function'`);

      // check replacing the getter on constructor works.
      const ctorPrototype = obj.constructor.prototype;
      const origProperties = Object.getOwnPropertyDescriptor(ctorPrototype, getter);
      assert(
        !!origProperties,
        `Object.getOwnPropertyDescriptor(${type}, '${getter}') !== undefined`
      );
      try {
        Object.defineProperty(ctorPrototype, getter, {
          get() {
            return 'testGetterValue';
          },
        });
        const value = (obj as unknown as Record<string, string>)[getter];
        t.expect(
          value === 'testGetterValue',
          `replacing getter: '${getter}' on ${type} returns test value`
        );
      } finally {
        Object.defineProperty(ctorPrototype, getter, origProperties);
      }

      // Check it turns the same value after restoring as before restoring.
      const afterValue = (obj as unknown as Record<string, () => unknown>)[getter];
      assert(afterValue === origValue, `able to restore getter for instance of ${type}.${getter}`);

      // Check getOwnProperty also returns the value we got before.
      assert(
        Object.getOwnPropertyDescriptor(ctorPrototype, getter)!.get === origProperties.get,
        `getOwnPropertyDescriptor(${type}, '${getter}').get is original function`
      );
    }
  });

g.test('method_replacement')
  .desc(
    `
    Test that replacing methods on class prototypes works

    This is a common pattern for shims and debugging libraries so make sure this pattern works.
    `
  )
  .params(u => u.combine('type', kResources))
  .fn(t => {
    const { type } = t.params;
    const { requiredKeys, getters, skipInCompatibility } = kResourceInfo[type];
    t.skipIf(t.isCompatibility && !!skipInCompatibility, 'skipped in compatibility mode');

    const gettersSet = new Set<string>(getters);
    const methods = requiredKeys.filter(k => !gettersSet.has(k));

    const obj = createResource(t, type);
    for (const method of methods) {
      const ctorPrototype = obj.constructor.prototype;
      const origFunc = ctorPrototype[method];

      t.expect(typeof origFunc === 'function', `${type}.prototype.${method} is a function`);

      // Check the function the prototype and the one on the object are the same
      t.expect(
        (obj as unknown as Record<string, unknown>)[method] === origFunc,
        `instance of ${type}.${method} === ${type}.prototype.${method}`
      );

      // Check replacing the method on constructor works.
      try {
        (ctorPrototype as unknown as Record<string, unknown>)[method] = function () {
          return 'testMethodValue';
        };
        const value = (obj as unknown as Record<string, () => string>)[method]();
        t.expect(
          value === 'testMethodValue',
          `replacing method: '${method}' on ${type} returns test value`
        );
      } finally {
        (ctorPrototype as unknown as Record<string, unknown>)[method] = origFunc;
      }

      // Check the function the prototype and the one on the object are the same after restoring.
      assert(
        (obj as unknown as Record<string, unknown>)[method] === origFunc,
        `instance of ${type}.${method} === ${type}.prototype.${method}`
      );
    }
  });

g.test('sameObject')
  .desc(
    `
    Test that property that are supposed to return the sameObject do.
    `
  )
  .params(u => u.combine('type', kResources))
  .fn(t => {
    const { type } = t.params;
    const { sameObject } = kResourceInfo[type];
    const obj = createResource(t, type);
    for (const property of sameObject) {
      const origValue1 = (obj as unknown as Record<string, Record<string, string>>)[property];
      const origValue2 = (obj as unknown as Record<string, Record<string, string>>)[property];
      t.expect(typeof origValue1 === 'object');
      t.expect(typeof origValue2 === 'object');
      // Seems like this should be enough if they are objects.
      t.expect(origValue1 === origValue2);
      // add a property
      origValue1['foo'] = 'test';
      // see that it appears on the object.
      t.expect(origValue1.foo === 'test');
      t.expect(origValue2.foo === 'test');
      // Delete the property.
      delete origValue2.foo;
      // See it was removed.
      assert(origValue1.foo === undefined);
      assert(origValue2.foo === undefined);
    }
  });

const kClassInheritanceTests: { [key: string]: () => boolean } = {
  GPUDevice: () => GPUDevice.prototype instanceof EventTarget,
  GPUPipelineError: () => GPUPipelineError.prototype instanceof DOMException,
  GPUValidationError: () => GPUValidationError.prototype instanceof GPUError,
  GPUOutOfMemoryError: () => GPUOutOfMemoryError.prototype instanceof GPUError,
  GPUInternalError: () => GPUInternalError.prototype instanceof GPUError,
  GPUUncapturedErrorEvent: () => GPUUncapturedErrorEvent.prototype instanceof Event,
} as const;

g.test('inheritance')
  .desc(
    `
Test that objects inherit from the correct base class

This is important because apps might patch the base class
and expect instances of these classes to respond correctly.
`
  )
  .params(u => u.combine('type', keysOf(kClassInheritanceTests)))
  .fn(t => {
    const fn = kClassInheritanceTests[t.params.type];
    t.expect(fn(), fn.toString());
  });

const kDispatchTests = {
  async canPassEventThroughDevice(t: GPUTest) {
    const result = await raceWithRejectOnTimeout(
      new Promise(resolve => {
        t.device.addEventListener('foo', resolve, { once: true });
        t.device.dispatchEvent(new Event('foo'));
      }),
      500,
      'timeout'
    );
    const event = result as Event;
    t.expect(() => event instanceof Event, 'event');
    t.expect(() => event.type === 'foo');
  },
  async canPassCustomEventThroughDevice(t: GPUTest) {
    const result = await raceWithRejectOnTimeout(
      new Promise(resolve => {
        t.device.addEventListener('bar', resolve, { once: true });
        t.device.dispatchEvent(new CustomEvent('bar'));
      }),
      500,
      'timeout'
    );
    const event = result as CustomEvent;
    t.expect(() => event instanceof CustomEvent);
    t.expect(() => event instanceof Event);
    t.expect(() => event.type === 'bar');
  },
  async patchingEventTargetAffectsDevice(t: GPUTest) {
    let addEventListenerWasCalled = false;
    let dispatchEventWasCalled = false;
    let removeEventListenerWasCalled = false;

    // eslint-disable-next-line @typescript-eslint/unbound-method
    const origFnAddEventListener = EventTarget.prototype.addEventListener;
    EventTarget.prototype.addEventListener = function (
      ...args: Parameters<typeof EventTarget.prototype.addEventListener>
    ) {
      addEventListenerWasCalled = true;
      return origFnAddEventListener.call(this, ...args);
    };

    // eslint-disable-next-line @typescript-eslint/unbound-method
    const origFnDispatchEvent = EventTarget.prototype.dispatchEvent;
    EventTarget.prototype.dispatchEvent = function (event: Event) {
      dispatchEventWasCalled = true;
      return origFnDispatchEvent.call(this, event);
    };

    // eslint-disable-next-line @typescript-eslint/unbound-method
    const origFnRemoveEventListener = EventTarget.prototype.removeEventListener;
    EventTarget.prototype.removeEventListener = function (
      ...args: Parameters<typeof EventTarget.prototype.removeEventListener>
    ) {
      removeEventListenerWasCalled = true;
      return origFnRemoveEventListener.call(this, ...args);
    };

    try {
      await raceWithRejectOnTimeout(
        new Promise(resolve => {
          t.device.addEventListener('foo', resolve);
          t.device.dispatchEvent(new Event('foo'));
          t.device.removeEventListener('foo', resolve);
        }),
        500,
        'timeout'
      );
    } finally {
      EventTarget.prototype.addEventListener = origFnAddEventListener;
      EventTarget.prototype.dispatchEvent = origFnDispatchEvent;
      EventTarget.prototype.removeEventListener = origFnRemoveEventListener;
    }
    t.expect(addEventListenerWasCalled, 'overriding EventTarget addEventListener worked');
    t.expect(dispatchEventWasCalled, 'overriding EventTarget dispatchEvent worked');
    t.expect(removeEventListenerWasCalled, 'overriding EventTarget removeEventListener worked');
  },
  async passingGPUUncapturedErrorEventWorksThoughEventTarget(t: GPUTest) {
    const target = new EventTarget();
    const result = await raceWithRejectOnTimeout(
      new Promise(resolve => {
        target.addEventListener('uncapturederror', resolve, { once: true });
        target.dispatchEvent(
          new GPUUncapturedErrorEvent('uncapturederror', {
            error: new GPUValidationError('test error'),
          })
        );
      }),
      500,
      'timeout'
    );
    const event = result as GPUUncapturedErrorEvent;
    t.expect(() => event instanceof GPUUncapturedErrorEvent);
    t.expect(() => event.error instanceof GPUValidationError);
    t.expect(() => event.error.message === 'test error');
  },
} as const;

g.test('device,EventTarget')
  .desc(
    `
Test some repercussions of the fact that GPUDevice extends EventTarget
`
  )
  .params(u => u.combine('test', keysOf(kDispatchTests)))
  .fn(async t => {
    await kDispatchTests[t.params.test](t);
  });

const kAddEventListenerTests = {
  EventHandler: async (t: GPUTest) => {
    const result = await raceWithRejectOnTimeout(
      new Promise(resolve => {
        t.device.addEventListener('foo', resolve, { once: true });
        t.device.dispatchEvent(new Event('foo'));
      }),
      500,
      'timeout'
    );
    const event = result as Event;
    t.expect(() => event instanceof Event, 'event');
    t.expect(() => event.type === 'foo');
  },
  EventListener: async (t: GPUTest) => {
    const result = await raceWithRejectOnTimeout(
      new Promise(resolve => {
        t.device.addEventListener(
          'foo',
          {
            handleEvent: resolve,
          },
          { once: true }
        );
        t.device.dispatchEvent(new Event('foo'));
      }),
      500,
      'timeout'
    );
    const event = result as Event;
    t.expect(() => event instanceof Event, 'event');
    t.expect(() => event.type === 'foo');
  },
} as const;

g.test('device,addEventListener')
  .desc(
    `
Test that addEventListener works with both an EventListener and an EventHandler

* https://dom.spec.whatwg.org/#interface-eventtarget
* https://html.spec.whatwg.org/multipage/webappapis.html#eventhandler

`
  )
  .params(u => u.combine('test', keysOf(kAddEventListenerTests)))
  .fn(async t => {
    await kAddEventListenerTests[t.params.test](t);
  });
