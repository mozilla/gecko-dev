﻿/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
 * This class mimics a state machine and handles a list of commands by
 * executing them synchronously.
 *
 * @constructor
 * @param {object} framework
 *        A back reference to the framework which makes use of the class. It's
 *        getting passed in as parameter to each command callback.
 * @param {Array[]} [commandList=[]]
 *        Default commands to set during initialization
 */
function CommandChain(framework, commandList) {
  this._framework = framework;

  this._commands = commandList || [ ];
  this._current = 0;

  this.onFinished = null;
}

CommandChain.prototype = {

  /**
   * Returns the index of the current command of the chain
   *
   * @returns {number} Index of the current command
   */
  get current() {
    return this._current;
  },

  /**
   * Checks if the chain has already processed all the commands
   *
   * @returns {boolean} True, if all commands have been processed
   */
  get finished() {
    return this._current === this._commands.length;
  },

  /**
   * Returns the assigned commands of the chain.
   *
   * @returns {Array[]} Commands of the chain
   */
  get commands() {
    return this._commands;
  },

  /**
   * Sets new commands for the chain. All existing commands will be replaced.
   *
   * @param {Array[]} commands
   *        List of commands
   */
  set commands(commands) {
    this._commands = commands;
  },

  /**
   * Execute the next command in the chain.
   */
  executeNext : function () {
    var self = this;

    function _executeNext() {
      if (!self.finished) {
        var step = self._commands[self._current];
        self._current++;

        info("Run step: " + step[0]);  // Label
        step[1](self._framework);      // Execute step
      }
      else if (typeof(self.onFinished) === 'function') {
        self.onFinished();
      }
    }

    // To prevent building up the stack we have to execute the next
    // step asynchronously
    window.setTimeout(_executeNext, 0);
  },

  /**
   * Add new commands to the end of the chain
   *
   * @param {Array[]} commands
   *        List of commands
   */
  append: function (commands) {
    this._commands = this._commands.concat(commands);
  },

  /**
   * Returns the index of the specified command in the chain.
   *
   * @param {string} id
   *        Identifier of the command
   * @returns {number} Index of the command
   */
  indexOf: function (id) {
    for (var i = 0; i < this._commands.length; i++) {
      if (this._commands[i][0] === id) {
        return i;
      }
    }

    return -1;
  },

  /**
   * Inserts the new commands after the specified command.
   *
   * @param {string} id
   *        Identifier of the command
   * @param {Array[]} commands
   *        List of commands
   */
  insertAfter: function (id, commands) {
    var index = this.indexOf(id);

    if (index > -1) {
      var tail = this.removeAfter(id);

      this.append(commands);
      this.append(tail);
    }
  },

  /**
   * Inserts the new commands before the specified command.
   *
   * @param {string} id
   *        Identifier of the command
   * @param {Array[]} commands
   *        List of commands
   */
  insertBefore: function (id, commands) {
    var index = this.indexOf(id);

    if (index > -1) {
      var tail = this.removeAfter(id);
      var object = this.remove(id);

      this.append(commands);
      this.append(object);
      this.append(tail);
    }
  },

  /**
   * Removes the specified command
   *
   * @param {string} id
   *        Identifier of the command
   * @returns {object[]} Removed command
   */
  remove : function (id) {
    return this._commands.splice(this.indexOf(id), 1);
  },

  /**
   * Removes all commands after the specified one.
   *
   * @param {string} id
   *        Identifier of the command
   * @returns {object[]} Removed commands
   */
  removeAfter : function (id) {
    var index = this.indexOf(id);

    if (index > -1) {
      return this._commands.splice(index + 1);
    }

    return null;
  },

  /**
   * Removes all commands before the specified one.
   *
   * @param {string} id
   *        Identifier of the command
   * @returns {object[]} Removed commands
   */
  removeBefore : function (id) {
    var index = this.indexOf(id);

    if (index > -1) {
      return this._commands.splice(0, index);
    }

    return null;
  },

  /**
   * Replaces all commands after the specified one.
   *
   * @param {string} id
   *        Identifier of the command
   * @returns {object[]} Removed commands
   */
  replaceAfter : function (id, commands) {
    var oldCommands = this.removeAfter(id);
    this.append(commands);

    return oldCommands;
  },

  /**
   * Replaces all commands before the specified one.
   *
   * @param {string} id
   *        Identifier of the command
   * @returns {object[]} Removed commands
   */
  replaceBefore : function (id, commands) {
    var oldCommands = this.removeBefore(id);
    this.insertBefore(id, commands);

    return oldCommands;
  },

  /**
   * Remove all commands whose identifiers match the specified regex.
   *
   * @param {regex} id_match
   *        Regular expression to match command identifiers.
   */
  filterOut : function (id_match) {
    for (var i = this._commands.length - 1; i >= 0; i--) {
      if (id_match.test(this._commands[i][0])) {
        this._commands.splice(i, 1);
      }
    }
  }
};

/**
 * This class provides a state checker for media elements which store
 * a media stream to check for media attribute state and events fired.
 * When constructed by a caller, an object instance is created with
 * a media element, event state checkers for canplaythrough, timeupdate, and
 * time changing on the media element and stream.
 *
 * @param {HTMLMediaElement} element the media element being analyzed
 */
