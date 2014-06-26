/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {Cc, Ci, Cu, Cr} = require("chrome");
const events = require("sdk/event/core");
const {Promise: promise} = Cu.import("resource://gre/modules/Promise.jsm", {});
const protocol = require("devtools/server/protocol");
const {CallWatcherActor, CallWatcherFront} = require("devtools/server/actors/call-watcher");
const DevToolsUtils = require("devtools/toolkit/DevToolsUtils.js");

const {on, once, off, emit} = events;
const {method, custom, Arg, Option, RetVal} = protocol;

const CANVAS_CONTEXTS = [
  "CanvasRenderingContext2D",
  "WebGLRenderingContext"
];

const ANIMATION_GENERATORS = [
  "requestAnimationFrame",
  "mozRequestAnimationFrame"
];

const DRAW_CALLS = [
  // 2D canvas
  "fill",
  "stroke",
  "clearRect",
  "fillRect",
  "strokeRect",
  "fillText",
  "strokeText",
  "drawImage",

  // WebGL
  "clear",
  "drawArrays",
  "drawElements",
  "finish",
  "flush"
];

const INTERESTING_CALLS = [
  // 2D canvas
  "save",
  "restore",

  // WebGL
  "useProgram"
];

exports.register = function(handle) {
  handle.addTabActor(CanvasActor, "canvasActor");
  handle.addGlobalActor(CanvasActor, "canvasActor");
};

exports.unregister = function(handle) {
  handle.removeTabActor(CanvasActor);
  handle.removeGlobalActor(CanvasActor);
};

/**
 * Type representing an Uint32Array buffer, serialized fast(er).
 *
 * XXX: It would be nice if on local connections (only), we could just *give*
 * the buffer directly to the front, instead of going through all this
 * serialization redundancy.
 */
protocol.types.addType("uint32-array", {
  write: (v) => "[" + Array.join(v, ",") + "]",
  read: (v) => new Uint32Array(JSON.parse(v))
});

/**
 * Type describing a thumbnail or screenshot in a recorded animation frame.
 */
protocol.types.addDictType("snapshot-image", {
  index: "number",
  width: "number",
  height: "number",
  flipped: "boolean",
  pixels: "uint32-array"
});

/**
 * Type describing an overview of a recorded animation frame.
 */
protocol.types.addDictType("snapshot-overview", {
  calls: "array:function-call",
  thumbnails: "array:snapshot-image",
  screenshot: "snapshot-image"
});

/**
 * This actor represents a recorded animation frame snapshot, along with
 * all the corresponding canvas' context methods invoked in that frame,
 * thumbnails for each draw call and a screenshot of the end result.
 */
let FrameSnapshotActor = protocol.ActorClass({
  typeName: "frame-snapshot",

  /**
   * Creates the frame snapshot call actor.
   *
   * @param DebuggerServerConnection conn
   *        The server connection.
   * @param HTMLCanvasElement canvas
   *        A reference to the content canvas.
   * @param array calls
   *        An array of "function-call" actor instances.
   * @param object screenshot
   *        A single "snapshot-image" type instance.
   */
  initialize: function(conn, { canvas, calls, screenshot }) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this._contentCanvas = canvas;
    this._functionCalls = calls;
    this._lastDrawCallScreenshot = screenshot;
  },

  /**
   * Gets as much data about this snapshot without computing anything costly.
   */
  getOverview: method(function() {
    return {
      calls: this._functionCalls,
      thumbnails: this._functionCalls.map(e => e._thumbnail).filter(e => !!e),
      screenshot: this._lastDrawCallScreenshot
    };
  }, {
    response: { overview: RetVal("snapshot-overview") }
  }),

  /**
   * Gets a screenshot of the canvas's contents after the specified
   * function was called.
   */
  generateScreenshotFor: method(function(functionCall) {
    let caller = functionCall.details.caller;
    let global = functionCall.meta.global;

    let canvas = this._contentCanvas;
    let calls = this._functionCalls;
    let index = calls.indexOf(functionCall);

    // To get a screenshot, replay all the steps necessary to render the frame,
    // by invoking the context calls up to and including the specified one.
    // This will be done in a custom framebuffer in case of a WebGL context.
    let { replayContext, lastDrawCallIndex } = ContextUtils.replayAnimationFrame({
      contextType: global,
      canvas: canvas,
      calls: calls,
      first: 0,
      last: index
    });

    // To keep things fast, generate an image that's relatively small.
    let dimensions = Math.min(CanvasFront.SCREENSHOT_HEIGHT_MAX, canvas.height);
    let screenshot;

    // Depending on the canvas' context, generating a screenshot is done
    // in different ways. In case of the WebGL context, we also need to reset
    // the framebuffer binding to the default value.
    if (global == CallWatcherFront.CANVAS_WEBGL_CONTEXT) {
      screenshot = ContextUtils.getPixelsForWebGL(replayContext);
      replayContext.bindFramebuffer(replayContext.FRAMEBUFFER, null);
      screenshot.flipped = true;
    }
    // In case of 2D contexts, no additional special treatment is necessary.
    else if (global == CallWatcherFront.CANVAS_2D_CONTEXT) {
      screenshot = ContextUtils.getPixelsFor2D(replayContext);
      screenshot.flipped = false;
    }

    screenshot.index = lastDrawCallIndex;
    return screenshot;
  }, {
    request: { call: Arg(0, "function-call") },
    response: { screenshot: RetVal("snapshot-image") }
  })
});

