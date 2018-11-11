/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

describe("loop.shared.utils", function() {
  "use strict";

  var expect = chai.expect;
  var mozL10n = navigator.mozL10n || document.mozL10n;
  var sandbox;
  var sharedUtils = loop.shared.utils;

  beforeEach(function() {
    sandbox = LoopMochaUtils.createSandbox();
  });

  afterEach(function() {
    sandbox.restore();
  });

  describe("#getUnsupportedPlatform", function() {
    it("should detect iOS", function() {
      expect(sharedUtils.getUnsupportedPlatform("iPad")).eql("ios");
      expect(sharedUtils.getUnsupportedPlatform("iPod")).eql("ios");
      expect(sharedUtils.getUnsupportedPlatform("iPhone")).eql("ios");
      expect(sharedUtils.getUnsupportedPlatform("iPhone Simulator")).eql("ios");
    });

    it("should detect Windows Phone", function() {
      expect(sharedUtils.getUnsupportedPlatform("Windows Phone"))
        .eql("windows_phone");
    });

    it("should detect BlackBerry", function() {
      expect(sharedUtils.getUnsupportedPlatform("BlackBerry"))
        .eql("blackberry");
    });

    it("shouldn't detect other platforms", function() {
      expect(sharedUtils.getUnsupportedPlatform("MacIntel")).eql(null);
    });
  });

  describe("#isFirefox", function() {
    it("should detect Firefox", function() {
      expect(sharedUtils.isFirefox("Firefox")).eql(true);
      expect(sharedUtils.isFirefox("Gecko/Firefox")).eql(true);
      expect(sharedUtils.isFirefox("Firefox/Gecko")).eql(true);
      expect(sharedUtils.isFirefox("Gecko/Firefox/Chuck Norris")).eql(true);
    });

    it("shouldn't detect Firefox with other platforms", function() {
      expect(sharedUtils.isFirefox("Opera")).eql(false);
    });
  });

  describe("#formatDate", function() {
    beforeEach(function() {
      sandbox.stub(Date.prototype, "toLocaleDateString").returns("fake result");
    });

    it("should call toLocaleDateString with arguments", function() {
      sharedUtils.formatDate(1000);

      sinon.assert.calledOnce(Date.prototype.toLocaleDateString);
      sinon.assert.calledWithExactly(Date.prototype.toLocaleDateString,
        navigator.language,
        { year: "numeric", month: "long", day: "numeric" }
      );
    });

    it("should return the formatted string", function() {
      expect(sharedUtils.formatDate(1000)).eql("fake result");
    });
  });

  describe("#getBoolPreference", function() {
    afterEach(function() {
      localStorage.removeItem("test.true");
    });

    describe("mozLoop set", function() {
      beforeEach(function() {
        LoopMochaUtils.stubLoopRequest({
          GetLoopPref: function(prefName) {
            return prefName === "test.true";
          }
        });
      });

      afterEach(function() {
        LoopMochaUtils.restore();
      });

      it("should return the mozLoop preference", function() {
        sharedUtils.getBoolPreference("test.true", function(result) {
          expect(result).eql(true);
        });
      });

      it("should not use the localStorage value", function() {
        localStorage.setItem("test.false", true);

        sharedUtils.getBoolPreference("test.false", function(result) {
          expect(result).eql(false);
        });
      });
    });

    describe("mozLoop not set", function() {
      beforeEach(function() {
        sandbox.stub(loop.shared.utils, "isDesktop").returns(false);
      });

      it("should return the localStorage value", function() {
        localStorage.setItem("test.true", true);

        sharedUtils.getBoolPreference("test.true", function(result) {
          expect(result).eql(true);
        });
      });
    });
  });

  describe("#hasAudioOrVideoDevices", function() {
    var fakeNavigatorObject, fakeWindowObject;

    beforeEach(function() {
      fakeNavigatorObject = {
        mediaDevices: {
          enumerateDevices: sinon.stub()
        }
      };

      fakeWindowObject = {
        MediaStreamTrack: {
          getSources: sinon.stub()
        }
      };

      sharedUtils.setRootObjects(fakeWindowObject, fakeNavigatorObject);
    });

    afterEach(function() {
      sharedUtils.setRootObjects();
    });

    it("should return true if no APIs to detect devices exist", function(done) {
      delete fakeNavigatorObject.mediaDevices;
      delete fakeWindowObject.MediaStreamTrack;

      sharedUtils.hasAudioOrVideoDevices(function(result) {
        expect(result).eql(true);
        done();
      });
    });

    it("should return true if enumerateDevices doesn't exist in navigator.mediaDevices", function(done) {
      sharedUtils.setRootObjects(fakeWindowObject, {
        mediaDevices: {}
      });
      delete fakeWindowObject.MediaStreamTrack;

      sharedUtils.hasAudioOrVideoDevices(function(result) {
        expect(result).eql(true);
        done();
      });
    });

    it("should return true if getSources doesn't exist in window.MediaStreamTrack", function(done) {
      sharedUtils.setRootObjects({
        MediaStreamTrack: {}
      }, fakeNavigatorObject);
      delete fakeNavigatorObject.mediaDevices;

      sharedUtils.hasAudioOrVideoDevices(function(result) {
        expect(result).eql(true);
        done();
      });
    });

    it("should return false if no audio nor video devices exist according to navigator.mediaDevices", function(done) {
      delete fakeWindowObject.MediaStreamTrack;

      fakeNavigatorObject.mediaDevices.enumerateDevices.returns(Promise.resolve([]));

      sharedUtils.hasAudioOrVideoDevices(function(result) {
        try {
          expect(result).eql(false);
          done();
        } catch (ex) {
          done(ex);
        }
      });
    });

    it("should return true if audio devices exist according to navigator.mediaDevices", function(done) {
      delete fakeWindowObject.MediaStreamTrack;

      fakeNavigatorObject.mediaDevices.enumerateDevices.returns(
        Promise.resolve([{
          deviceId: "54321",
          groupId: "",
          kind: "audioinput",
          label: ""
        }])
      );

      sharedUtils.hasAudioOrVideoDevices(function(result) {
        try {
          expect(result).eql(true);
          done();
        } catch (ex) {
          done(ex);
        }
      });
    });

    it("should return true if video devices exist according to navigator.mediaDevices", function(done) {
      delete fakeWindowObject.MediaStreamTrack;

      fakeNavigatorObject.mediaDevices.enumerateDevices.returns(
        Promise.resolve([{
          deviceId: "15234",
          groupId: "",
          kind: "videoinput",
          label: ""
        }])
      );

      sharedUtils.hasAudioOrVideoDevices(function(result) {
        try {
          expect(result).eql(true);
          done();
        } catch (ex) {
          done(ex);
        }
      });
    });

    it("should return false if no audio nor video devices exist according to window.MediaStreamTrack", function(done) {
      delete fakeNavigatorObject.mediaDevices;

      fakeWindowObject.MediaStreamTrack.getSources.callsArgWith(0, []);
      sharedUtils.hasAudioOrVideoDevices(function(result) {
        try {
          expect(result).eql(false);
          done();
        } catch (ex) {
          done(ex);
        }
      });
    });

    it("should return true if audio devices exist according to window.MediaStreamTrack", function(done) {
      delete fakeNavigatorObject.mediaDevices;

      fakeWindowObject.MediaStreamTrack.getSources.callsArgWith(0, [{
        facing: "",
        id: "54321",
        kind: "audio",
        label: ""
      }]);

      sharedUtils.hasAudioOrVideoDevices(function(result) {
        try {
          expect(result).eql(true);
          done();
        } catch (ex) {
          done(ex);
        }
      });
    });

    it("should return true if video devices exist according to window.MediaStreamTrack", function(done) {
      delete fakeNavigatorObject.mediaDevices;

      fakeWindowObject.MediaStreamTrack.getSources.callsArgWith(0, [{
        facing: "",
        id: "15234",
        kind: "video",
        label: ""
      }]);

      sharedUtils.hasAudioOrVideoDevices(function(result) {
        try {
          expect(result).eql(true);
          done();
        } catch (ex) {
          done(ex);
        }
      });
    });
  });

  describe("#formatURL", function() {
    beforeEach(function() {
      // Stub to prevent console messages.
      sandbox.stub(window.console, "log");
    });

    it("should decode encoded URIs", function() {
      expect(sharedUtils.formatURL("http://invalid.com/?a=Foo%20Bar"))
        .eql({
          location: "http://invalid.com/?a=Foo Bar",
          hostname: "invalid.com",
          protocol: "http:"
        });
    });

    it("should change some idn urls to ascii encoded", function() {
      // Note, this is based on the browser's list of what does/doesn't get
      // altered for punycode, so if the list changes this could change in the
      // future.
      expect(sharedUtils.formatURL("http://\u0261oogle.com/"))
        .eql({
          location: "http://xn--oogle-qmc.com/",
          hostname: "xn--oogle-qmc.com",
          protocol: "http:"
        });
    });

    it("should return null if suppressConsoleError is true and the url is not valid", function() {
      expect(sharedUtils.formatURL("hinvalid//url", true)).eql(null);
    });

    it("should return null if suppressConsoleError is true and is a malformed URI sequence", function() {
      expect(sharedUtils.formatURL("http://www.example.com/DNA/pizza/menu/lots-of-different-kinds-of-pizza/%8D%E0%B8%88%E0%B8%A1%E0%B8%A3%E0%8D%E0%B8%88%E0%B8%A1%E0%B8%A3%E0%", true)).eql(null);
    });

    it("should log a malformed URI sequence error to the console", function() {
      sharedUtils.formatURL("http://www.example.com/DNA/pizza/menu/lots-of-different-kinds-of-pizza/%8D%E0%B8%88%E0%B8%A1%E0%B8%A3%E0%8D%E0%B8%88%E0%B8%A1%E0%B8%A3%E0%");
      sinon.assert.calledOnce(console.log);
    });
  });

  describe("#composeCallUrlEmail", function() {
    var requestStubs;

    beforeEach(function() {
      // fake mozL10n
      sandbox.stub(mozL10n, "get", function(id) {
        switch (id) {
          case "share_email_subject7":
            return "subject";
          case "share_email_body7":
            return "body";
          case "share_email_body_context3":
            return "body_context";
          case "share_email_footer2":
            return "footer";
        }
      });
      LoopMochaUtils.stubLoopRequest(requestStubs = {
        GetAllConstants: function() {
          return {
            SHARING_ROOM_URL: {
              EMAIL_FROM_CALLFAILED: 2,
              EMAIL_FROM_CONVERSATION: 3
            }
          };
        },
        GetLoopPref: sinon.stub(),
        ComposeEmail: sinon.stub()
      });
    });

    afterEach(function() {
      LoopMochaUtils.restore();
    });

    it("should compose a call url email", function() {
      sharedUtils.composeCallUrlEmail("http://invalid", "fake@invalid.tld");

      sinon.assert.calledOnce(requestStubs.ComposeEmail);
      sinon.assert.calledWith(requestStubs.ComposeEmail,
                              "subject", "body" + "footer", "fake@invalid.tld");
    });

    it("should compose a different email when context info is provided", function() {
      sharedUtils.composeCallUrlEmail("http://invalid", null, "Hello, is me you're looking for?");

      sinon.assert.calledOnce(requestStubs.ComposeEmail);
      sinon.assert.calledWith(requestStubs.ComposeEmail, "subject", "body_context" + "footer");
    });
  });

  describe("#btoa", function() {
    it("should encode a basic base64 string", function() {
      var result = sharedUtils.btoa(sharedUtils.strToUint8Array("crypto is great"));

      expect(result).eql("Y3J5cHRvIGlzIGdyZWF0");
    });

    it("should pad encoded base64 strings", function() {
      var result = sharedUtils.btoa(sharedUtils.strToUint8Array("crypto is grea"));

      expect(result).eql("Y3J5cHRvIGlzIGdyZWE=");

      result = sharedUtils.btoa(sharedUtils.strToUint8Array("crypto is gre"));

      expect(result).eql("Y3J5cHRvIGlzIGdyZQ==");
    });

    it("should encode a non-unicode base64 string", function() {
      var result = sharedUtils.btoa(sharedUtils.strToUint8Array("\uFDFD"));
      expect(result).eql("77e9");
    });
  });

  describe("#atob", function() {
    it("should decode a basic base64 string", function() {
      var result = sharedUtils.Uint8ArrayToStr(sharedUtils.atob("Y3J5cHRvIGlzIGdyZWF0"));

      expect(result).eql("crypto is great");
    });

    it("should decode a padded base64 string", function() {
      var result = sharedUtils.Uint8ArrayToStr(sharedUtils.atob("Y3J5cHRvIGlzIGdyZWE="));

      expect(result).eql("crypto is grea");

      result = sharedUtils.Uint8ArrayToStr(sharedUtils.atob("Y3J5cHRvIGlzIGdyZQ=="));

      expect(result).eql("crypto is gre");
    });

    it("should decode a base64 string that has unicode characters", function() {
      var result = sharedUtils.Uint8ArrayToStr(sharedUtils.atob("77e9"));

      expect(result).eql("\uFDFD");
    });
  });

  describe("#getOS", function() {
    it("should recognize the OSX userAgent string", function() {
      var UA = "Mozilla/5.0 (Macintosh; Intel Mac OS X 10.10; rv:37.0) Gecko/20100101 Firefox/37.0";
      var result = sharedUtils.getOS(UA);

      expect(result).eql("intel mac os x");
    });

    it("should recognize the OSX userAgent string with version", function() {
      var UA = "Mozilla/5.0 (Macintosh; Intel Mac OS X 10.10; rv:37.0) Gecko/20100101 Firefox/37.0";
      var result = sharedUtils.getOS(UA, true);

      expect(result).eql("intel mac os x 10.10");
    });

    it("should recognize the Windows userAgent string with version", function() {
      var UA = "Mozilla/5.0 (Windows NT 6.1; Win64; x64; rv:10.0) Gecko/20100101 Firefox/10.0";
      var result = sharedUtils.getOS(UA, true);

      expect(result).eql("windows nt 6.1");
    });

    it("should recognize the Linux userAgent string", function() {
      var UA = "Mozilla/5.0 (X11; Linux i686 on x86_64; rv:10.0) Gecko/20100101 Firefox/10.0";
      var result = sharedUtils.getOS(UA);

      expect(result).eql("linux i686 on x86_64");
    });

    it("should recognize the OSX oscpu string", function() {
      var oscpu = "Intel Mac OS X 10.10";
      var result = sharedUtils.getOS(oscpu, true);

      expect(result).eql("intel mac os x 10.10");
    });

    it("should recognize the Windows oscpu string", function() {
      var oscpu = "Windows NT 5.3; Win64; x64";
      var result = sharedUtils.getOS(oscpu, true);

      expect(result).eql("windows nt 5.3");
    });
  });

  describe("#getOSVersion", function() {
    it("should fetch the correct version info for OSX", function() {
      var UA = "Mozilla/5.0 (Macintosh; Intel Mac OS X 10.10; rv:37.0) Gecko/20100101 Firefox/37.0";
      var result = sharedUtils.getOSVersion(UA);

      expect(result).eql({ major: 10, minor: 10 });
    });

    it("should fetch the correct version info for Windows", function() {
      var UA = "Mozilla/5.0 (Windows NT 6.1; Win64; x64; rv:10.0) Gecko/20100101 Firefox/10.0";
      var result = sharedUtils.getOSVersion(UA);

      expect(result).eql({ major: 6, minor: 1 });
    });

    it("should fetch the correct version info for Windows XP", function() {
      var oscpu = "Windows XP";
      var result = sharedUtils.getOSVersion(oscpu);

      expect(result).eql({ major: 5, minor: 2 });
    });

    it("should fetch the correct version info for Linux", function() {
      var UA = "Mozilla/5.0 (X11; Linux i686 on x86_64; rv:10.0) Gecko/20100101 Firefox/10.0";
      var result = sharedUtils.getOSVersion(UA);

      // Linux version can't be determined correctly.
      expect(result).eql({ major: Infinity, minor: 0 });
    });
  });

  describe("#getPlatform", function() {
    it("should recognize the OSX userAgent string", function() {
      var UA = "Mozilla/5.0 (Macintosh; Intel Mac OS X 10.10; rv:37.0) Gecko/20100101 Firefox/37.0";
      var result = sharedUtils.getPlatform(UA);

      expect(result).eql("mac");
    });

    it("should recognize the Windows userAgent string", function() {
      var UA = "Mozilla/5.0 (Windows NT 6.1; Win64; x64; rv:10.0) Gecko/20100101 Firefox/10.0";
      var result = sharedUtils.getPlatform(UA);

      expect(result).eql("win");
    });

    it("should recognize the Linux userAgent string", function() {
      var UA = "Mozilla/5.0 (X11; Linux i686 on x86_64; rv:10.0) Gecko/20100101 Firefox/10.0";
      var result = sharedUtils.getPlatform(UA);

      expect(result).eql("other");
    });

    it("should recognize the OSX oscpu string", function() {
      var oscpu = "Intel Mac OS X 10.10";
      var result = sharedUtils.getPlatform(oscpu);

      expect(result).eql("mac");
    });

    it("should recognize the Windows oscpu string", function() {
      var oscpu = "Windows NT 5.3; Win64; x64";
      var result = sharedUtils.getPlatform(oscpu);

      expect(result).eql("win");
    });
  });

  describe("#objectDiff", function() {
    var a, b, diff;

    afterEach(function() {
      a = b = diff = null;
    });

    it("should find object property additions", function() {
      a = {
        prop1: null
      };
      b = {
        prop1: null,
        prop2: null
      };

      diff = sharedUtils.objectDiff(a, b);
      expect(diff.updated).to.eql([]);
      expect(diff.removed).to.eql([]);
      expect(diff.added).to.eql(["prop2"]);
    });

    it("should find object property value changes", function() {
      a = {
        prop1: null
      };
      b = {
        prop1: "null"
      };

      diff = sharedUtils.objectDiff(a, b);
      expect(diff.updated).to.eql(["prop1"]);
      expect(diff.removed).to.eql([]);
      expect(diff.added).to.eql([]);
    });

    it("should find object property removals", function() {
      a = {
        prop1: null
      };
      b = {};

      diff = sharedUtils.objectDiff(a, b);
      expect(diff.updated).to.eql([]);
      expect(diff.removed).to.eql(["prop1"]);
      expect(diff.added).to.eql([]);
    });

    it("should report a mix of removed, added and updated properties", function() {
      a = {
        prop1: null,
        prop2: null
      };
      b = {
        prop1: "null",
        prop3: null
      };

      diff = sharedUtils.objectDiff(a, b);
      expect(diff.updated).to.eql(["prop1"]);
      expect(diff.removed).to.eql(["prop2"]);
      expect(diff.added).to.eql(["prop3"]);
    });
  });

  describe("#stripFalsyValues", function() {
    var obj;

    afterEach(function() {
      obj = null;
    });

    it("should strip falsy object property values", function() {
      obj = {
        prop1: null,
        prop2: false,
        prop3: undefined,
        prop4: void 0,
        prop5: "",
        prop6: 0
      };

      sharedUtils.stripFalsyValues(obj);
      expect(obj).to.eql({});
    });

    it("should keep non-falsy values", function() {
      obj = {
        prop1: "null",
        prop2: null,
        prop3: true
      };

      sharedUtils.stripFalsyValues(obj);
      expect(obj).to.eql({ prop1: "null", prop3: true });
    });
  });
});