function MediaElementChecker(element) {
  this.element = element;
  this.canPlayThroughFired = false;
  this.timeUpdateFired = false;
  this.timePassed = false;

  var self = this;
  var elementId = self.element.getAttribute('id');

  // When canplaythrough fires, we track that it's fired and remove the
  // event listener.
  var canPlayThroughCallback = function() {
    info('canplaythrough fired for media element ' + elementId);
    self.canPlayThroughFired = true;
    self.element.removeEventListener('canplaythrough', canPlayThroughCallback,
                                     false);
  };

  // When timeupdate fires, we track that it's fired and check if time
  // has passed on the media stream and media element.
  var timeUpdateCallback = function() {
    self.timeUpdateFired = true;
    info('timeupdate fired for media element ' + elementId);

    // If time has passed, then track that and remove the timeupdate event
    // listener.
    if(element.mozSrcObject && element.mozSrcObject.currentTime > 0 &&
       element.currentTime > 0) {
      info('time passed for media element ' + elementId);
      self.timePassed = true;
      self.element.removeEventListener('timeupdate', timeUpdateCallback,
                                       false);
    }
  };

  element.addEventListener('canplaythrough', canPlayThroughCallback, false);
  element.addEventListener('timeupdate', timeUpdateCallback, false);
}

MediaElementChecker.prototype = {

  /**
   * Waits until the canplaythrough & timeupdate events to fire along with
   * ensuring time has passed on the stream and media element.
   *
   * @param {Function} onSuccess the success callback when media flow is
   *                             established
   */
  waitForMediaFlow : function MEC_WaitForMediaFlow(onSuccess) {
    var self = this;
    var elementId = self.element.getAttribute('id');
    info('Analyzing element: ' + elementId);

    if(self.canPlayThroughFired && self.timeUpdateFired && self.timePassed) {
      ok(true, 'Media flowing for ' + elementId);
      onSuccess();
    } else {
      setTimeout(function() {
        self.waitForMediaFlow(onSuccess);
      }, 100);
    }
  },

  /**
   * Checks if there is no media flow present by checking that the ready
   * state of the media element is HAVE_METADATA.
   */
  checkForNoMediaFlow : function MEC_CheckForNoMediaFlow() {
    ok(this.element.readyState === HTMLMediaElement.HAVE_METADATA,
       'Media element has a ready state of HAVE_METADATA');
  }
};

/**
 * Query function for determining if any IP address is available for
 * generating SDP.
 *
 * @return false if required additional network setup.
 */
function isNetworkReady() {
  // for gonk platform
  if ("nsINetworkInterfaceListService" in SpecialPowers.Ci) {
    var listService = SpecialPowers.Cc["@mozilla.org/network/interface-list-service;1"]
                        .getService(SpecialPowers.Ci.nsINetworkInterfaceListService);
    var itfList = listService.getDataInterfaceList(
          SpecialPowers.Ci.nsINetworkInterfaceListService.LIST_NOT_INCLUDE_MMS_INTERFACES |
          SpecialPowers.Ci.nsINetworkInterfaceListService.LIST_NOT_INCLUDE_SUPL_INTERFACES);
    var num = itfList.getNumberOfInterface();
    for (var i = 0; i < num; i++) {
      if (itfList.getInterface(i).ip) {
        info("Network interface is ready with address: " + itfList.getInterface(i).ip);
        return true;
      }
    }
    // ip address is not available
    info("Network interface is not ready, required additional network setup");
    return false;
  }
  info("Network setup is not required");
  return true;
}

/**
 * Network setup utils for Gonk
 *
 * @return {object} providing functions for setup/teardown data connection
 */
function getNetworkUtils() {
  var url = SimpleTest.getTestFileURL("NetworkPreparationChromeScript.js");
  var script = SpecialPowers.loadChromeScript(url);

  var utils = {
    /**
     * Utility for setting up data connection.
     *
     * @param aCallback callback after data connection is ready.
     */
    prepareNetwork: function(aCallback) {
      script.addMessageListener('network-ready', function (message) {
        info("Network interface is ready");
        aCallback();
      });
      info("Setup network interface");
      script.sendAsyncMessage("prepare-network", true);
    },
    /**
     * Utility for tearing down data connection.
     *
     * @param aCallback callback after data connection is closed.
     */
    tearDownNetwork: function(aCallback) {
      script.addMessageListener('network-disabled', function (message) {
        ok(true, 'network-disabled');
        script.destroy();
        aCallback();
      });
      script.sendAsyncMessage("network-cleanup", true);
    }
  };

  return utils;
}

/**
 * This class handles tests for peer connections.
 *
 * @constructor
 * @param {object} [options={}]
 *        Optional options for the peer connection test
 * @param {object} [options.commands=commandsPeerConnection]
 *        Commands to run for the test
 * @param {bool}   [options.is_local=true]
 *        true if this test should run the tests for the "local" side.
 * @param {bool}   [options.is_remote=true]
 *        true if this test should run the tests for the "remote" side.
 * @param {object} [options.config_pc1=undefined]
 *        Configuration for the local peer connection instance
 * @param {object} [options.config_pc2=undefined]
 *        Configuration for the remote peer connection instance. If not defined
 *        the configuration from the local instance will be used
 */
function PeerConnectionTest(options) {
  // If no options are specified make it an empty object
  options = options || { };
  options.commands = options.commands || commandsPeerConnection;
  options.is_local = "is_local" in options ? options.is_local : true;
  options.is_remote = "is_remote" in options ? options.is_remote : true;

  var netTeardownCommand = null;
  if (!isNetworkReady()) {
    var utils = getNetworkUtils();
    // Trigger network setup to obtain IP address before creating any PeerConnection.
    utils.prepareNetwork(function() {
      ok(isNetworkReady(),'setup network connection successfully');
    });

    netTeardownCommand = [
      [
        'TEARDOWN_NETWORK',
        function(test) {
          utils.tearDownNetwork(function() {
            info('teardown network connection');
            test.next();
          });
        }
      ]
    ];
  }

  if (options.is_local)
    this.pcLocal = new PeerConnectionWrapper('pcLocal', options.config_pc1);
  else
    this.pcLocal = null;

  if (options.is_remote)
    this.pcRemote = new PeerConnectionWrapper('pcRemote', options.config_pc2 || options.config_pc1);
  else
    this.pcRemote = null;

  this.connected = false;

  // Create command chain instance and assign default commands
  this.chain = new CommandChain(this, options.commands);
  if (!options.is_local) {
    this.chain.filterOut(/^PC_LOCAL/);
  }
  if (!options.is_remote) {
    this.chain.filterOut(/^PC_REMOTE/);
  }

  // Insert network teardown after testcase execution.
  if (netTeardownCommand) {
    this.chain.append(netTeardownCommand);
  }

  var self = this;
  this.chain.onFinished = function () {
    self.teardown();
  };
}