/**
 * The corresponding Front object for the FrameSnapshotActor.
 */
let FrameSnapshotFront = protocol.FrontClass(FrameSnapshotActor, {
  initialize: function(client, form) {
    protocol.Front.prototype.initialize.call(this, client, form);
    this._lastDrawCallScreenshot = null;
    this._cachedScreenshots = new WeakMap();
  },

  /**
   * This implementation caches the last draw call screenshot to optimize
   * frontend requests to `generateScreenshotFor`.
   */
  getOverview: custom(function() {
    return this._getOverview().then(data => {
      this._lastDrawCallScreenshot = data.screenshot;
      return data;
    });
  }, {
    impl: "_getOverview"
  }),

  /**
   * This implementation saves a roundtrip to the backend if the screenshot
   * was already generated and retrieved once.
   */
  generateScreenshotFor: custom(function(functionCall) {
    if (CanvasFront.ANIMATION_GENERATORS.has(functionCall.name)) {
      return promise.resolve(this._lastDrawCallScreenshot);
    }
    let cachedScreenshot = this._cachedScreenshots.get(functionCall);
    if (cachedScreenshot) {
      return cachedScreenshot;
    }
    let screenshot = this._generateScreenshotFor(functionCall);
    this._cachedScreenshots.set(functionCall, screenshot);
    return screenshot;
  }, {
    impl: "_generateScreenshotFor"
  })
});

/**
 * This Canvas Actor handles simple instrumentation of all the methods
 * of a 2D or WebGL context, to provide information regarding all the calls
 * made when drawing frame inside an animation loop.
 */
