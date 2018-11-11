/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const {
  clonePacket,
  getFirstMessage,
  setupActions,
  setupStore,
} = require("devtools/client/webconsole/test/helpers");

const { stubPackets } = require("devtools/client/webconsole/test/fixtures/stubs/index");
const expect = require("expect");

describe("Release actor enhancer:", () => {
  let actions;

  before(() => {
    actions = setupActions();
  });

  describe("Client proxy", () => {
    it("releases backend actors when limit reached adding a single message", () => {
      const logLimit = 100;
      const releasedActors = [];
      const { dispatch, getState } = setupStore([], {
        storeOptions: {logLimit},
        hud: {
          proxy: {
            releaseActor: (actor) => {
              releasedActors.push(actor);
            },
          },
        },
      });

      // Add a log message.
      dispatch(actions.messagesAdd([
        stubPackets.get("console.log('myarray', ['red', 'green', 'blue'])")]));

      const firstMessage = getFirstMessage(getState());
      const firstMessageActor = firstMessage.parameters[1].actor;

      // Add an evaluation result message (see Bug 1408321).
      const evaluationResultPacket = stubPackets.get("new Date(0)");
      dispatch(actions.messagesAdd([evaluationResultPacket]));
      const secondMessageActor = evaluationResultPacket.result.actor;

      const logCount = logLimit + 1;
      const packet = clonePacket(stubPackets.get(
        "console.assert(false, {message: 'foobar'})"));
      const thirdMessageActor = packet.message.arguments[0].actor;

      for (let i = 1; i <= logCount; i++) {
        packet.message.arguments.push(`message num ${i}`);
        dispatch(actions.messagesAdd([packet]));
      }

      expect(releasedActors.length).toBe(3);
      expect(releasedActors).toInclude(firstMessageActor);
      expect(releasedActors).toInclude(secondMessageActor);
      expect(releasedActors).toInclude(thirdMessageActor);
    });

    it("releases backend actors when limit reached adding multiple messages", () => {
      const logLimit = 100;
      const releasedActors = [];
      const { dispatch, getState } = setupStore([], {
        storeOptions: {logLimit},
        hud: {
          proxy: {
            releaseActor: (actor) => {
              releasedActors.push(actor);
            },
          },
        },
      });

      // Add a log message.
      dispatch(actions.messagesAdd([
        stubPackets.get("console.log('myarray', ['red', 'green', 'blue'])")]));

      const firstMessage = getFirstMessage(getState());
      const firstMessageActor = firstMessage.parameters[1].actor;

      // Add an evaluation result message (see Bug 1408321).
      const evaluationResultPacket = stubPackets.get("new Date(0)");
      dispatch(actions.messagesAdd([evaluationResultPacket]));
      const secondMessageActor = evaluationResultPacket.result.actor;

      // Add an assertion message.
      const assertPacket = stubPackets.get("console.assert(false, {message: 'foobar'})");
      dispatch(actions.messagesAdd([assertPacket]));
      const thirdMessageActor = assertPacket.message.arguments[0].actor;

      // Add ${logLimit} messages so we prune the ones we added before.
      const packets = [];
      // Alternate between 2 packets so we don't trigger the repeat message mechanism.
      const oddPacket = stubPackets.get("console.log(undefined)");
      const evenPacket = stubPackets.get("console.log('foobar', 'test')");
      for (let i = 0; i < logLimit; i++) {
        const packet = i % 2 === 0 ? evenPacket : oddPacket;
        packets.push(packet);
      }

      // Add all the packets at once. This will prune the first 3 messages.
      dispatch(actions.messagesAdd(packets));

      expect(releasedActors.length).toBe(3);
      expect(releasedActors).toInclude(firstMessageActor);
      expect(releasedActors).toInclude(secondMessageActor);
      expect(releasedActors).toInclude(thirdMessageActor);
    });

    it("properly releases backend actors after clear", () => {
      const releasedActors = [];
      const { dispatch, getState } = setupStore([], {
        hud: {
          proxy: {
            releaseActor: (actor) => {
              releasedActors.push(actor);
            },
          },
        },
      });

      // Add a log message.
      dispatch(actions.messagesAdd([
        stubPackets.get("console.log('myarray', ['red', 'green', 'blue'])")]));

      const firstMessage = getFirstMessage(getState());
      const firstMessageActor = firstMessage.parameters[1].actor;

      const packet = clonePacket(stubPackets.get(
        "console.assert(false, {message: 'foobar'})"));
      const secondMessageActor = packet.message.arguments[0].actor;
      dispatch(actions.messagesAdd([packet]));

      // Add an evaluation result message (see Bug 1408321).
      const evaluationResultPacket = stubPackets.get("new Date(0)");
      dispatch(actions.messagesAdd([evaluationResultPacket]));
      const thirdMessageActor = evaluationResultPacket.result.actor;

      // Add a message with a long string messageText property.
      const longStringPacket = stubPackets.get("TypeError longString message");
      dispatch(actions.messagesAdd([longStringPacket]));
      const fourthMessageActor = longStringPacket.pageError.errorMessage.actor;

      // Kick-off the actor release.
      dispatch(actions.messagesClear());

      expect(releasedActors.length).toBe(4);
      expect(releasedActors).toInclude(firstMessageActor);
      expect(releasedActors).toInclude(secondMessageActor);
      expect(releasedActors).toInclude(thirdMessageActor);
      expect(releasedActors).toInclude(fourthMessageActor);
    });
  });
});