/**
 * Closes the peer connection if it is active
 *
 * @param {Function} onSuccess
 *        Callback to execute when the peer connection has been closed successfully
 */
PeerConnectionTest.prototype.close = function PCT_close(onSuccess) {
  info("Closing peer connections. Connection state=" + this.connected);

  // There is no onclose event for the remote peer existent yet. So close it
  // side-by-side with the local peer.
  if (this.pcLocal)
    this.pcLocal.close();
  if (this.pcRemote)
    this.pcRemote.close();
  this.connected = false;

  onSuccess();
};

/**
 * Executes the next command.
 */
PeerConnectionTest.prototype.next = function PCT_next() {
  this.chain.executeNext();
};

/**
 * Creates an answer for the specified peer connection instance
 * and automatically handles the failure case.
 *
 * @param {PeerConnectionWrapper} peer
 *        The peer connection wrapper to run the command on
 * @param {function} onSuccess
 *        Callback to execute if the offer was created successfully
 */
PeerConnectionTest.prototype.createAnswer =
function PCT_createAnswer(peer, onSuccess) {
  peer.createAnswer(function (answer) {
    onSuccess(answer);
  });
};

/**
 * Creates an offer for the specified peer connection instance
 * and automatically handles the failure case.
 *
 * @param {PeerConnectionWrapper} peer
 *        The peer connection wrapper to run the command on
 * @param {function} onSuccess
 *        Callback to execute if the offer was created successfully
 */
PeerConnectionTest.prototype.createOffer =
function PCT_createOffer(peer, onSuccess) {
  peer.createOffer(function (offer) {
    onSuccess(offer);
  });
};

/**
 * Sets the local description for the specified peer connection instance
 * and automatically handles the failure case.
 *
 * @param {PeerConnectionWrapper} peer
          The peer connection wrapper to run the command on
 * @param {mozRTCSessionDescription} desc
 *        Session description for the local description request
 * @param {function} onSuccess
 *        Callback to execute if the local description was set successfully
 */
PeerConnectionTest.prototype.setLocalDescription =
function PCT_setLocalDescription(peer, desc, onSuccess) {
  var eventFired = false;
  var stateChanged = false;

  function check_next_test() {
    if (eventFired && stateChanged) {
      onSuccess();
    }
  }

  peer.onsignalingstatechange = function () {
    info(peer + ": 'onsignalingstatechange' event registered for async check");

    eventFired = true;
    check_next_test();
  };

  peer.setLocalDescription(desc, function () {
    stateChanged = true;
    check_next_test();
  });
};

/**
 * Sets the media constraints for both peer connection instances.
 *
 * @param {object} constraintsLocal
 *        Media constrains for the local peer connection instance
 * @param constraintsRemote
 */
PeerConnectionTest.prototype.setMediaConstraints =
function PCT_setMediaConstraints(constraintsLocal, constraintsRemote) {
  if (this.pcLocal)
    this.pcLocal.constraints = constraintsLocal;
  if (this.pcRemote)
    this.pcRemote.constraints = constraintsRemote;
};

/**
 * Sets the media constraints used on a createOffer call in the test.
 *
 * @param {object} constraints the media constraints to use on createOffer
 */
PeerConnectionTest.prototype.setOfferConstraints =
function PCT_setOfferConstraints(constraints) {
  if (this.pcLocal)
    this.pcLocal.offerConstraints = constraints;
};

/**
 * Sets the remote description for the specified peer connection instance
 * and automatically handles the failure case.
 *
 * @param {PeerConnectionWrapper} peer
          The peer connection wrapper to run the command on
 * @param {mozRTCSessionDescription} desc
 *        Session description for the remote description request
 * @param {function} onSuccess
 *        Callback to execute if the local description was set successfully
 */
PeerConnectionTest.prototype.setRemoteDescription =
function PCT_setRemoteDescription(peer, desc, onSuccess) {
  var eventFired = false;
  var stateChanged = false;

  function check_next_test() {
    if (eventFired && stateChanged) {
      onSuccess();
    }
  }

  peer.onsignalingstatechange = function () {
    info(peer + ": 'onsignalingstatechange' event registered for async check");

    eventFired = true;
    check_next_test();
  };

  peer.setRemoteDescription(desc, function () {
    stateChanged = true;
    check_next_test();
  });
};

/**
 * Start running the tests as assigned to the command chain.
 */
PeerConnectionTest.prototype.run = function PCT_run() {
  this.next();
};

/**
 * Clean up the objects used by the test
 */
PeerConnectionTest.prototype.teardown = function PCT_teardown() {
  this.close(function () {
    info("Test finished");
    if (window.SimpleTest)
      SimpleTest.finish();
    else
      finish();
  });
};