let CanvasActor = exports.CanvasActor = protocol.ActorClass({
  typeName: "canvas",
  initialize: function(conn, tabActor) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this.tabActor = tabActor;
    this._onContentFunctionCall = this._onContentFunctionCall.bind(this);
  },
  destroy: function(conn) {
    protocol.Actor.prototype.destroy.call(this, conn);
    this.finalize();
  },

  /**
   * Starts listening for function calls.
   */
  setup: method(function({ reload }) {
    if (this._initialized) {
      return;
    }
    this._initialized = true;

    this._callWatcher = new CallWatcherActor(this.conn, this.tabActor);
    this._callWatcher.onCall = this._onContentFunctionCall;
    this._callWatcher.setup({
      tracedGlobals: CANVAS_CONTEXTS,
      tracedFunctions: ANIMATION_GENERATORS,
      performReload: reload
    });
  }, {
    request: { reload: Option(0, "boolean") },
    oneway: true
  }),

  /**
   * Stops listening for function calls.
   */
  finalize: method(function() {
    if (!this._initialized) {
      return;
    }
    this._initialized = false;

    this._callWatcher.finalize();
    this._callWatcher = null;
  }, {
    oneway: true
  }),

  /**
   * Returns whether this actor has been set up.
   */
  isInitialized: method(function() {
    return !!this._initialized;
  }, {
    response: { initialized: RetVal("boolean") }
  }),

  /**
   * Records a snapshot of all the calls made during the next animation frame.
   * The animation should be implemented via the de-facto requestAnimationFrame
   * utility, not inside a `setInterval` or recursive `setTimeout`.
   *
   * XXX: Currently only supporting requestAnimationFrame. When this isn't used,
   * it'd be a good idea to display a huge red flashing banner telling people to
   * STOP USING `setInterval` OR `setTimeout` FOR ANIMATION. Bug 978948.
   */
  recordAnimationFrame: method(function() {
    if (this._callWatcher.isRecording()) {
      return this._currentAnimationFrameSnapshot.promise;
    }

    this._callWatcher.eraseRecording();
    this._callWatcher.resumeRecording();

    let deferred = this._currentAnimationFrameSnapshot = promise.defer();
    return deferred.promise;
  }, {
    response: { snapshot: RetVal("frame-snapshot") }
  }),

  /**
   * Invoked whenever an instrumented function is called, be it on a
   * 2d or WebGL context, or an animation generator like requestAnimationFrame.
   */
  _onContentFunctionCall: function(functionCall) {
    let { window, name, args } = functionCall.details;

    // The function call arguments are required to replay animation frames,
    // in order to generate screenshots. However, simply storing references to
    // every kind of object is a bad idea, since their properties may change.
    // Consider transformation matrices for example, which are typically
    // Float32Arrays whose values can easily change across context calls.
    // They need to be cloned.
    inplaceShallowCloneArrays(args, window);

    if (CanvasFront.ANIMATION_GENERATORS.has(name)) {
      this._handleAnimationFrame(functionCall);
      return;
    }
    if (CanvasFront.DRAW_CALLS.has(name) && this._animationStarted) {
      this._handleDrawCall(functionCall);
      return;
    }
  },

  /**
   * Handle animations generated using requestAnimationFrame.
   */
  _handleAnimationFrame: function(functionCall) {
    if (!this._animationStarted) {
      this._handleAnimationFrameBegin();
    } else {
      this._handleAnimationFrameEnd(functionCall);
    }
  },

  /**
   * Called whenever an animation frame rendering begins.
   */
  _handleAnimationFrameBegin: function() {
    this._callWatcher.eraseRecording();
    this._animationStarted = true;
  },

  /**
   * Called whenever an animation frame rendering ends.
   */
  _handleAnimationFrameEnd: function() {
    // Get a hold of all the function calls made during this animation frame.
    // Since only one snapshot can be recorded at a time, erase all the
    // previously recorded calls.
    let functionCalls = this._callWatcher.pauseRecording();
    this._callWatcher.eraseRecording();

    // Since the animation frame finished, get a hold of the (already retrieved)
    // canvas pixels to conveniently create a screenshot of the final rendering.
    let index = this._lastDrawCallIndex;
    let width = this._lastContentCanvasWidth;
    let height = this._lastContentCanvasHeight;
    let flipped = this._lastThumbnailFlipped;
    let pixels = ContextUtils.getPixelStorage()["32bit"];
    let lastDrawCallScreenshot = {
      index: index,
      width: width,
      height: height,
      flipped: flipped,
      pixels: pixels.subarray(0, width * height)
    };

    // Wrap the function calls and screenshot in a FrameSnapshotActor instance,
    // which will resolve the promise returned by `recordAnimationFrame`.
    let frameSnapshot = new FrameSnapshotActor(this.conn, {
      canvas: this._lastDrawCallCanvas,
      calls: functionCalls,
      screenshot: lastDrawCallScreenshot
    });

    this._currentAnimationFrameSnapshot.resolve(frameSnapshot);
    this._currentAnimationFrameSnapshot = null;
    this._animationStarted = false;
  },

  /**
   * Invoked whenever a draw call is detected in the animation frame which is
   * currently being recorded.
   */
  _handleDrawCall: function(functionCall) {
    let functionCalls = this._callWatcher.pauseRecording();
    let caller = functionCall.details.caller;
    let global = functionCall.meta.global;

    let contentCanvas = this._lastDrawCallCanvas = caller.canvas;
    let index = this._lastDrawCallIndex = functionCalls.indexOf(functionCall);
    let w = this._lastContentCanvasWidth = contentCanvas.width;
    let h = this._lastContentCanvasHeight = contentCanvas.height;

    // To keep things fast, generate images of small and fixed dimensions.
    let dimensions = CanvasFront.THUMBNAIL_HEIGHT;
    let thumbnail;

    // Create a thumbnail on every draw call on the canvas context, to augment
    // the respective function call actor with this additional data.
    if (global == CallWatcherFront.CANVAS_WEBGL_CONTEXT) {
      // Check if drawing to a custom framebuffer (when rendering to texture).
      // Don't create a thumbnail in this particular case.
      let framebufferBinding = caller.getParameter(caller.FRAMEBUFFER_BINDING);
      if (framebufferBinding == null) {
        thumbnail = ContextUtils.getPixelsForWebGL(caller, 0, 0, w, h, dimensions);
        thumbnail.flipped = this._lastThumbnailFlipped = true;
        thumbnail.index = index;
      }
    } else if (global == CallWatcherFront.CANVAS_2D_CONTEXT) {
      thumbnail = ContextUtils.getPixelsFor2D(caller, 0, 0, w, h, dimensions);
      thumbnail.flipped = this._lastThumbnailFlipped = false;
      thumbnail.index = index;
    }

    functionCall._thumbnail = thumbnail;
    this._callWatcher.resumeRecording();
  }
});

