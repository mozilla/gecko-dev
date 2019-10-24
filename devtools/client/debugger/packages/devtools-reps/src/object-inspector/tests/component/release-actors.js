/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/* global jest */
const { mountObjectInspector } = require("../test-utils");

const repsPath = "../../../reps";
const gripRepStubs = require(`${repsPath}/stubs/grip`);
const ObjectFront = require("../__mocks__/object-front");
const stub = gripRepStubs.get("testMoreThanMaxProps");
const { waitForDispatch } = require("../test-utils");

function getEnumPropertiesMock() {
  return jest.fn(() => ({
    iterator: {
      slice: () => ({}),
    },
  }));
}

function generateDefaults(overrides) {
  return {
    autoExpandDepth: 0,
    roots: [
      {
        path: "root",
        contents: {
          value: stub,
        },
      },
    ],
    ...overrides,
  };
}

function mount(props, { initialState } = {}) {
  const enumProperties = getEnumPropertiesMock();

  const client = {
    createObjectFront: grip => ObjectFront(grip, { enumProperties }),
    releaseActor: jest.fn(),
  };

  return mountObjectInspector({
    client,
    props: generateDefaults(props),
    initialState: {
      objectInspector: {
        ...initialState,
        evaluations: new Map(),
      },
    },
  });
}

describe("release actors", () => {
  it("calls release actors when unmount", async () => {
    const { wrapper, client, store } = mount(
      {
        injectWaitService: true,
      },
      {
        initialState: {
          actors: new Set(["actor 1", "actor 2"]),
          watchpoints: new Map(),
        },
      }
    );

    const onActorReleased = waitForDispatch(store, "RELEASED_ACTORS");
    wrapper.unmount();
    await onActorReleased;

    expect(client.releaseActor.mock.calls).toHaveLength(2);
    expect(client.releaseActor.mock.calls[0][0]).toBe("actor 1");
    expect(client.releaseActor.mock.calls[1][0]).toBe("actor 2");

    const state = store.getState().objectInspector;
    expect(state.actors.size).toBe(0);
  });

  it.skip("calls release actors when the roots prop changed", async () => {
    const { wrapper, store, client } = mount(
      {
        injectWaitService: true,
      },
      {
        initialState: {
          actors: new Set(["actor 3", "actor 4"]),
        },
      }
    );

    const onRootsChanged = waitForDispatch(store, "ROOTS_CHANGED");

    wrapper.setProps({
      roots: [
        {
          path: "root-2",
          contents: {
            value: gripRepStubs.get("testMaxProps"),
          },
        },
      ],
    });
    wrapper.update();
    //
    await onRootsChanged;
    //
    expect(client.releaseActor.mock.calls).toHaveLength(2);
    expect(client.releaseActor.mock.calls[0][0]).toBe("actor 3");
    expect(client.releaseActor.mock.calls[1][0]).toBe("actor 4");
  });
});