/**
 * This class handles tests for data channels.
 *
 * @constructor
 * @param {object} [options={}]
 *        Optional options for the peer connection test
 * @param {object} [options.commands=commandsDataChannel]
 *        Commands to run for the test
 * @param {object} [options.config_pc1=undefined]
 *        Configuration for the local peer connection instance
 * @param {object} [options.config_pc2=undefined]
 *        Configuration for the remote peer connection instance. If not defined
 *        the configuration from the local instance will be used
 */
function DataChannelTest(options) {
  options = options || { };
  options.commands = options.commands || commandsDataChannel;

  PeerConnectionTest.call(this, options);
}

DataChannelTest.prototype = Object.create(PeerConnectionTest.prototype, {
  close : {
    /**
     * Close the open data channels, followed by the underlying peer connection
     *
     * @param {Function} onSuccess
     *        Callback to execute when the connection has been closed
     */
    value : function DCT_close(onSuccess) {
      var self = this;

      function _closeChannels() {
        var length = self.pcLocal.dataChannels.length;

        if (length > 0) {
          self.closeDataChannel(length - 1, function () {
            _closeChannels();
          });
        }
        else {
          PeerConnectionTest.prototype.close.call(self, onSuccess);
        }
      }

      _closeChannels();
    }
  },

  closeDataChannel : {
    /**
     * Close the specified data channel
     *
     * @param {Number} index
     *        Index of the data channel to close on both sides
     * @param {Function} onSuccess
     *        Callback to execute when the data channel has been closed
     */
    value : function DCT_closeDataChannel(index, onSuccess) {
      var localChannel = this.pcLocal.dataChannels[index];
      var remoteChannel = this.pcRemote.dataChannels[index];

      var self = this;

      // Register handler for remote channel, cause we have to wait until
      // the current close operation has been finished.
      remoteChannel.onclose = function () {
        self.pcRemote.dataChannels.splice(index, 1);

        onSuccess(remoteChannel);
      };

      localChannel.close();
      this.pcLocal.dataChannels.splice(index, 1);
    }
  },

  createDataChannel : {
    /**
     * Create a data channel
     *
     * @param {Dict} options
     *        Options for the data channel (see nsIPeerConnection)
     * @param {Function} onSuccess
     *        Callback when the creation was successful
     */
    value : function DCT_createDataChannel(options, onSuccess) {
      var localChannel = null;
      var remoteChannel = null;
      var self = this;

      // Method to synchronize all asynchronous events.
      function check_next_test() {
        if (self.connected && localChannel && remoteChannel) {
          onSuccess(localChannel, remoteChannel);
        }
      }

      if (!options.negotiated) {
        // Register handlers for the remote peer
        this.pcRemote.registerDataChannelOpenEvents(function (channel) {
          remoteChannel = channel;
          check_next_test();
        });
      }

      // Create the datachannel and handle the local 'onopen' event
      this.pcLocal.createDataChannel(options, function (channel) {
        localChannel = channel;

        if (options.negotiated) {
          // externally negotiated - we need to open from both ends
          options.id = options.id || channel.id;  // allow for no id to let the impl choose
          self.pcRemote.createDataChannel(options, function (channel) {
            remoteChannel = channel;
            check_next_test();
          });
        } else {
          check_next_test();
	}
      });
    }
  },

  send : {
    /**
     * Send data (message or blob) to the other peer
     *
     * @param {String|Blob} data
     *        Data to send to the other peer. For Blobs the MIME type will be lost.
     * @param {Function} onSuccess
     *        Callback to execute when data has been sent
     * @param {Object} [options={ }]
     *        Options to specify the data channels to be used
     * @param {DataChannelWrapper} [options.sourceChannel=pcLocal.dataChannels[length - 1]]
     *        Data channel to use for sending the message
     * @param {DataChannelWrapper} [options.targetChannel=pcRemote.dataChannels[length - 1]]
     *        Data channel to use for receiving the message
     */
    value : function DCT_send(data, onSuccess, options) {
      options = options || { };
      source = options.sourceChannel ||
               this.pcLocal.dataChannels[this.pcLocal.dataChannels.length - 1];
      target = options.targetChannel ||
               this.pcRemote.dataChannels[this.pcRemote.dataChannels.length - 1];

      // Register event handler for the target channel
      target.onmessage = function (recv_data) {
        onSuccess(target, recv_data);
      };

      source.send(data);
    }
  },

  setLocalDescription : {
    /**
     * Sets the local description for the specified peer connection instance
     * and automatically handles the failure case. In case for the final call
     * it will setup the requested datachannel.
     *
     * @param {PeerConnectionWrapper} peer
              The peer connection wrapper to run the command on
     * @param {mozRTCSessionDescription} desc
     *        Session description for the local description request
     * @param {function} onSuccess
     *        Callback to execute if the local description was set successfully
     */
    value : function DCT_setLocalDescription(peer, desc, onSuccess) {
      // If the peer has a remote offer we are in the final call, and have
      // to wait for the datachannel connection to be open. It will also set
      // the local description internally.
      if (peer.signalingState === 'have-remote-offer') {
        this.waitForInitialDataChannel(peer, desc, onSuccess);
      }
      else {
        PeerConnectionTest.prototype.setLocalDescription.call(this, peer,
                                                              desc, onSuccess);
      }

    }
  },

  waitForInitialDataChannel : {
    /**
     * Create an initial data channel before the peer connection has been connected
     *
     * @param {PeerConnectionWrapper} peer
              The peer connection wrapper to run the command on
     * @param {mozRTCSessionDescription} desc
     *        Session description for the local description request
     * @param {Function} onSuccess
     *        Callback when the creation was successful
     */
    value : function DCT_waitForInitialDataChannel(peer, desc, onSuccess) {
      var self = this;

      var targetPeer = peer;
      var targetChannel = null;

      var sourcePeer = (peer == this.pcLocal) ? this.pcRemote : this.pcLocal;
      var sourceChannel = null;

      // Method to synchronize all asynchronous events which current happen
      // due to a non-predictable flow. With bug 875346 fixed we will be able
      // to simplify this code.
      function check_next_test() {
        if (self.connected && sourceChannel && targetChannel) {
          onSuccess(sourceChannel, targetChannel);
        }
      }

      // Register 'onopen' handler for the first local data channel
      sourcePeer.dataChannels[0].onopen = function (channel) {
        sourceChannel = channel;
        check_next_test();
      };

      // Register handlers for the target peer
      targetPeer.registerDataChannelOpenEvents(function (channel) {
        targetChannel = channel;
        check_next_test();
      });

      PeerConnectionTest.prototype.setLocalDescription.call(this, targetPeer, desc,
        function () {
          self.connected = true;
          check_next_test();
        }
      );
    }
  }
});