/**
 * A collection of methods for manipulating canvas contexts.
 */
let ContextUtils = {
  /**
   * WebGL contexts are sensitive to how they're queried. Use this function
   * to make sure the right context is always retrieved, if available.
   *
   * @param HTMLCanvasElement canvas
   *        The canvas element for which to get a WebGL context.
   * @param WebGLRenderingContext gl
   *        The queried WebGL context, or null if unavailable.
   */
  getWebGLContext: function(canvas) {
    return canvas.getContext("webgl") ||
           canvas.getContext("experimental-webgl");
  },

  /**
   * Gets a hold of the rendered pixels in the most efficient way possible for
   * a canvas with a WebGL context.
   *
   * @param WebGLRenderingContext gl
   *        The WebGL context to get a screenshot from.
   * @param number srcX [optional]
   *        The first left pixel that is read from the framebuffer.
   * @param number srcY [optional]
   *        The first top pixel that is read from the framebuffer.
   * @param number srcWidth [optional]
   *        The number of pixels to read on the X axis.
   * @param number srcHeight [optional]
   *        The number of pixels to read on the Y axis.
   * @param number dstHeight [optional]
   *        The desired generated screenshot height.
   * @return object
   *         An objet containing the screenshot's width, height and pixel data.
   */
  getPixelsForWebGL: function(gl,
    srcX = 0, srcY = 0,
    srcWidth = gl.canvas.width,
    srcHeight = gl.canvas.height,
    dstHeight = srcHeight)
  {
    let contentPixels = ContextUtils.getPixelStorage(srcWidth, srcHeight);
    let { "8bit": charView, "32bit": intView } = contentPixels;
    gl.readPixels(srcX, srcY, srcWidth, srcHeight, gl.RGBA, gl.UNSIGNED_BYTE, charView);
    return this.resizePixels(intView, srcWidth, srcHeight, dstHeight);
  },

  /**
   * Gets a hold of the rendered pixels in the most efficient way possible for
   * a canvas with a 2D context.
   *
   * @param CanvasRenderingContext2D ctx
   *        The 2D context to get a screenshot from.
   * @param number srcX [optional]
   *        The first left pixel that is read from the canvas.
   * @param number srcY [optional]
   *        The first top pixel that is read from the canvas.
   * @param number srcWidth [optional]
   *        The number of pixels to read on the X axis.
   * @param number srcHeight [optional]
   *        The number of pixels to read on the Y axis.
   * @param number dstHeight [optional]
   *        The desired generated screenshot height.
   * @return object
   *         An objet containing the screenshot's width, height and pixel data.
   */
  getPixelsFor2D: function(ctx,
    srcX = 0, srcY = 0,
    srcWidth = ctx.canvas.width,
    srcHeight = ctx.canvas.height,
    dstHeight = srcHeight)
  {
    let { data } = ctx.getImageData(srcX, srcY, srcWidth, srcHeight);
    let { "32bit": intView } = ContextUtils.usePixelStorage(data.buffer);
    return this.resizePixels(intView, srcWidth, srcHeight, dstHeight);
  },

  /**
   * Resizes the provided pixels to fit inside a rectangle with the specified
   * height and the same aspect ratio as the source.
   *
   * @param Uint32Array srcPixels
   *        The source pixel data, assuming 32bit/pixel and 4 color components.
   * @param number srcWidth
   *        The source pixel data width.
   * @param number srcHeight
   *        The source pixel data height.
   * @param number dstHeight [optional]
   *        The desired resized pixel data height.
   * @return object
   *         An objet containing the resized pixels width, height and data.
   */
  resizePixels: function(srcPixels, srcWidth, srcHeight, dstHeight) {
    let screenshotRatio = dstHeight / srcHeight;
    let dstWidth = Math.floor(srcWidth * screenshotRatio);

    // Use a plain array instead of a Uint32Array to make serializing faster.
    let dstPixels = new Array(dstWidth * dstHeight);

    // If the resized image ends up being completely transparent, returning
    // an empty array will skip some redundant serialization cycles.
    let isTransparent = true;

    for (let dstX = 0; dstX < dstWidth; dstX++) {
      for (let dstY = 0; dstY < dstHeight; dstY++) {
        let srcX = Math.floor(dstX / screenshotRatio);
        let srcY = Math.floor(dstY / screenshotRatio);
        let cPos = srcX + srcWidth * srcY;
        let dPos = dstX + dstWidth * dstY;
        let color = dstPixels[dPos] = srcPixels[cPos];
        if (color) {
          isTransparent = false;
        }
      }
    }

    return {
      width: dstWidth,
      height: dstHeight,
      pixels: isTransparent ? [] : dstPixels
    };
  },

  /**
   * Invokes a series of canvas context calls, to "replay" an animation frame
   * and generate a screenshot.
   *
   * In case of a WebGL context, an offscreen framebuffer is created for
   * the respective canvas, and the rendering will be performed into it.
   * This is necessary because some state (like shaders, textures etc.) can't
   * be shared between two different WebGL contexts.
   * Hopefully, once SharedResources are a thing this won't be necessary:
   * http://www.khronos.org/webgl/wiki/SharedResouces
   *
   * In case of a 2D context, a new canvas is created, since there's no
   * intrinsic state that can't be easily duplicated.
   *
   * @param number contexType
   *        The type of context to use. See the CallWatcherFront scope types.
   * @param HTMLCanvasElement canvas
   *        The canvas element which is the source of all context calls.
   * @param array calls
   *        An array of function call actors.
   * @param number first
   *        The first function call to start from.
   * @param number last
   *        The last (inclusive) function call to end at.
   * @return object
   *         The context on which the specified calls were invoked and the
   *         last registered draw call's index.
   */
  replayAnimationFrame: function({ contextType, canvas, calls, first, last }) {
    let w = canvas.width;
    let h = canvas.height;

    let replayCanvas;
    let replayContext;
    let customFramebuffer;
    let lastDrawCallIndex = -1;

    // In case of WebGL contexts, rendering will be done offscreen, in a
    // custom framebuffer, but on the provided canvas context.
    if (contextType == CallWatcherFront.CANVAS_WEBGL_CONTEXT) {
      replayCanvas = canvas;
      replayContext = this.getWebGLContext(replayCanvas);
      customFramebuffer = this.createBoundFramebuffer(replayContext, w, h);
    }
    // In case of 2D contexts, draw everything on a separate canvas context.
    else if (contextType == CallWatcherFront.CANVAS_2D_CONTEXT) {
      let contentDocument = canvas.ownerDocument;
      replayCanvas = contentDocument.createElement("canvas");
      replayCanvas.width = w;
      replayCanvas.height = h;
      replayContext = replayCanvas.getContext("2d");
      replayContext.clearRect(0, 0, w, h);
    }

    // Replay all the context calls up to and including the specified one.
    for (let i = first; i <= last; i++) {
      let { type, name, args } = calls[i].details;

      // Prevent WebGL context calls that try to reset the framebuffer binding
      // to the default value, since we want to perform the rendering offscreen.
      if (name == "bindFramebuffer" && args[1] == null) {
        replayContext.bindFramebuffer(replayContext.FRAMEBUFFER, customFramebuffer);
      } else {
        if (type == CallWatcherFront.METHOD_FUNCTION) {
          replayContext[name].apply(replayContext, args);
        } else if (type == CallWatcherFront.SETTER_FUNCTION) {
          replayContext[name] = args;
        } else {
          // Ignore getter calls.
        }
        if (CanvasFront.DRAW_CALLS.has(name)) {
          lastDrawCallIndex = i;
        }
      }
    }

    return {
      replayContext: replayContext,
      lastDrawCallIndex: lastDrawCallIndex
    };
  },

  /**
   * Gets an object containing a buffer large enough to hold width * height
   * pixels, assuming 32bit/pixel and 4 color components.
   *
   * This method avoids allocating memory and tries to reuse a common buffer
   * as much as possible.
   *
   * @param number w
   *        The desired pixel array storage width.
   * @param number h
   *        The desired pixel array storage height.
   * @return object
   *         The requested pixel array buffer.
   */
  getPixelStorage: function(w = 0, h = 0) {
    let storage = this._currentPixelStorage;
    if (storage && storage["32bit"].length >= w * h) {
      return storage;
    }
    return this.usePixelStorage(new ArrayBuffer(w * h * 4));
  },

  /**
   * Creates and saves the array buffer views used by `getPixelStorage`.
   *
   * @param ArrayBuffer buffer
   *        The raw buffer used as storage for various array buffer views.
   */
  usePixelStorage: function(buffer) {
    let array8bit = new Uint8Array(buffer);
    let array32bit = new Uint32Array(buffer);
    return this._currentPixelStorage = {
      "8bit": array8bit,
      "32bit": array32bit
    };
  },

  /**
   * Creates a framebuffer of the specified dimensions for a WebGL context,
   * assuming a RGBA color buffer, a depth buffer and no stencil buffer.
   *
   * @param WebGLRenderingContext gl
   *        The WebGL context to create and bind a framebuffer for.
   * @param number width
   *        The desired width of the renderbuffers.
   * @param number height
   *        The desired height of the renderbuffers.
   * @return WebGLFramebuffer
   *         The generated framebuffer object.
   */
  createBoundFramebuffer: function(gl, width, height) {
    let framebuffer = gl.createFramebuffer();
    gl.bindFramebuffer(gl.FRAMEBUFFER, framebuffer);

    // Use a texture as the color rendebuffer attachment, since consumenrs of
    // this function will most likely want to read the rendered pixels back.
    let colorBuffer = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, colorBuffer);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
    gl.generateMipmap(gl.TEXTURE_2D);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, width, height, 0, gl.RGBA, gl.UNSIGNED_BYTE, null);

    let depthBuffer = gl.createRenderbuffer();
    gl.bindRenderbuffer(gl.RENDERBUFFER, depthBuffer);
    gl.renderbufferStorage(gl.RENDERBUFFER, gl.DEPTH_COMPONENT16, width, height);

    gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, colorBuffer, 0);
    gl.framebufferRenderbuffer(gl.FRAMEBUFFER, gl.DEPTH_ATTACHMENT, gl.RENDERBUFFER, depthBuffer);

    gl.bindTexture(gl.TEXTURE_2D, null);
    gl.bindRenderbuffer(gl.RENDERBUFFER, null);

    return framebuffer;
  }
};

