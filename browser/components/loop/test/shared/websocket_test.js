/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

describe("loop.CallConnectionWebSocket", function() {
  "use strict";

  var expect = chai.expect;
  var WEBSOCKET_REASONS = loop.shared.utils.WEBSOCKET_REASONS;

  var sandbox,
      dummySocket;

  beforeEach(function() {
    sandbox = sinon.sandbox.create();
    sandbox.useFakeTimers();

    dummySocket = {
      close: sinon.spy(),
      send: sinon.spy()
    };
    sandbox.stub(window, "WebSocket").returns(dummySocket);
  });

  afterEach(function() {
    sandbox.restore();
  });

  describe("#constructor", function() {
    it("should require a url option", function() {
      expect(function() {
        return new loop.CallConnectionWebSocket();
      }).to.Throw(/No url/);
    });

    it("should require a callId setting", function() {
      expect(function() {
        return new loop.CallConnectionWebSocket({url: "wss://fake/"});
      }).to.Throw(/No callId/);
    });

    it("should require a websocketToken setting", function() {
      expect(function() {
        return new loop.CallConnectionWebSocket({
          url: "http://fake/",
          callId: "hello"
        });
      }).to.Throw(/No websocketToken/);
    });
  });

  describe("constructed", function() {
    var callWebSocket, fakeUrl, fakeCallId, fakeWebSocketToken;

    beforeEach(function() {
      fakeUrl = "wss://fake/";
      fakeCallId = "callId";
      fakeWebSocketToken = "7b";

      callWebSocket = new loop.CallConnectionWebSocket({
        url: fakeUrl,
        callId: fakeCallId,
        websocketToken: fakeWebSocketToken
      });
    });

    describe("#promiseConnect", function() {
      it("should create a new websocket connection", function() {
        callWebSocket.promiseConnect();

        sinon.assert.calledOnce(window.WebSocket);
        sinon.assert.calledWithExactly(window.WebSocket, fakeUrl);
      });

      it("should reject the promise if connection is not completed in " +
         "5 seconds", function(done) {
        var promise = callWebSocket.promiseConnect();

        sandbox.clock.tick(5101);

        promise.then(function() {}, function(error) {
          expect(error).to.be.equal(WEBSOCKET_REASONS.TIMEOUT);
          done();
        });
      });

      it("should reject the promise if the connection errors", function(done) {
        var promise = callWebSocket.promiseConnect();

        dummySocket.onerror("error");

        promise.then(function() {}, function(error) {
          expect(error).to.be.equal("error");
          done();
        });
      });

      it("should reject the promise if the connection closes", function(done) {
        var promise = callWebSocket.promiseConnect();

        dummySocket.onclose("close");

        promise.then(function() {}, function(error) {
          expect(error).to.be.equal("close");
          done();
        });
      });

      it("should send hello when the socket is opened", function() {
        callWebSocket.promiseConnect();

        dummySocket.onopen();

        sinon.assert.calledOnce(dummySocket.send);
        sinon.assert.calledWithExactly(dummySocket.send, JSON.stringify({
          messageType: "hello",
          callId: fakeCallId,
          auth: fakeWebSocketToken
        }));
      });

      it("should resolve the promise when the 'hello' is received",
        function(done) {
          var promise = callWebSocket.promiseConnect();

          dummySocket.onmessage({
            data: '{"messageType":"hello", "state":"init"}'
          });

          promise.then(function(state) {
            expect(state).eql("init");
            done();
          }, function() {
            done(new Error("shouldn't have rejected the promise"));
          });
        });
    });

    describe("#close", function() {
      it("should close the socket", function() {
        callWebSocket.promiseConnect();

        callWebSocket.close();

        sinon.assert.calledOnce(dummySocket.close);
      });
    });

    describe("#decline", function() {
      it("should send a terminate message to the server", function() {
        callWebSocket.promiseConnect();

        callWebSocket.decline();

        sinon.assert.calledOnce(dummySocket.send);
        sinon.assert.calledWithExactly(dummySocket.send, JSON.stringify({
          messageType: "action",
          event: "terminate",
          reason: WEBSOCKET_REASONS.REJECT
        }));
      });
    });

    describe("#accept", function() {
      it("should send an accept message to the server", function() {
        callWebSocket.promiseConnect();

        callWebSocket.accept();

        sinon.assert.calledOnce(dummySocket.send);
        sinon.assert.calledWithExactly(dummySocket.send, JSON.stringify({
          messageType: "action",
          event: "accept"
        }));
      });
    });

    describe("#mediaUp", function() {
      it("should send a media-up message to the server", function() {
        callWebSocket.promiseConnect();

        callWebSocket.mediaUp();

        sinon.assert.calledOnce(dummySocket.send);
        sinon.assert.calledWithExactly(dummySocket.send, JSON.stringify({
          messageType: "action",
          event: "media-up"
        }));
      });
    });

    describe("#cancel", function() {
      it("should send a terminate message to the server with a reason of WEBSOCKET_REASONS.CANCEL",
        function() {
          callWebSocket.promiseConnect();

          callWebSocket.cancel();

          sinon.assert.calledOnce(dummySocket.send);
          sinon.assert.calledWithExactly(dummySocket.send, JSON.stringify({
            messageType: "action",
            event: "terminate",
            reason: WEBSOCKET_REASONS.CANCEL
          }));
        });
    });

    describe("#mediaFail", function() {
      it("should send a terminate message to the server with a reason of WEBSOCKET_REASONS.MEDIA_FAIL",
        function() {
          callWebSocket.promiseConnect();

          callWebSocket.mediaFail();

          sinon.assert.calledOnce(dummySocket.send);
          sinon.assert.calledWithExactly(dummySocket.send, JSON.stringify({
            messageType: "action",
            event: "terminate",
            reason: WEBSOCKET_REASONS.MEDIA_FAIL
          }));
        });
    });

    describe("Events", function() {
      beforeEach(function() {
        sandbox.stub(callWebSocket, "trigger");

        callWebSocket.promiseConnect();
      });

      describe("Progress", function() {
        it("should trigger a progress event on the callWebSocket", function() {
          var eventData = {
            messageType: "progress",
            state: "terminate",
            reason: WEBSOCKET_REASONS.REJECT
          };

          dummySocket.onmessage({
            data: JSON.stringify(eventData)
          });

          sinon.assert.called(callWebSocket.trigger);
          sinon.assert.calledWithExactly(callWebSocket.trigger, "progress",
                                         eventData, "init");
        });

        it("should trigger a progress event with the previous state", function() {
          var previousEventData = {
            messageType: "progress",
            state: "alerting"
          };

          // This first call is to set the previous state of the object
          // ready for the main test below.
          dummySocket.onmessage({
            data: JSON.stringify(previousEventData)
          });

          var currentEventData = {
            messageType: "progress",
            state: "terminate",
            reason: WEBSOCKET_REASONS.REJECT
          };

          dummySocket.onmessage({
            data: JSON.stringify(currentEventData)
          });

          sinon.assert.called(callWebSocket.trigger);
          sinon.assert.calledWithExactly(callWebSocket.trigger, "progress",
                                         currentEventData, "alerting");
        });

        it("should trigger a progress:<state> event on the callWebSocket", function() {
          var eventData = {
            messageType: "progress",
            state: "terminate",
            reason: WEBSOCKET_REASONS.REJECT
          };

          dummySocket.onmessage({
            data: JSON.stringify(eventData)
          });

          sinon.assert.called(callWebSocket.trigger);
          sinon.assert.calledWithExactly(callWebSocket.trigger, "progress:terminate");
        });
      });

      describe("Error", function() {
        // Handled in constructed -> #promiseConnect:
        //   should reject the promise if the connection errors

        it("should trigger an error if state is not completed", function() {
          callWebSocket._clearConnectionFlags();

          dummySocket.onerror("Error");

          sinon.assert.calledOnce(callWebSocket.trigger);
          sinon.assert.calledWithExactly(callWebSocket.trigger,
                                         "error", "Error");
        });

        it("should not trigger an error if state is completed", function() {
          callWebSocket._clearConnectionFlags();
          callWebSocket._lastServerState = "connected";

          dummySocket.onerror("Error");

          sinon.assert.notCalled(callWebSocket.trigger);
        });
      });

      describe("Close", function() {
        // Handled in constructed -> #promiseConnect:
        //   should reject the promise if the connection closes

        it("should trigger a close event if state is not completed", function() {
          callWebSocket._clearConnectionFlags();

          dummySocket.onclose("Error");

          sinon.assert.calledOnce(callWebSocket.trigger);
          sinon.assert.calledWithExactly(callWebSocket.trigger,
                                         "closed", "Error");
        });

        it("should not trigger an error if state is completed", function() {
          callWebSocket._clearConnectionFlags();
          callWebSocket._lastServerState = "terminated";

          dummySocket.onclose("Error");

          sinon.assert.notCalled(callWebSocket.trigger);
        });
      });
    });
  });
});