/**
 * This class acts as a wrapper around a DataChannel instance.
 *
 * @param dataChannel
 * @param peerConnectionWrapper
 * @constructor
 */
function DataChannelWrapper(dataChannel, peerConnectionWrapper) {
  this._channel = dataChannel;
  this._pc = peerConnectionWrapper;

  info("Creating " + this);

  /**
   * Setup appropriate callbacks
   */

  this.onclose = unexpectedEventAndFinish(this, 'onclose');
  this.onerror = unexpectedEventAndFinish(this, 'onerror');
  this.onmessage = unexpectedEventAndFinish(this, 'onmessage');
  this.onopen = unexpectedEventAndFinish(this, 'onopen');

  var self = this;

  /**
   * Callback for native data channel 'onclose' events. If no custom handler
   * has been specified via 'this.onclose', a failure will be raised if an
   * event of this type gets caught.
   */
  this._channel.onclose = function () {
    info(self + ": 'onclose' event fired");

    self.onclose(self);
    self.onclose = unexpectedEventAndFinish(self, 'onclose');
  };

  /**
   * Callback for native data channel 'onmessage' events. If no custom handler
   * has been specified via 'this.onmessage', a failure will be raised if an
   * event of this type gets caught.
   *
   * @param {Object} event
   *        Event data which includes the sent message
   */
  this._channel.onmessage = function (event) {
    info(self + ": 'onmessage' event fired for '" + event.data + "'");

    self.onmessage(event.data);
    self.onmessage = unexpectedEventAndFinish(self, 'onmessage');
  };

  /**
   * Callback for native data channel 'onopen' events. If no custom handler
   * has been specified via 'this.onopen', a failure will be raised if an
   * event of this type gets caught.
   */
  this._channel.onopen = function () {
    info(self + ": 'onopen' event fired");

    self.onopen(self);
    self.onopen = unexpectedEventAndFinish(self, 'onopen');
  };
}

DataChannelWrapper.prototype = {
  /**
   * Returns the binary type of the channel
   *
   * @returns {String} The binary type
   */
  get binaryType() {
    return this._channel.binaryType;
  },

  /**
   * Sets the binary type of the channel
   *
   * @param {String} type
   *        The new binary type of the channel
   */
  set binaryType(type) {
    this._channel.binaryType = type;
  },

  /**
   * Returns the label of the underlying data channel
   *
   * @returns {String} The label
   */
  get label() {
    return this._channel.label;
  },

  /**
   * Returns the protocol of the underlying data channel
   *
   * @returns {String} The protocol
   */
  get protocol() {
    return this._channel.protocol;
  },

  /**
   * Returns the id of the underlying data channel
   *
   * @returns {number} The stream id
   */
  get id() {
    return this._channel.id;
  },

  /**
   * Returns the reliable state of the underlying data channel
   *
   * @returns {bool} The stream's reliable state
   */
  get reliable() {
    return this._channel.reliable;
  },

  // ordered, maxRetransmits and maxRetransmitTime not exposed yet

  /**
   * Returns the readyState bit of the data channel
   *
   * @returns {String} The state of the channel
   */
  get readyState() {
    return this._channel.readyState;
  },

  /**
   * Close the data channel
   */
  close : function () {
    info(this + ": Closing channel");
    this._channel.close();
  },

  /**
   * Send data through the data channel
   *
   * @param {String|Object} data
   *        Data which has to be sent through the data channel
   */
  send: function DCW_send(data) {
    info(this + ": Sending data '" + data + "'");
    this._channel.send(data);
  },

  /**
   * Returns the string representation of the class
   *
   * @returns {String} The string representation
   */
  toString: function DCW_toString() {
    return "DataChannelWrapper (" + this._pc.label + '_' + this._channel.label + ")";
  }
};


/**
 * This class acts as a wrapper around a PeerConnection instance.
 *
 * @constructor
 * @param {string} label
 *        Description for the peer connection instance
 * @param {object} configuration
 *        Configuration for the peer connection instance
 */