/**
 * The corresponding Front object for the CanvasActor.
 */
let CanvasFront = exports.CanvasFront = protocol.FrontClass(CanvasActor, {
  initialize: function(client, { canvasActor }) {
    protocol.Front.prototype.initialize.call(this, client, { actor: canvasActor });
    client.addActorPool(this);
    this.manage(this);
  }
});

/**
 * Constants.
 */
CanvasFront.CANVAS_CONTEXTS = new Set(CANVAS_CONTEXTS);
CanvasFront.ANIMATION_GENERATORS = new Set(ANIMATION_GENERATORS);
CanvasFront.DRAW_CALLS = new Set(DRAW_CALLS);
CanvasFront.INTERESTING_CALLS = new Set(INTERESTING_CALLS);
CanvasFront.THUMBNAIL_HEIGHT = 50; // px
CanvasFront.SCREENSHOT_HEIGHT_MAX = 256; // px
CanvasFront.INVALID_SNAPSHOT_IMAGE = {
  index: -1,
  width: 0,
  height: 0,
  pixels: []
};

/**
 * Goes through all the arguments and creates a one-level shallow copy
 * of all arrays and array buffers.
 */
function inplaceShallowCloneArrays(functionArguments, contentWindow) {
  let { Object, Array, ArrayBuffer } = contentWindow;

  functionArguments.forEach((arg, index, store) => {
    if (arg instanceof Array) {
      store[index] = arg.slice();
    }
    if (arg instanceof Object && arg.buffer instanceof ArrayBuffer) {
      store[index] = new arg.constructor(arg);
    }
  });
}