function PeerConnectionWrapper(label, configuration) {
  this.configuration = configuration;
  this.label = label;

  this.constraints = [ ];
  this.offerConstraints = {};
  this.streams = [ ];
  this.mediaCheckers = [ ];

  this.dataChannels = [ ];

  info("Creating " + this);
  this._pc = new mozRTCPeerConnection(this.configuration);
  is(this._pc.iceConnectionState, "new", "iceConnectionState starts at 'new'");

  /**
   * Setup callback handlers
   */
  var self = this;
  // This enables tests to validate that the next ice state is the one they expect to happen
  this.next_ice_state = ""; // in most cases, the next state will be "checking", but in some tests "closed"
  // This allows test to register their own callbacks for ICE connection state changes
  this.ice_connection_callbacks = [ ];

  this._pc.oniceconnectionstatechange = function() {
      ok(self._pc.iceConnectionState != undefined, "iceConnectionState should not be undefined");
      info(self + ": oniceconnectionstatechange fired, new state is: " + self._pc.iceConnectionState);
      if (Object.keys(self.ice_connection_callbacks).length >= 1) {
        var it = Iterator(self.ice_connection_callbacks);
        var name = "";
        var callback = "";
        for ([name, callback] in it) {
          callback();
        }
      }
      if (self.next_ice_state != "") {
        is(self._pc.iceConnectionState, self.next_ice_state, "iceConnectionState changed to '" +
           self.next_ice_state + "'");
        self.next_ice_state = "";
      }
  };
  this.ondatachannel = unexpectedEventAndFinish(this, 'ondatachannel');
  this.onsignalingstatechange = unexpectedEventAndFinish(this, 'onsignalingstatechange');

  /**
   * Callback for native peer connection 'onaddstream' events.
   *
   * @param {Object} event
   *        Event data which includes the stream to be added
   */
  this._pc.onaddstream = function (event) {
    info(self + ": 'onaddstream' event fired for " + event.stream);

    // TODO: Bug 834835 - Assume type is video until we get get{Audio,Video}Tracks.
    self.attachMedia(event.stream, 'video', 'remote');
   };

  /**
   * Callback for native peer connection 'ondatachannel' events. If no custom handler
   * has been specified via 'this.ondatachannel', a failure will be raised if an
   * event of this type gets caught.
   *
   * @param {Object} event
   *        Event data which includes the newly created data channel
   */
  this._pc.ondatachannel = function (event) {
    info(self + ": 'ondatachannel' event fired for " + event.channel.label);

    self.ondatachannel(new DataChannelWrapper(event.channel, self));
    self.ondatachannel = unexpectedEventAndFinish(self, 'ondatachannel');
  }

  /**
   * Callback for native peer connection 'onsignalingstatechange' events. If no
   * custom handler has been specified via 'this.onsignalingstatechange', a
   * failure will be raised if an event of this type is caught.
   *
   * @param {Object} aEvent
   *        Event data which includes the newly created data channel
   */
  this._pc.onsignalingstatechange = function (aEvent) {
    info(self + ": 'onsignalingstatechange' event fired");

    self.onsignalingstatechange();
    self.onsignalingstatechange = unexpectedEventAndFinish(self, 'onsignalingstatechange');
  }
}

PeerConnectionWrapper.prototype = {

  /**
   * Returns the local description.
   *
   * @returns {object} The local description
   */
  get localDescription() {
    return this._pc.localDescription;
  },

  /**
   * Sets the local description.
   *
   * @param {object} desc
   *        The new local description
   */
  set localDescription(desc) {
    this._pc.localDescription = desc;
  },

  /**
   * Returns the readyState.
   *
   * @returns {string}
   */
  get readyState() {
    return this._pc.readyState;
  },

  /**
   * Returns the remote description.
   *
   * @returns {object} The remote description
   */
  get remoteDescription() {
    return this._pc.remoteDescription;
  },

  /**
   * Sets the remote description.
   *
   * @param {object} desc
   *        The new remote description
   */
  set remoteDescription(desc) {
    this._pc.remoteDescription = desc;
  },

  /**
   * Returns the signaling state.
   *
   * @returns {object} The local description
   */
  get signalingState() {
    return this._pc.signalingState;
  },
  /**
   * Returns the ICE connection state.
   *
   * @returns {object} The local description
   */
  get iceConnectionState() {
    return this._pc.iceConnectionState;
  },

  /**
   * Callback when we get media from either side. Also an appropriate
   * HTML media element will be created.
   *
   * @param {MediaStream} stream
   *        Media stream to handle
   * @param {string} type
   *        The type of media stream ('audio' or 'video')
   * @param {string} side
   *        The location the stream is coming from ('local' or 'remote')
   */
  attachMedia : function PCW_attachMedia(stream, type, side) {
    info("Got media stream: " + type + " (" + side + ")");
    this.streams.push(stream);

    if (side === 'local') {
      this._pc.addStream(stream);
    }

    var element = createMediaElement(type, this.label + '_' + side);
    this.mediaCheckers.push(new MediaElementChecker(element));
    element.mozSrcObject = stream;
    element.play();
  },

  /**
   * Requests all the media streams as specified in the constrains property.
   *
   * @param {function} onSuccess
   *        Callback to execute if all media has been requested successfully
   */
  getAllUserMedia : function PCW_GetAllUserMedia(onSuccess) {
    var self = this;

    function _getAllUserMedia(constraintsList, index) {
      if (index < constraintsList.length) {
        var constraints = constraintsList[index];

        getUserMedia(constraints, function (stream) {
          var type = '';

          if (constraints.audio) {
            type = 'audio';
          }

          if (constraints.video) {
            type += 'video';
          }

          self.attachMedia(stream, type, 'local');

          _getAllUserMedia(constraintsList, index + 1);
        }, unexpectedCallbackAndFinish());
      } else {
        onSuccess();
      }
    }

    info("Get " + this.constraints.length + " local streams");
    _getAllUserMedia(this.constraints, 0);
  },

  /**
   * Create a new data channel instance
   *
   * @param {Object} options
   *        Options which get forwarded to nsIPeerConnection.createDataChannel
   * @param {function} [onCreation=undefined]
   *        Callback to execute when the local data channel has been created
   * @returns {DataChannelWrapper} The created data channel
   */
  createDataChannel : function PCW_createDataChannel(options, onCreation) {
    var label = 'channel_' + this.dataChannels.length;
    info(this + ": Create data channel '" + label);

    var channel = this._pc.createDataChannel(label, options);
    var wrapper = new DataChannelWrapper(channel, this);

    if (onCreation) {
      wrapper.onopen = function () {
        onCreation(wrapper);
      }
    }

    this.dataChannels.push(wrapper);
    return wrapper;
  },

  /**
   * Creates an offer and automatically handles the failure case.
   *
   * @param {function} onSuccess
   *        Callback to execute if the offer was created successfully
   */
  createOffer : function PCW_createOffer(onSuccess) {
    var self = this;

    this._pc.createOffer(function (offer) {
      info("Got offer: " + JSON.stringify(offer));
      self._last_offer = offer;
      onSuccess(offer);
    }, unexpectedCallbackAndFinish(), this.offerConstraints);
  },

  /**
   * Creates an answer and automatically handles the failure case.
   *
   * @param {function} onSuccess
   *        Callback to execute if the answer was created successfully
   */
  createAnswer : function PCW_createAnswer(onSuccess) {
    var self = this;

    this._pc.createAnswer(function (answer) {
      info(self + ": Got answer: " + JSON.stringify(answer));
      self._last_answer = answer;
      onSuccess(answer);
    }, unexpectedCallbackAndFinish());
  },

  /**
   * Sets the local description and automatically handles the failure case.
   *
   * @param {object} desc
   *        mozRTCSessionDescription for the local description request
   * @param {function} onSuccess
   *        Callback to execute if the local description was set successfully
   */
  setLocalDescription : function PCW_setLocalDescription(desc, onSuccess) {
    var self = this;
    this._pc.setLocalDescription(desc, function () {
      info(self + ": Successfully set the local description");
      onSuccess();
    }, unexpectedCallbackAndFinish());
  },

  /**
   * Tries to set the local description and expect failure. Automatically
   * causes the test case to fail if the call succeeds.
   *
   * @param {object} desc
   *        mozRTCSessionDescription for the local description request
   * @param {function} onFailure
   *        Callback to execute if the call fails.
   */
  setLocalDescriptionAndFail : function PCW_setLocalDescriptionAndFail(desc, onFailure) {
    var self = this;
    this._pc.setLocalDescription(desc,
      unexpectedCallbackAndFinish("setLocalDescription should have failed."),
      function (err) {
        info(self + ": As expected, failed to set the local description");
        onFailure(err);
    });
  },

  /**
   * Sets the remote description and automatically handles the failure case.
   *
   * @param {object} desc
   *        mozRTCSessionDescription for the remote description request
   * @param {function} onSuccess
   *        Callback to execute if the remote description was set successfully
   */
  setRemoteDescription : function PCW_setRemoteDescription(desc, onSuccess) {
    var self = this;
    this._pc.setRemoteDescription(desc, function () {
      info(self + ": Successfully set remote description");
      onSuccess();
    }, unexpectedCallbackAndFinish());
  },

  /**
   * Tries to set the remote description and expect failure. Automatically
   * causes the test case to fail if the call succeeds.
   *
   * @param {object} desc
   *        mozRTCSessionDescription for the remote description request
   * @param {function} onFailure
   *        Callback to execute if the call fails.
   */
  setRemoteDescriptionAndFail : function PCW_setRemoteDescriptionAndFail(desc, onFailure) {
    var self = this;
    this._pc.setRemoteDescription(desc,
      unexpectedCallbackAndFinish("setRemoteDescription should have failed."),
      function (err) {
        info(self + ": As expected, failed to set the remote description");
        onFailure(err);
    });
  },

  /**
   * Adds an ICE candidate and automatically handles the failure case.
   *
   * @param {object} candidate
   *        SDP candidate
   * @param {function} onSuccess
   *        Callback to execute if the local description was set successfully
   */
  addIceCandidate : function PCW_addIceCandidate(candidate, onSuccess) {
    var self = this;

    this._pc.addIceCandidate(candidate, function () {
      info(self + ": Successfully added an ICE candidate");
      onSuccess();
    }, unexpectedCallbackAndFinish());
  },

  /**
   * Tries to add an ICE candidate and expects failure. Automatically
   * causes the test case to fail if the call succeeds.
   *
   * @param {object} candidate
   *        SDP candidate
   * @param {function} onFailure
   *        Callback to execute if the call fails.
   */
  addIceCandidateAndFail : function PCW_addIceCandidateAndFail(candidate, onFailure) {
    var self = this;

    this._pc.addIceCandidate(candidate,
      unexpectedCallbackAndFinish("addIceCandidate should have failed."),
      function (err) {
        info(self + ": As expected, failed to add an ICE candidate");
        onFailure(err);
    }) ;
  },

  /**
   * Returns if the ICE the connection state is "connected".
   *
   * @returns {boolean} True is the connection state is "connected", otherwise false.
   */
  isIceConnected : function PCW_isIceConnected() {
    info("iceConnectionState: " + this.iceConnectionState);
    return this.iceConnectionState === "connected";
  },

  /**
   * Returns if the ICE the connection state is "checking".
   *
   * @returns {boolean} True is the connection state is "checking", otherwise false.
   */
  isIceChecking : function PCW_isIceChecking() {
    return this.iceConnectionState === "checking";
  },

  /**
   * Returns if the ICE the connection state is "new".
   *
   * @returns {boolean} True is the connection state is "new", otherwise false.
   */
  isIceNew : function PCW_isIceNew() {
    return this.iceConnectionState === "new";
  },

  /**
   * Registers a callback for the ICE connection state change and
   * reports success (=connected) or failure via the callbacks.
   * States "new" and "checking" are ignored.
   *
   * @param {function} onSuccess
   *        Callback if ICE connection status is "connected".
   * @param {function} onFailure
   *        Callback if ICE connection reaches a different state than
   *        "new", "checking" or "connected".
   */
  waitForIceConnected : function PCW_waitForIceConnected(onSuccess, onFailure) {
    var self = this;
    var mySuccess = onSuccess;
    var myFailure = onFailure;

    function iceConnectedChanged () {
      if (self.isIceConnected()) {
        delete self.ice_connection_callbacks["waitForIceConnected"];
        mySuccess();
      } else if (! (self.isIceChecking() || self.isIceNew())) {
        delete self.ice_connection_callbacks["waitForIceConnected"];
        myFailure();
      }
    };

    self.ice_connection_callbacks["waitForIceConnected"] = (function() {iceConnectedChanged()});
  },

  /**
   * Checks that we are getting the media streams we expect.
   *
   * @param {object} constraintsRemote
   *        The media constraints of the remote peer connection object
   */
  checkMediaStreams : function PCW_checkMediaStreams(constraintsRemote) {
    is(this._pc.getLocalStreams().length, this.constraints.length,
       this + ' has ' + this.constraints.length + ' local streams');

    // TODO: change this when multiple incoming streams are supported (bug 834835)
    is(this._pc.getRemoteStreams().length, 1,
       this + ' has ' + 1 + ' remote streams');
  },

  /**
   * Check that media flow is present on all media elements involved in this
   * test by waiting for confirmation that media flow is present.
   *
   * @param {Function} onSuccess the success callback when media flow
   *                             is confirmed on all media elements
   */
  checkMediaFlowPresent : function PCW_checkMediaFlowPresent(onSuccess) {
    var self = this;

    function _checkMediaFlowPresent(index, onSuccess) {
      if(index >= self.mediaCheckers.length) {
        onSuccess();
      } else {
        var mediaChecker = self.mediaCheckers[index];
        mediaChecker.waitForMediaFlow(function() {
          _checkMediaFlowPresent(index + 1, onSuccess);
        });
      }
    }

    _checkMediaFlowPresent(0, onSuccess);
  },

  /**
   * Check that stats are present by checking for known stats.
   *
   * @param {Function} onSuccess the success callback to return stats to
   */
  getStats : function PCW_getStats(selector, onSuccess) {
    var self = this;

    this._pc.getStats(selector, function(stats) {
      info(self + ": Got stats: " + JSON.stringify(stats));
      self._last_stats = stats;
      onSuccess(stats);
    }, unexpectedCallbackAndFinish());
  },

  /**
   * Checks that we are getting the media streams we expect.
   *
   * @param {object} stats
   *        The stats to check from this PeerConnectionWrapper
   */
  checkStats : function PCW_checkStats(stats) {
    function toNum(obj) {
      return obj? obj : 0;
    }
    function numTracks(streams) {
      var n = 0;
      streams.forEach(function(stream) {
          n += stream.getAudioTracks().length + stream.getVideoTracks().length;
        });
      return n;
    }

    // Use spec way of enumerating stats
    var counters = {};
    for (var key in stats) {
      if (stats.hasOwnProperty(key)) {
        var res = stats[key];
        if (!res.isRemote) {
          counters[res.type] = toNum(counters[res.type]) + 1;
        }
      }
    }
    // Use MapClass way of enumerating stats
    var counters2 = {};
    stats.forEach(function(res) {
        if (!res.isRemote) {
          counters2[res.type] = toNum(counters2[res.type]) + 1;
        }
      });
    is(JSON.stringify(counters), JSON.stringify(counters2),
       "Spec and MapClass variant of RTCStatsReport enumeration agree");
    var nin = numTracks(this._pc.getRemoteStreams());
    var nout = numTracks(this._pc.getLocalStreams());

    // TODO(Bug 957145): Restore stronger inboundrtp test once Bug 948249 is fixed
    //is(toNum(counters["inboundrtp"]), nin, "Have " + nin + " inboundrtp stat(s)");
    ok(toNum(counters["inboundrtp"]) >= nin, "Have at least " + nin + " inboundrtp stat(s) *");

    is(toNum(counters["outboundrtp"]), nout, "Have " + nout + " outboundrtp stat(s)");

    var numLocalCandidates  = toNum(counters["localcandidate"]);
    var numRemoteCandidates = toNum(counters["remotecandidate"]);
    // If there are no tracks, there will be no stats either.
    if (nin + nout > 0) {
      ok(numLocalCandidates, "Have localcandidate stat(s)");
      ok(numRemoteCandidates, "Have remotecandidate stat(s)");
    } else {
      is(numLocalCandidates, 0, "Have no localcandidate stats");
      is(numRemoteCandidates, 0, "Have no remotecandidate stats");
    }
  },

  /**
   * Closes the connection
   */
  close : function PCW_close() {
    // It might be that a test has already closed the pc. In those cases
    // we should not fail.
    try {
      this._pc.close();
      info(this + ": Closed connection.");
    }
    catch (e) {
      info(this + ": Failure in closing connection - " + e.message);
    }
  },

  /**
   * Register all events during the setup of the data channel
   *
   * @param {Function} onDataChannelOpened
   *        Callback to execute when the data channel has been opened
   */
  registerDataChannelOpenEvents : function (onDataChannelOpened) {
    info(this + ": Register callbacks for 'ondatachannel' and 'onopen'");

    this.ondatachannel = function (targetChannel) {
      targetChannel.onopen = function (targetChannel) {
        onDataChannelOpened(targetChannel);
      };

      this.dataChannels.push(targetChannel);
    }
  },

  /**
   * Returns the string representation of the class
   *
   * @returns {String} The string representation
   */
  toString : function PCW_toString() {
    return "PeerConnectionWrapper (" + this.label + ")";
  }
};
