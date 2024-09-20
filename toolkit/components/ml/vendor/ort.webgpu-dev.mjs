/*!
 * ONNX Runtime Web v1.20.0-dev.20240827-1d059b8702
 * Copyright (c) Microsoft Corporation. All rights reserved.
 * Licensed under the MIT License.
 */
var __defProp = Object.defineProperty;
var __getOwnPropDesc = Object.getOwnPropertyDescriptor;
var __getOwnPropNames = Object.getOwnPropertyNames;
var __hasOwnProp = Object.prototype.hasOwnProperty;
var __require = /* @__PURE__ */ ((x) => typeof require !== "undefined" ? require : typeof Proxy !== "undefined" ? new Proxy(x, {
  get: (a, b) => (typeof require !== "undefined" ? require : a)[b]
}) : x)(function(x) {
  if (typeof require !== "undefined")
    return require.apply(this, arguments);
  throw Error('Dynamic require of "' + x + '" is not supported');
});
var __esm = (fn, res) => function __init() {
  return fn && (res = (0, fn[__getOwnPropNames(fn)[0]])(fn = 0)), res;
};
var __export = (target, all) => {
  for (var name in all)
    __defProp(target, name, { get: all[name], enumerable: true });
};
var __copyProps = (to, from, except, desc) => {
  if (from && typeof from === "object" || typeof from === "function") {
    for (let key of __getOwnPropNames(from))
      if (!__hasOwnProp.call(to, key) && key !== except)
        __defProp(to, key, { get: () => from[key], enumerable: !(desc = __getOwnPropDesc(from, key)) || desc.enumerable });
  }
  return to;
};
var __toCommonJS = (mod) => __copyProps(__defProp({}, "__esModule", { value: true }), mod);

// common/dist/esm/backend-impl.js
var backends, backendsSortedByPriority, registerBackend, tryResolveAndInitializeBackend, resolveBackendAndExecutionProviders;
var init_backend_impl = __esm({
  "common/dist/esm/backend-impl.js"() {
    "use strict";
    backends = /* @__PURE__ */ new Map();
    backendsSortedByPriority = [];
    registerBackend = (name, backend, priority) => {
      if (backend && typeof backend.init === "function" && typeof backend.createInferenceSessionHandler === "function") {
        const currentBackend = backends.get(name);
        if (currentBackend === void 0) {
          backends.set(name, { backend, priority });
        } else if (currentBackend.priority > priority) {
          return;
        } else if (currentBackend.priority === priority) {
          if (currentBackend.backend !== backend) {
            throw new Error(`cannot register backend "${name}" using priority ${priority}`);
          }
        }
        if (priority >= 0) {
          const i = backendsSortedByPriority.indexOf(name);
          if (i !== -1) {
            backendsSortedByPriority.splice(i, 1);
          }
          for (let i2 = 0; i2 < backendsSortedByPriority.length; i2++) {
            if (backends.get(backendsSortedByPriority[i2]).priority <= priority) {
              backendsSortedByPriority.splice(i2, 0, name);
              return;
            }
          }
          backendsSortedByPriority.push(name);
        }
        return;
      }
      throw new TypeError("not a valid backend");
    };
    tryResolveAndInitializeBackend = async (backendName) => {
      const backendInfo = backends.get(backendName);
      if (!backendInfo) {
        return "backend not found.";
      }
      if (backendInfo.initialized) {
        return backendInfo.backend;
      } else if (backendInfo.aborted) {
        return backendInfo.error;
      } else {
        const isInitializing = !!backendInfo.initPromise;
        try {
          if (!isInitializing) {
            backendInfo.initPromise = backendInfo.backend.init(backendName);
          }
          await backendInfo.initPromise;
          backendInfo.initialized = true;
          return backendInfo.backend;
        } catch (e) {
          if (!isInitializing) {
            backendInfo.error = `${e}`;
            backendInfo.aborted = true;
          }
          return backendInfo.error;
        } finally {
          delete backendInfo.initPromise;
        }
      }
    };
    resolveBackendAndExecutionProviders = async (options) => {
      const eps = options.executionProviders || [];
      const backendHints = eps.map((i) => typeof i === "string" ? i : i.name);
      const backendNames = backendHints.length === 0 ? backendsSortedByPriority : backendHints;
      let backend;
      const errors = [];
      const availableBackendNames = /* @__PURE__ */ new Set();
      for (const backendName of backendNames) {
        const resolveResult = await tryResolveAndInitializeBackend(backendName);
        if (typeof resolveResult === "string") {
          errors.push({ name: backendName, err: resolveResult });
        } else {
          if (!backend) {
            backend = resolveResult;
          }
          if (backend === resolveResult) {
            availableBackendNames.add(backendName);
          }
        }
      }
      if (!backend) {
        throw new Error(`no available backend found. ERR: ${errors.map((e) => `[${e.name}] ${e.err}`).join(", ")}`);
      }
      for (const { name, err } of errors) {
        if (backendHints.includes(name)) {
          console.warn(`removing requested execution provider "${name}" from session options because it is not available: ${err}`);
        }
      }
      const filteredEps = eps.filter((i) => availableBackendNames.has(typeof i === "string" ? i : i.name));
      return [
        backend,
        new Proxy(options, {
          get: (target, prop) => {
            if (prop === "executionProviders") {
              return filteredEps;
            }
            return Reflect.get(target, prop);
          }
        })
      ];
    };
  }
});

// common/dist/esm/backend.js
var init_backend = __esm({
  "common/dist/esm/backend.js"() {
    "use strict";
    init_backend_impl();
  }
});

// common/dist/esm/version.js
var version;
var init_version = __esm({
  "common/dist/esm/version.js"() {
    "use strict";
    version = "1.20.0-dev.20240827-5d54dc1462";
  }
});

// common/dist/esm/env-impl.js
var logLevelValue, env;
var init_env_impl = __esm({
  "common/dist/esm/env-impl.js"() {
    "use strict";
    init_version();
    logLevelValue = "warning";
    env = {
      wasm: {},
      webgl: {},
      webgpu: {},
      versions: { common: version },
      set logLevel(value) {
        if (value === void 0) {
          return;
        }
        if (typeof value !== "string" || ["verbose", "info", "warning", "error", "fatal"].indexOf(value) === -1) {
          throw new Error(`Unsupported logging level: ${value}`);
        }
        logLevelValue = value;
      },
      get logLevel() {
        return logLevelValue;
      }
    };
    Object.defineProperty(env, "logLevel", { enumerable: true });
  }
});

// common/dist/esm/env.js
var env2;
var init_env = __esm({
  "common/dist/esm/env.js"() {
    "use strict";
    init_env_impl();
    env2 = env;
  }
});

// common/dist/esm/tensor-conversion-impl.js
var tensorToDataURL, tensorToImageData;
var init_tensor_conversion_impl = __esm({
  "common/dist/esm/tensor-conversion-impl.js"() {
    "use strict";
    tensorToDataURL = (tensor, options) => {
      const canvas = typeof document !== "undefined" ? document.createElement("canvas") : new OffscreenCanvas(1, 1);
      canvas.width = tensor.dims[3];
      canvas.height = tensor.dims[2];
      const pixels2DContext = canvas.getContext("2d");
      if (pixels2DContext != null) {
        let width;
        let height;
        if (options?.tensorLayout !== void 0 && options.tensorLayout === "NHWC") {
          width = tensor.dims[2];
          height = tensor.dims[3];
        } else {
          width = tensor.dims[3];
          height = tensor.dims[2];
        }
        const inputformat = options?.format !== void 0 ? options.format : "RGB";
        const norm = options?.norm;
        let normMean;
        let normBias;
        if (norm === void 0 || norm.mean === void 0) {
          normMean = [255, 255, 255, 255];
        } else {
          if (typeof norm.mean === "number") {
            normMean = [norm.mean, norm.mean, norm.mean, norm.mean];
          } else {
            normMean = [norm.mean[0], norm.mean[1], norm.mean[2], 0];
            if (norm.mean[3] !== void 0) {
              normMean[3] = norm.mean[3];
            }
          }
        }
        if (norm === void 0 || norm.bias === void 0) {
          normBias = [0, 0, 0, 0];
        } else {
          if (typeof norm.bias === "number") {
            normBias = [norm.bias, norm.bias, norm.bias, norm.bias];
          } else {
            normBias = [norm.bias[0], norm.bias[1], norm.bias[2], 0];
            if (norm.bias[3] !== void 0) {
              normBias[3] = norm.bias[3];
            }
          }
        }
        const stride = height * width;
        let rTensorPointer = 0, gTensorPointer = stride, bTensorPointer = stride * 2, aTensorPointer = -1;
        if (inputformat === "RGBA") {
          rTensorPointer = 0;
          gTensorPointer = stride;
          bTensorPointer = stride * 2;
          aTensorPointer = stride * 3;
        } else if (inputformat === "RGB") {
          rTensorPointer = 0;
          gTensorPointer = stride;
          bTensorPointer = stride * 2;
        } else if (inputformat === "RBG") {
          rTensorPointer = 0;
          bTensorPointer = stride;
          gTensorPointer = stride * 2;
        }
        for (let i = 0; i < height; i++) {
          for (let j = 0; j < width; j++) {
            const R = (tensor.data[rTensorPointer++] - normBias[0]) * normMean[0];
            const G = (tensor.data[gTensorPointer++] - normBias[1]) * normMean[1];
            const B = (tensor.data[bTensorPointer++] - normBias[2]) * normMean[2];
            const A = aTensorPointer === -1 ? 255 : (tensor.data[aTensorPointer++] - normBias[3]) * normMean[3];
            pixels2DContext.fillStyle = "rgba(" + R + "," + G + "," + B + "," + A + ")";
            pixels2DContext.fillRect(j, i, 1, 1);
          }
        }
        if ("toDataURL" in canvas) {
          return canvas.toDataURL();
        } else {
          throw new Error("toDataURL is not supported");
        }
      } else {
        throw new Error("Can not access image data");
      }
    };
    tensorToImageData = (tensor, options) => {
      const pixels2DContext = typeof document !== "undefined" ? document.createElement("canvas").getContext("2d") : new OffscreenCanvas(1, 1).getContext("2d");
      let image;
      if (pixels2DContext != null) {
        let width;
        let height;
        let channels;
        if (options?.tensorLayout !== void 0 && options.tensorLayout === "NHWC") {
          width = tensor.dims[2];
          height = tensor.dims[1];
          channels = tensor.dims[3];
        } else {
          width = tensor.dims[3];
          height = tensor.dims[2];
          channels = tensor.dims[1];
        }
        const inputformat = options !== void 0 ? options.format !== void 0 ? options.format : "RGB" : "RGB";
        const norm = options?.norm;
        let normMean;
        let normBias;
        if (norm === void 0 || norm.mean === void 0) {
          normMean = [255, 255, 255, 255];
        } else {
          if (typeof norm.mean === "number") {
            normMean = [norm.mean, norm.mean, norm.mean, norm.mean];
          } else {
            normMean = [norm.mean[0], norm.mean[1], norm.mean[2], 255];
            if (norm.mean[3] !== void 0) {
              normMean[3] = norm.mean[3];
            }
          }
        }
        if (norm === void 0 || norm.bias === void 0) {
          normBias = [0, 0, 0, 0];
        } else {
          if (typeof norm.bias === "number") {
            normBias = [norm.bias, norm.bias, norm.bias, norm.bias];
          } else {
            normBias = [norm.bias[0], norm.bias[1], norm.bias[2], 0];
            if (norm.bias[3] !== void 0) {
              normBias[3] = norm.bias[3];
            }
          }
        }
        const stride = height * width;
        if (options !== void 0) {
          if (options.format !== void 0 && channels === 4 && options.format !== "RGBA" || channels === 3 && options.format !== "RGB" && options.format !== "BGR") {
            throw new Error("Tensor format doesn't match input tensor dims");
          }
        }
        const step = 4;
        let rImagePointer = 0, gImagePointer = 1, bImagePointer = 2, aImagePointer = 3;
        let rTensorPointer = 0, gTensorPointer = stride, bTensorPointer = stride * 2, aTensorPointer = -1;
        if (inputformat === "RGBA") {
          rTensorPointer = 0;
          gTensorPointer = stride;
          bTensorPointer = stride * 2;
          aTensorPointer = stride * 3;
        } else if (inputformat === "RGB") {
          rTensorPointer = 0;
          gTensorPointer = stride;
          bTensorPointer = stride * 2;
        } else if (inputformat === "RBG") {
          rTensorPointer = 0;
          bTensorPointer = stride;
          gTensorPointer = stride * 2;
        }
        image = pixels2DContext.createImageData(width, height);
        for (let i = 0; i < height * width; rImagePointer += step, gImagePointer += step, bImagePointer += step, aImagePointer += step, i++) {
          image.data[rImagePointer] = (tensor.data[rTensorPointer++] - normBias[0]) * normMean[0];
          image.data[gImagePointer] = (tensor.data[gTensorPointer++] - normBias[1]) * normMean[1];
          image.data[bImagePointer] = (tensor.data[bTensorPointer++] - normBias[2]) * normMean[2];
          image.data[aImagePointer] = aTensorPointer === -1 ? 255 : (tensor.data[aTensorPointer++] - normBias[3]) * normMean[3];
        }
      } else {
        throw new Error("Can not access image data");
      }
      return image;
    };
  }
});

// common/dist/esm/tensor-factory-impl.js
var bufferToTensor, tensorFromImage, tensorFromTexture, tensorFromGpuBuffer, tensorFromPinnedBuffer;
var init_tensor_factory_impl = __esm({
  "common/dist/esm/tensor-factory-impl.js"() {
    "use strict";
    init_tensor_impl();
    bufferToTensor = (buffer, options) => {
      if (buffer === void 0) {
        throw new Error("Image buffer must be defined");
      }
      if (options.height === void 0 || options.width === void 0) {
        throw new Error("Image height and width must be defined");
      }
      if (options.tensorLayout === "NHWC") {
        throw new Error("NHWC Tensor layout is not supported yet");
      }
      const { height, width } = options;
      const norm = options.norm ?? { mean: 255, bias: 0 };
      let normMean;
      let normBias;
      if (typeof norm.mean === "number") {
        normMean = [norm.mean, norm.mean, norm.mean, norm.mean];
      } else {
        normMean = [norm.mean[0], norm.mean[1], norm.mean[2], norm.mean[3] ?? 255];
      }
      if (typeof norm.bias === "number") {
        normBias = [norm.bias, norm.bias, norm.bias, norm.bias];
      } else {
        normBias = [norm.bias[0], norm.bias[1], norm.bias[2], norm.bias[3] ?? 0];
      }
      const inputformat = options.format !== void 0 ? options.format : "RGBA";
      const outputformat = options.tensorFormat !== void 0 ? options.tensorFormat !== void 0 ? options.tensorFormat : "RGB" : "RGB";
      const stride = height * width;
      const float32Data = outputformat === "RGBA" ? new Float32Array(stride * 4) : new Float32Array(stride * 3);
      let step = 4, rImagePointer = 0, gImagePointer = 1, bImagePointer = 2, aImagePointer = 3;
      let rTensorPointer = 0, gTensorPointer = stride, bTensorPointer = stride * 2, aTensorPointer = -1;
      if (inputformat === "RGB") {
        step = 3;
        rImagePointer = 0;
        gImagePointer = 1;
        bImagePointer = 2;
        aImagePointer = -1;
      }
      if (outputformat === "RGBA") {
        aTensorPointer = stride * 3;
      } else if (outputformat === "RBG") {
        rTensorPointer = 0;
        bTensorPointer = stride;
        gTensorPointer = stride * 2;
      } else if (outputformat === "BGR") {
        bTensorPointer = 0;
        gTensorPointer = stride;
        rTensorPointer = stride * 2;
      }
      for (let i = 0; i < stride; i++, rImagePointer += step, bImagePointer += step, gImagePointer += step, aImagePointer += step) {
        float32Data[rTensorPointer++] = (buffer[rImagePointer] + normBias[0]) / normMean[0];
        float32Data[gTensorPointer++] = (buffer[gImagePointer] + normBias[1]) / normMean[1];
        float32Data[bTensorPointer++] = (buffer[bImagePointer] + normBias[2]) / normMean[2];
        if (aTensorPointer !== -1 && aImagePointer !== -1) {
          float32Data[aTensorPointer++] = (buffer[aImagePointer] + normBias[3]) / normMean[3];
        }
      }
      const outputTensor = outputformat === "RGBA" ? new Tensor("float32", float32Data, [1, 4, height, width]) : new Tensor("float32", float32Data, [1, 3, height, width]);
      return outputTensor;
    };
    tensorFromImage = async (image, options) => {
      const isHTMLImageEle = typeof HTMLImageElement !== "undefined" && image instanceof HTMLImageElement;
      const isImageDataEle = typeof ImageData !== "undefined" && image instanceof ImageData;
      const isImageBitmap = typeof ImageBitmap !== "undefined" && image instanceof ImageBitmap;
      const isString = typeof image === "string";
      let data;
      let bufferToTensorOptions = options ?? {};
      const createCanvas = () => {
        if (typeof document !== "undefined") {
          return document.createElement("canvas");
        } else if (typeof OffscreenCanvas !== "undefined") {
          return new OffscreenCanvas(1, 1);
        } else {
          throw new Error("Canvas is not supported");
        }
      };
      const createCanvasContext = (canvas) => {
        if (canvas instanceof HTMLCanvasElement) {
          return canvas.getContext("2d");
        } else if (canvas instanceof OffscreenCanvas) {
          return canvas.getContext("2d");
        } else {
          return null;
        }
      };
      if (isHTMLImageEle) {
        const canvas = createCanvas();
        canvas.width = image.width;
        canvas.height = image.height;
        const pixels2DContext = createCanvasContext(canvas);
        if (pixels2DContext != null) {
          let height = image.height;
          let width = image.width;
          if (options !== void 0 && options.resizedHeight !== void 0 && options.resizedWidth !== void 0) {
            height = options.resizedHeight;
            width = options.resizedWidth;
          }
          if (options !== void 0) {
            bufferToTensorOptions = options;
            if (options.tensorFormat !== void 0) {
              throw new Error("Image input config format must be RGBA for HTMLImageElement");
            } else {
              bufferToTensorOptions.tensorFormat = "RGBA";
            }
            bufferToTensorOptions.height = height;
            bufferToTensorOptions.width = width;
          } else {
            bufferToTensorOptions.tensorFormat = "RGBA";
            bufferToTensorOptions.height = height;
            bufferToTensorOptions.width = width;
          }
          pixels2DContext.drawImage(image, 0, 0);
          data = pixels2DContext.getImageData(0, 0, width, height).data;
        } else {
          throw new Error("Can not access image data");
        }
      } else if (isImageDataEle) {
        let height;
        let width;
        if (options !== void 0 && options.resizedWidth !== void 0 && options.resizedHeight !== void 0) {
          height = options.resizedHeight;
          width = options.resizedWidth;
        } else {
          height = image.height;
          width = image.width;
        }
        if (options !== void 0) {
          bufferToTensorOptions = options;
        }
        bufferToTensorOptions.format = "RGBA";
        bufferToTensorOptions.height = height;
        bufferToTensorOptions.width = width;
        if (options !== void 0) {
          const tempCanvas = createCanvas();
          tempCanvas.width = width;
          tempCanvas.height = height;
          const pixels2DContext = createCanvasContext(tempCanvas);
          if (pixels2DContext != null) {
            pixels2DContext.putImageData(image, 0, 0);
            data = pixels2DContext.getImageData(0, 0, width, height).data;
          } else {
            throw new Error("Can not access image data");
          }
        } else {
          data = image.data;
        }
      } else if (isImageBitmap) {
        if (options === void 0) {
          throw new Error("Please provide image config with format for Imagebitmap");
        }
        const canvas = createCanvas();
        canvas.width = image.width;
        canvas.height = image.height;
        const pixels2DContext = createCanvasContext(canvas);
        if (pixels2DContext != null) {
          const height = image.height;
          const width = image.width;
          pixels2DContext.drawImage(image, 0, 0, width, height);
          data = pixels2DContext.getImageData(0, 0, width, height).data;
          bufferToTensorOptions.height = height;
          bufferToTensorOptions.width = width;
          return bufferToTensor(data, bufferToTensorOptions);
        } else {
          throw new Error("Can not access image data");
        }
      } else if (isString) {
        return new Promise((resolve, reject) => {
          const canvas = createCanvas();
          const context = createCanvasContext(canvas);
          if (!image || !context) {
            return reject();
          }
          const newImage = new Image();
          newImage.crossOrigin = "Anonymous";
          newImage.src = image;
          newImage.onload = () => {
            canvas.width = newImage.width;
            canvas.height = newImage.height;
            context.drawImage(newImage, 0, 0, canvas.width, canvas.height);
            const img = context.getImageData(0, 0, canvas.width, canvas.height);
            bufferToTensorOptions.height = canvas.height;
            bufferToTensorOptions.width = canvas.width;
            resolve(bufferToTensor(img.data, bufferToTensorOptions));
          };
        });
      } else {
        throw new Error("Input data provided is not supported - aborted tensor creation");
      }
      if (data !== void 0) {
        return bufferToTensor(data, bufferToTensorOptions);
      } else {
        throw new Error("Input data provided is not supported - aborted tensor creation");
      }
    };
    tensorFromTexture = (texture, options) => {
      const { width, height, download, dispose } = options;
      const dims = [1, height, width, 4];
      return new Tensor({ location: "texture", type: "float32", texture, dims, download, dispose });
    };
    tensorFromGpuBuffer = (gpuBuffer, options) => {
      const { dataType, dims, download, dispose } = options;
      return new Tensor({ location: "gpu-buffer", type: dataType ?? "float32", gpuBuffer, dims, download, dispose });
    };
    tensorFromPinnedBuffer = (type, buffer, dims) => new Tensor({ location: "cpu-pinned", type, data: buffer, dims: dims ?? [buffer.length] });
  }
});

// common/dist/esm/tensor-impl-type-mapping.js
var NUMERIC_TENSOR_TYPE_TO_TYPEDARRAY_MAP, NUMERIC_TENSOR_TYPEDARRAY_TO_TYPE_MAP, isTypedArrayChecked, checkTypedArray;
var init_tensor_impl_type_mapping = __esm({
  "common/dist/esm/tensor-impl-type-mapping.js"() {
    "use strict";
    NUMERIC_TENSOR_TYPE_TO_TYPEDARRAY_MAP = /* @__PURE__ */ new Map([
      ["float32", Float32Array],
      ["uint8", Uint8Array],
      ["int8", Int8Array],
      ["uint16", Uint16Array],
      ["int16", Int16Array],
      ["int32", Int32Array],
      ["bool", Uint8Array],
      ["float64", Float64Array],
      ["uint32", Uint32Array],
      ["int4", Uint8Array],
      ["uint4", Uint8Array]
    ]);
    NUMERIC_TENSOR_TYPEDARRAY_TO_TYPE_MAP = /* @__PURE__ */ new Map([
      [Float32Array, "float32"],
      [Uint8Array, "uint8"],
      [Int8Array, "int8"],
      [Uint16Array, "uint16"],
      [Int16Array, "int16"],
      [Int32Array, "int32"],
      [Float64Array, "float64"],
      [Uint32Array, "uint32"]
    ]);
    isTypedArrayChecked = false;
    checkTypedArray = () => {
      if (!isTypedArrayChecked) {
        isTypedArrayChecked = true;
        const isBigInt64ArrayAvailable = typeof BigInt64Array !== "undefined" && BigInt64Array.from;
        const isBigUint64ArrayAvailable = typeof BigUint64Array !== "undefined" && BigUint64Array.from;
        const isFloat16ArrayAvailable = typeof Float16Array !== "undefined" && Float16Array.from;
        if (isBigInt64ArrayAvailable) {
          NUMERIC_TENSOR_TYPE_TO_TYPEDARRAY_MAP.set("int64", BigInt64Array);
          NUMERIC_TENSOR_TYPEDARRAY_TO_TYPE_MAP.set(BigInt64Array, "int64");
        }
        if (isBigUint64ArrayAvailable) {
          NUMERIC_TENSOR_TYPE_TO_TYPEDARRAY_MAP.set("uint64", BigUint64Array);
          NUMERIC_TENSOR_TYPEDARRAY_TO_TYPE_MAP.set(BigUint64Array, "uint64");
        }
        if (isFloat16ArrayAvailable) {
          NUMERIC_TENSOR_TYPE_TO_TYPEDARRAY_MAP.set("float16", Float16Array);
          NUMERIC_TENSOR_TYPEDARRAY_TO_TYPE_MAP.set(Float16Array, "float16");
        } else {
          NUMERIC_TENSOR_TYPE_TO_TYPEDARRAY_MAP.set("float16", Uint16Array);
        }
      }
    };
  }
});

// common/dist/esm/tensor-utils-impl.js
var calculateSize, tensorReshape;
var init_tensor_utils_impl = __esm({
  "common/dist/esm/tensor-utils-impl.js"() {
    "use strict";
    init_tensor_impl();
    calculateSize = (dims) => {
      let size = 1;
      for (let i = 0; i < dims.length; i++) {
        const dim = dims[i];
        if (typeof dim !== "number" || !Number.isSafeInteger(dim)) {
          throw new TypeError(`dims[${i}] must be an integer, got: ${dim}`);
        }
        if (dim < 0) {
          throw new RangeError(`dims[${i}] must be a non-negative integer, got: ${dim}`);
        }
        size *= dim;
      }
      return size;
    };
    tensorReshape = (tensor, dims) => {
      switch (tensor.location) {
        case "cpu":
          return new Tensor(tensor.type, tensor.data, dims);
        case "cpu-pinned":
          return new Tensor({
            location: "cpu-pinned",
            data: tensor.data,
            type: tensor.type,
            dims
          });
        case "texture":
          return new Tensor({
            location: "texture",
            texture: tensor.texture,
            type: tensor.type,
            dims
          });
        case "gpu-buffer":
          return new Tensor({
            location: "gpu-buffer",
            gpuBuffer: tensor.gpuBuffer,
            type: tensor.type,
            dims
          });
        default:
          throw new Error(`tensorReshape: tensor location ${tensor.location} is not supported`);
      }
    };
  }
});

// common/dist/esm/tensor-impl.js
var Tensor;
var init_tensor_impl = __esm({
  "common/dist/esm/tensor-impl.js"() {
    "use strict";
    init_tensor_conversion_impl();
    init_tensor_factory_impl();
    init_tensor_impl_type_mapping();
    init_tensor_utils_impl();
    Tensor = class {
      /**
       * implementation.
       */
      constructor(arg0, arg1, arg2) {
        checkTypedArray();
        let type;
        let dims;
        if (typeof arg0 === "object" && "location" in arg0) {
          this.dataLocation = arg0.location;
          type = arg0.type;
          dims = arg0.dims;
          switch (arg0.location) {
            case "cpu-pinned": {
              const expectedTypedArrayConstructor = NUMERIC_TENSOR_TYPE_TO_TYPEDARRAY_MAP.get(type);
              if (!expectedTypedArrayConstructor) {
                throw new TypeError(`unsupported type "${type}" to create tensor from pinned buffer`);
              }
              if (!(arg0.data instanceof expectedTypedArrayConstructor)) {
                throw new TypeError(`buffer should be of type ${expectedTypedArrayConstructor.name}`);
              }
              this.cpuData = arg0.data;
              break;
            }
            case "texture": {
              if (type !== "float32") {
                throw new TypeError(`unsupported type "${type}" to create tensor from texture`);
              }
              this.gpuTextureData = arg0.texture;
              this.downloader = arg0.download;
              this.disposer = arg0.dispose;
              break;
            }
            case "gpu-buffer": {
              if (type !== "float32" && type !== "float16" && type !== "int32" && type !== "int64" && type !== "uint32" && type !== "uint8" && type !== "bool" && type !== "uint4" && type !== "int4") {
                throw new TypeError(`unsupported type "${type}" to create tensor from gpu buffer`);
              }
              this.gpuBufferData = arg0.gpuBuffer;
              this.downloader = arg0.download;
              this.disposer = arg0.dispose;
              break;
            }
            default:
              throw new Error(`Tensor constructor: unsupported location '${this.dataLocation}'`);
          }
        } else {
          let data;
          let maybeDims;
          if (typeof arg0 === "string") {
            type = arg0;
            maybeDims = arg2;
            if (arg0 === "string") {
              if (!Array.isArray(arg1)) {
                throw new TypeError("A string tensor's data must be a string array.");
              }
              data = arg1;
            } else {
              const typedArrayConstructor = NUMERIC_TENSOR_TYPE_TO_TYPEDARRAY_MAP.get(arg0);
              if (typedArrayConstructor === void 0) {
                throw new TypeError(`Unsupported tensor type: ${arg0}.`);
              }
              if (Array.isArray(arg1)) {
                if (arg0 === "float16" && typedArrayConstructor === Uint16Array || arg0 === "uint4" || arg0 === "int4") {
                  throw new TypeError(`Creating a ${arg0} tensor from number array is not supported. Please use ${typedArrayConstructor.name} as data.`);
                } else if (arg0 === "uint64" || arg0 === "int64") {
                  data = typedArrayConstructor.from(arg1, BigInt);
                } else {
                  data = typedArrayConstructor.from(arg1);
                }
              } else if (arg1 instanceof typedArrayConstructor) {
                data = arg1;
              } else {
                throw new TypeError(`A ${type} tensor's data must be type of ${typedArrayConstructor}`);
              }
            }
          } else {
            maybeDims = arg1;
            if (Array.isArray(arg0)) {
              if (arg0.length === 0) {
                throw new TypeError("Tensor type cannot be inferred from an empty array.");
              }
              const firstElementType = typeof arg0[0];
              if (firstElementType === "string") {
                type = "string";
                data = arg0;
              } else if (firstElementType === "boolean") {
                type = "bool";
                data = Uint8Array.from(arg0);
              } else {
                throw new TypeError(`Invalid element type of data array: ${firstElementType}.`);
              }
            } else {
              const mappedType = NUMERIC_TENSOR_TYPEDARRAY_TO_TYPE_MAP.get(arg0.constructor);
              if (mappedType === void 0) {
                throw new TypeError(`Unsupported type for tensor data: ${arg0.constructor}.`);
              }
              type = mappedType;
              data = arg0;
            }
          }
          if (maybeDims === void 0) {
            maybeDims = [data.length];
          } else if (!Array.isArray(maybeDims)) {
            throw new TypeError("A tensor's dims must be a number array");
          }
          dims = maybeDims;
          this.cpuData = data;
          this.dataLocation = "cpu";
        }
        const size = calculateSize(dims);
        if (this.cpuData && size !== this.cpuData.length) {
          if ((type === "uint4" || type === "int4") && Math.ceil(size / 2) === this.cpuData.length) {
          } else {
            throw new Error(`Tensor's size(${size}) does not match data length(${this.cpuData.length}).`);
          }
        }
        this.type = type;
        this.dims = dims;
        this.size = size;
      }
      // #endregion
      // #region factory
      static async fromImage(image, options) {
        return tensorFromImage(image, options);
      }
      static fromTexture(texture, options) {
        return tensorFromTexture(texture, options);
      }
      static fromGpuBuffer(gpuBuffer, options) {
        return tensorFromGpuBuffer(gpuBuffer, options);
      }
      static fromPinnedBuffer(type, buffer, dims) {
        return tensorFromPinnedBuffer(type, buffer, dims);
      }
      // #endregion
      // #region conversions
      toDataURL(options) {
        return tensorToDataURL(this, options);
      }
      toImageData(options) {
        return tensorToImageData(this, options);
      }
      // #endregion
      // #region properties
      get data() {
        this.ensureValid();
        if (!this.cpuData) {
          throw new Error("The data is not on CPU. Use `getData()` to download GPU data to CPU, or use `texture` or `gpuBuffer` property to access the GPU data directly.");
        }
        return this.cpuData;
      }
      get location() {
        return this.dataLocation;
      }
      get texture() {
        this.ensureValid();
        if (!this.gpuTextureData) {
          throw new Error("The data is not stored as a WebGL texture.");
        }
        return this.gpuTextureData;
      }
      get gpuBuffer() {
        this.ensureValid();
        if (!this.gpuBufferData) {
          throw new Error("The data is not stored as a WebGPU buffer.");
        }
        return this.gpuBufferData;
      }
      // #endregion
      // #region methods
      async getData(releaseData) {
        this.ensureValid();
        switch (this.dataLocation) {
          case "cpu":
          case "cpu-pinned":
            return this.data;
          case "texture":
          case "gpu-buffer": {
            if (!this.downloader) {
              throw new Error("The current tensor is not created with a specified data downloader.");
            }
            if (this.isDownloading) {
              throw new Error("The current tensor is being downloaded.");
            }
            try {
              this.isDownloading = true;
              const data = await this.downloader();
              this.downloader = void 0;
              this.dataLocation = "cpu";
              this.cpuData = data;
              if (releaseData && this.disposer) {
                this.disposer();
                this.disposer = void 0;
              }
              return data;
            } finally {
              this.isDownloading = false;
            }
          }
          default:
            throw new Error(`cannot get data from location: ${this.dataLocation}`);
        }
      }
      dispose() {
        if (this.isDownloading) {
          throw new Error("The current tensor is being downloaded.");
        }
        if (this.disposer) {
          this.disposer();
          this.disposer = void 0;
        }
        this.cpuData = void 0;
        this.gpuTextureData = void 0;
        this.gpuBufferData = void 0;
        this.downloader = void 0;
        this.isDownloading = void 0;
        this.dataLocation = "none";
      }
      // #endregion
      // #region tensor utilities
      ensureValid() {
        if (this.dataLocation === "none") {
          throw new Error("The tensor is disposed.");
        }
      }
      reshape(dims) {
        this.ensureValid();
        if (this.downloader || this.disposer) {
          throw new Error("Cannot reshape a tensor that owns GPU resource.");
        }
        return tensorReshape(this, dims);
      }
    };
  }
});

// common/dist/esm/tensor.js
var Tensor2;
var init_tensor = __esm({
  "common/dist/esm/tensor.js"() {
    "use strict";
    init_tensor_impl();
    Tensor2 = Tensor;
  }
});

// common/dist/esm/trace.js
var TRACE, TRACE_FUNC, TRACE_FUNC_BEGIN, TRACE_FUNC_END;
var init_trace = __esm({
  "common/dist/esm/trace.js"() {
    "use strict";
    init_env_impl();
    TRACE = (deviceType, label) => {
      if (typeof env.trace === "undefined" ? !env.wasm.trace : !env.trace) {
        return;
      }
      console.timeStamp(`${deviceType}::ORT::${label}`);
    };
    TRACE_FUNC = (msg, extraMsg) => {
      const stack = new Error().stack?.split(/\r\n|\r|\n/g) || [];
      let hasTraceFunc = false;
      for (let i = 0; i < stack.length; i++) {
        if (hasTraceFunc && !stack[i].includes("TRACE_FUNC")) {
          let label = `FUNC_${msg}::${stack[i].trim().split(" ")[1]}`;
          if (extraMsg) {
            label += `::${extraMsg}`;
          }
          TRACE("CPU", label);
          return;
        }
        if (stack[i].includes("TRACE_FUNC")) {
          hasTraceFunc = true;
        }
      }
    };
    TRACE_FUNC_BEGIN = (extraMsg) => {
      if (typeof env.trace === "undefined" ? !env.wasm.trace : !env.trace) {
        return;
      }
      TRACE_FUNC("BEGIN", extraMsg);
    };
    TRACE_FUNC_END = (extraMsg) => {
      if (typeof env.trace === "undefined" ? !env.wasm.trace : !env.trace) {
        return;
      }
      TRACE_FUNC("END", extraMsg);
    };
  }
});

// common/dist/esm/inference-session-impl.js
var InferenceSession;
var init_inference_session_impl = __esm({
  "common/dist/esm/inference-session-impl.js"() {
    "use strict";
    init_backend_impl();
    init_tensor();
    init_trace();
    InferenceSession = class _InferenceSession {
      constructor(handler) {
        this.handler = handler;
      }
      async run(feeds, arg1, arg2) {
        TRACE_FUNC_BEGIN();
        const fetches = {};
        let options = {};
        if (typeof feeds !== "object" || feeds === null || feeds instanceof Tensor2 || Array.isArray(feeds)) {
          throw new TypeError("'feeds' must be an object that use input names as keys and OnnxValue as corresponding values.");
        }
        let isFetchesEmpty = true;
        if (typeof arg1 === "object") {
          if (arg1 === null) {
            throw new TypeError("Unexpected argument[1]: cannot be null.");
          }
          if (arg1 instanceof Tensor2) {
            throw new TypeError("'fetches' cannot be a Tensor");
          }
          if (Array.isArray(arg1)) {
            if (arg1.length === 0) {
              throw new TypeError("'fetches' cannot be an empty array.");
            }
            isFetchesEmpty = false;
            for (const name of arg1) {
              if (typeof name !== "string") {
                throw new TypeError("'fetches' must be a string array or an object.");
              }
              if (this.outputNames.indexOf(name) === -1) {
                throw new RangeError(`'fetches' contains invalid output name: ${name}.`);
              }
              fetches[name] = null;
            }
            if (typeof arg2 === "object" && arg2 !== null) {
              options = arg2;
            } else if (typeof arg2 !== "undefined") {
              throw new TypeError("'options' must be an object.");
            }
          } else {
            let isFetches = false;
            const arg1Keys = Object.getOwnPropertyNames(arg1);
            for (const name of this.outputNames) {
              if (arg1Keys.indexOf(name) !== -1) {
                const v = arg1[name];
                if (v === null || v instanceof Tensor2) {
                  isFetches = true;
                  isFetchesEmpty = false;
                  fetches[name] = v;
                }
              }
            }
            if (isFetches) {
              if (typeof arg2 === "object" && arg2 !== null) {
                options = arg2;
              } else if (typeof arg2 !== "undefined") {
                throw new TypeError("'options' must be an object.");
              }
            } else {
              options = arg1;
            }
          }
        } else if (typeof arg1 !== "undefined") {
          throw new TypeError("Unexpected argument[1]: must be 'fetches' or 'options'.");
        }
        for (const name of this.inputNames) {
          if (typeof feeds[name] === "undefined") {
            throw new Error(`input '${name}' is missing in 'feeds'.`);
          }
        }
        if (isFetchesEmpty) {
          for (const name of this.outputNames) {
            fetches[name] = null;
          }
        }
        const results = await this.handler.run(feeds, fetches, options);
        const returnValue = {};
        for (const key in results) {
          if (Object.hasOwnProperty.call(results, key)) {
            const result = results[key];
            if (result instanceof Tensor2) {
              returnValue[key] = result;
            } else {
              returnValue[key] = new Tensor2(result.type, result.data, result.dims);
            }
          }
        }
        TRACE_FUNC_END();
        return returnValue;
      }
      async release() {
        return this.handler.dispose();
      }
      static async create(arg0, arg1, arg2, arg3) {
        TRACE_FUNC_BEGIN();
        let filePathOrUint8Array;
        let options = {};
        if (typeof arg0 === "string") {
          filePathOrUint8Array = arg0;
          if (typeof arg1 === "object" && arg1 !== null) {
            options = arg1;
          } else if (typeof arg1 !== "undefined") {
            throw new TypeError("'options' must be an object.");
          }
        } else if (arg0 instanceof Uint8Array) {
          filePathOrUint8Array = arg0;
          if (typeof arg1 === "object" && arg1 !== null) {
            options = arg1;
          } else if (typeof arg1 !== "undefined") {
            throw new TypeError("'options' must be an object.");
          }
        } else if (arg0 instanceof ArrayBuffer || typeof SharedArrayBuffer !== "undefined" && arg0 instanceof SharedArrayBuffer) {
          const buffer = arg0;
          let byteOffset = 0;
          let byteLength = arg0.byteLength;
          if (typeof arg1 === "object" && arg1 !== null) {
            options = arg1;
          } else if (typeof arg1 === "number") {
            byteOffset = arg1;
            if (!Number.isSafeInteger(byteOffset)) {
              throw new RangeError("'byteOffset' must be an integer.");
            }
            if (byteOffset < 0 || byteOffset >= buffer.byteLength) {
              throw new RangeError(`'byteOffset' is out of range [0, ${buffer.byteLength}).`);
            }
            byteLength = arg0.byteLength - byteOffset;
            if (typeof arg2 === "number") {
              byteLength = arg2;
              if (!Number.isSafeInteger(byteLength)) {
                throw new RangeError("'byteLength' must be an integer.");
              }
              if (byteLength <= 0 || byteOffset + byteLength > buffer.byteLength) {
                throw new RangeError(`'byteLength' is out of range (0, ${buffer.byteLength - byteOffset}].`);
              }
              if (typeof arg3 === "object" && arg3 !== null) {
                options = arg3;
              } else if (typeof arg3 !== "undefined") {
                throw new TypeError("'options' must be an object.");
              }
            } else if (typeof arg2 !== "undefined") {
              throw new TypeError("'byteLength' must be a number.");
            }
          } else if (typeof arg1 !== "undefined") {
            throw new TypeError("'options' must be an object.");
          }
          filePathOrUint8Array = new Uint8Array(buffer, byteOffset, byteLength);
        } else {
          throw new TypeError("Unexpected argument[0]: must be 'path' or 'buffer'.");
        }
        const [backend, optionsWithValidatedEPs] = await resolveBackendAndExecutionProviders(options);
        const handler = await backend.createInferenceSessionHandler(filePathOrUint8Array, optionsWithValidatedEPs);
        TRACE_FUNC_END();
        return new _InferenceSession(handler);
      }
      startProfiling() {
        this.handler.startProfiling();
      }
      endProfiling() {
        this.handler.endProfiling();
      }
      get inputNames() {
        return this.handler.inputNames;
      }
      get outputNames() {
        return this.handler.outputNames;
      }
    };
  }
});

// common/dist/esm/inference-session.js
var InferenceSession2;
var init_inference_session = __esm({
  "common/dist/esm/inference-session.js"() {
    "use strict";
    init_inference_session_impl();
    InferenceSession2 = InferenceSession;
  }
});

// common/dist/esm/tensor-conversion.js
var init_tensor_conversion = __esm({
  "common/dist/esm/tensor-conversion.js"() {
    "use strict";
  }
});

// common/dist/esm/tensor-factory.js
var init_tensor_factory = __esm({
  "common/dist/esm/tensor-factory.js"() {
    "use strict";
  }
});

// common/dist/esm/onnx-model.js
var init_onnx_model = __esm({
  "common/dist/esm/onnx-model.js"() {
    "use strict";
  }
});

// common/dist/esm/onnx-value.js
var init_onnx_value = __esm({
  "common/dist/esm/onnx-value.js"() {
    "use strict";
  }
});

// common/dist/esm/training-session-impl.js
var noBackendErrMsg, TrainingSession;
var init_training_session_impl = __esm({
  "common/dist/esm/training-session-impl.js"() {
    "use strict";
    init_backend_impl();
    init_tensor();
    noBackendErrMsg = "Training backend could not be resolved. Make sure you're using the correct configuration & WebAssembly files.";
    TrainingSession = class _TrainingSession {
      constructor(handler, hasOptimizerModel, hasEvalModel) {
        this.handler = handler;
        this.hasOptimizerModel = hasOptimizerModel;
        this.hasEvalModel = hasEvalModel;
      }
      get trainingInputNames() {
        return this.handler.inputNames;
      }
      get trainingOutputNames() {
        return this.handler.outputNames;
      }
      get evalInputNames() {
        if (this.hasEvalModel) {
          return this.handler.evalInputNames;
        } else {
          throw new Error("This training session has no evalModel loaded.");
        }
      }
      get evalOutputNames() {
        if (this.hasEvalModel) {
          return this.handler.evalOutputNames;
        } else {
          throw new Error("This training session has no evalModel loaded.");
        }
      }
      static async create(trainingOptions, sessionOptions) {
        const evalModel = trainingOptions.evalModel || "";
        const optimizerModel = trainingOptions.optimizerModel || "";
        const options = sessionOptions || {};
        const [backend, optionsWithValidatedEPs] = await resolveBackendAndExecutionProviders(options);
        if (backend.createTrainingSessionHandler) {
          const handler = await backend.createTrainingSessionHandler(trainingOptions.checkpointState, trainingOptions.trainModel, evalModel, optimizerModel, optionsWithValidatedEPs);
          return new _TrainingSession(handler, !!trainingOptions.optimizerModel, !!trainingOptions.evalModel);
        } else {
          throw new Error(noBackendErrMsg);
        }
      }
      /**
       * Helper function for runTrainStep and future runStep methods that handles the type-narrowing conversion from
       * the given parameters to SessionHandler.FetchesType and RunOptions.
       *
       * @param inputNames the feeds object is checked that they contain all input names in the provided list of input
       * names.
       * @param outputNames the fetches object is checked that their keys match up with valid names in the list of output
       * names.
       * @param feeds the required input
       * @param arg1 narrowed & converted into the SessionHandler.FetchesType or RunOptions object
       * @param arg2 optional RunOptions object.
       * @returns
       */
      typeNarrowingForRunStep(inputNames, outputNames, feeds, arg1, arg2) {
        const fetches = {};
        let options = {};
        if (typeof feeds !== "object" || feeds === null || feeds instanceof Tensor2 || Array.isArray(feeds)) {
          throw new TypeError("'feeds' must be an object that use input names as keys and OnnxValue as corresponding values.");
        }
        let isFetchesEmpty = true;
        if (typeof arg1 === "object") {
          if (arg1 === null) {
            throw new TypeError("Unexpected argument[1]: cannot be null.");
          }
          if (arg1 instanceof Tensor2) {
            throw new TypeError("'fetches' cannot be a Tensor");
          }
          if (Array.isArray(arg1)) {
            if (arg1.length === 0) {
              throw new TypeError("'fetches' cannot be an empty array.");
            }
            isFetchesEmpty = false;
            for (const name of arg1) {
              if (typeof name !== "string") {
                throw new TypeError("'fetches' must be a string array or an object.");
              }
              if (outputNames.indexOf(name) === -1) {
                throw new RangeError(`'fetches' contains invalid output name: ${name}.`);
              }
              fetches[name] = null;
            }
            if (typeof arg2 === "object" && arg2 !== null) {
              options = arg2;
            } else if (typeof arg2 !== "undefined") {
              throw new TypeError("'options' must be an object.");
            }
          } else {
            let isFetches = false;
            const arg1Keys = Object.getOwnPropertyNames(arg1);
            for (const name of outputNames) {
              if (arg1Keys.indexOf(name) !== -1) {
                const v = arg1[name];
                if (v === null || v instanceof Tensor2) {
                  isFetches = true;
                  isFetchesEmpty = false;
                  fetches[name] = v;
                }
              }
            }
            if (isFetches) {
              if (typeof arg2 === "object" && arg2 !== null) {
                options = arg2;
              } else if (typeof arg2 !== "undefined") {
                throw new TypeError("'options' must be an object.");
              }
            } else {
              options = arg1;
            }
          }
        } else if (typeof arg1 !== "undefined") {
          throw new TypeError("Unexpected argument[1]: must be 'fetches' or 'options'.");
        }
        for (const name of inputNames) {
          if (typeof feeds[name] === "undefined") {
            throw new Error(`input '${name}' is missing in 'feeds'.`);
          }
        }
        if (isFetchesEmpty) {
          for (const name of outputNames) {
            fetches[name] = null;
          }
        }
        return [fetches, options];
      }
      /**
       * Helper method for runTrainStep and any other runStep methods. Takes the ReturnType result from the SessionHandler
       * and changes it into a map of Tensors.
       *
       * @param results
       * @returns
       */
      convertHandlerReturnTypeToMapOfTensors(results) {
        const returnValue = {};
        for (const key in results) {
          if (Object.hasOwnProperty.call(results, key)) {
            const result = results[key];
            if (result instanceof Tensor2) {
              returnValue[key] = result;
            } else {
              returnValue[key] = new Tensor2(result.type, result.data, result.dims);
            }
          }
        }
        return returnValue;
      }
      async lazyResetGrad() {
        await this.handler.lazyResetGrad();
      }
      async runTrainStep(feeds, arg1, arg2) {
        const [fetches, options] = this.typeNarrowingForRunStep(this.trainingInputNames, this.trainingOutputNames, feeds, arg1, arg2);
        const results = await this.handler.runTrainStep(feeds, fetches, options);
        return this.convertHandlerReturnTypeToMapOfTensors(results);
      }
      async runOptimizerStep(options) {
        if (this.hasOptimizerModel) {
          await this.handler.runOptimizerStep(options || {});
        } else {
          throw new Error("This TrainingSession has no OptimizerModel loaded.");
        }
      }
      async runEvalStep(feeds, arg1, arg2) {
        if (this.hasEvalModel) {
          const [fetches, options] = this.typeNarrowingForRunStep(this.evalInputNames, this.evalOutputNames, feeds, arg1, arg2);
          const results = await this.handler.runEvalStep(feeds, fetches, options);
          return this.convertHandlerReturnTypeToMapOfTensors(results);
        } else {
          throw new Error("This TrainingSession has no EvalModel loaded.");
        }
      }
      async getParametersSize(trainableOnly = true) {
        return this.handler.getParametersSize(trainableOnly);
      }
      async loadParametersBuffer(array, trainableOnly = true) {
        const paramsSize = await this.getParametersSize(trainableOnly);
        if (array.length !== 4 * paramsSize) {
          throw new Error("Size of the buffer passed into loadParametersBuffer must match the number of parameters in the model. Please use getParametersSize method to check.");
        }
        return this.handler.loadParametersBuffer(array, trainableOnly);
      }
      async getContiguousParameters(trainableOnly = true) {
        return this.handler.getContiguousParameters(trainableOnly);
      }
      async release() {
        return this.handler.dispose();
      }
    };
  }
});

// common/dist/esm/training-session.js
var TrainingSession2;
var init_training_session = __esm({
  "common/dist/esm/training-session.js"() {
    "use strict";
    init_training_session_impl();
    TrainingSession2 = TrainingSession;
  }
});

// common/dist/esm/index.js
var esm_exports = {};
__export(esm_exports, {
  InferenceSession: () => InferenceSession2,
  TRACE: () => TRACE,
  TRACE_FUNC_BEGIN: () => TRACE_FUNC_BEGIN,
  TRACE_FUNC_END: () => TRACE_FUNC_END,
  Tensor: () => Tensor2,
  TrainingSession: () => TrainingSession2,
  env: () => env2,
  registerBackend: () => registerBackend
});
var init_esm = __esm({
  "common/dist/esm/index.js"() {
    "use strict";
    init_backend();
    init_env();
    init_inference_session();
    init_tensor();
    init_tensor_conversion();
    init_tensor_factory();
    init_trace();
    init_onnx_model();
    init_onnx_value();
    init_training_session();
  }
});

// web/lib/wasm/wasm-utils-env.ts
var isNode;
var init_wasm_utils_env = __esm({
  "web/lib/wasm/wasm-utils-env.ts"() {
    "use strict";
    isNode = false;
  }
});

// web/lib/wasm/proxy-worker/main.ts
var main_exports = {};
__export(main_exports, {
  default: () => main_default
});
var WORKER_NAME, isProxyWorker, main_default;
var init_main = __esm({
  "web/lib/wasm/proxy-worker/main.ts"() {
    "use strict";
    init_wasm_core_impl();
    init_wasm_factory();
    init_wasm_utils_import();
    WORKER_NAME = "ort-wasm-proxy-worker";
    isProxyWorker = globalThis.self?.name === WORKER_NAME;
    if (isProxyWorker) {
      self.onmessage = (ev) => {
        const { type, in: message } = ev.data;
        try {
          switch (type) {
            case "init-wasm":
              initializeWebAssembly(message.wasm).then(
                () => {
                  initRuntime(message).then(
                    () => {
                      postMessage({ type });
                    },
                    (err) => {
                      postMessage({ type, err });
                    }
                  );
                },
                (err) => {
                  postMessage({ type, err });
                }
              );
              break;
            case "init-ep": {
              const { epName, env: env3 } = message;
              initEp(env3, epName).then(
                () => {
                  postMessage({ type });
                },
                (err) => {
                  postMessage({ type, err });
                }
              );
              break;
            }
            case "copy-from": {
              const { buffer } = message;
              const bufferData = copyFromExternalBuffer(buffer);
              postMessage({ type, out: bufferData });
              break;
            }
            case "create": {
              const { model, options } = message;
              createSession(model, options).then(
                (sessionMetadata) => {
                  postMessage({ type, out: sessionMetadata });
                },
                (err) => {
                  postMessage({ type, err });
                }
              );
              break;
            }
            case "release":
              releaseSession(message);
              postMessage({ type });
              break;
            case "run": {
              const { sessionId, inputIndices, inputs, outputIndices, options } = message;
              run(sessionId, inputIndices, inputs, outputIndices, new Array(outputIndices.length).fill(null), options).then(
                (outputs) => {
                  if (outputs.some((o) => o[3] !== "cpu")) {
                    postMessage({ type, err: "Proxy does not support non-cpu tensor location." });
                  } else {
                    postMessage(
                      { type, out: outputs },
                      extractTransferableBuffers([...inputs, ...outputs])
                    );
                  }
                },
                (err) => {
                  postMessage({ type, err });
                }
              );
              break;
            }
            case "end-profiling":
              endProfiling(message);
              postMessage({ type });
              break;
            default:
          }
        } catch (err) {
          postMessage({ type, err });
        }
      };
    }
    main_default = isProxyWorker ? null : (urlOverride) => new Worker(urlOverride ?? scriptSrc, { type: true ? "module" : "classic", name: WORKER_NAME });
  }
});

// web/lib/wasm/wasm-utils-import.ts
var scriptSrc, origin, isSameOrigin, normalizeUrl, fallbackUrl, preload, dynamicImportDefault, createProxyWorker, importProxyWorker, embeddedWasmModule, importWasmModule;
var init_wasm_utils_import = __esm({
  "web/lib/wasm/wasm-utils-import.ts"() {
    "use strict";
    init_wasm_utils_env();
    scriptSrc = // if Nodejs, return undefined
    isNode ? void 0 : (
      // if It's ESM, use import.meta.url
      import.meta.url ?? // use `document.currentScript.src` if available
      (typeof document !== "undefined" ? document.currentScript?.src : (
        // use `self.location.href` if available
        typeof self !== "undefined" ? self.location?.href : void 0
      ))
    );
    origin = isNode || typeof location === "undefined" ? void 0 : location.origin;
    isSameOrigin = (filename, prefixOverride) => {
      try {
        const baseUrl = prefixOverride ?? scriptSrc;
        const url = baseUrl ? new URL(filename, baseUrl) : new URL(filename);
        return url.origin === origin;
      } catch {
        return false;
      }
    };
    normalizeUrl = (filename, prefixOverride) => {
      const baseUrl = prefixOverride ?? scriptSrc;
      try {
        const url = baseUrl ? new URL(filename, baseUrl) : new URL(filename);
        return url.href;
      } catch {
        return void 0;
      }
    };
    fallbackUrl = (filename, prefixOverride) => `${prefixOverride ?? "./"}${filename}`;
    preload = async (absoluteUrl) => {
      const response = await fetch(absoluteUrl, { credentials: "same-origin" });
      const blob = await response.blob();
      return URL.createObjectURL(blob);
    };
    dynamicImportDefault = async (url) => (await import(
      /* webpackIgnore: true */
      url
    )).default;
    createProxyWorker = // eslint-disable-next-line @typescript-eslint/no-require-imports, @typescript-eslint/no-var-requires
    false ? void 0 : (init_main(), __toCommonJS(main_exports)).default;
    importProxyWorker = async () => {
      if (!scriptSrc) {
        throw new Error("Failed to load proxy worker: cannot determine the script source URL.");
      }
      if (isSameOrigin(scriptSrc)) {
        return [void 0, createProxyWorker()];
      }
      const url = await preload(scriptSrc);
      return [url, createProxyWorker(url)];
    };
    embeddedWasmModule = false ? (
      // eslint-disable-next-line @typescript-eslint/no-require-imports, @typescript-eslint/no-var-requires
      (false ? null : true ? null : null).default
    ) : void 0;
    importWasmModule = async (urlOverride, prefixOverride, isMultiThreaded) => {
      if (false) {
        return [void 0, embeddedWasmModule];
      } else {
        const wasmModuleFilename = false ? "ort-training-wasm-simd-threaded.mjs" : true ? "ort-wasm-simd-threaded.jsep.mjs" : "ort-wasm-simd-threaded.mjs";
        const wasmModuleUrl = urlOverride ?? normalizeUrl(wasmModuleFilename, prefixOverride);
        const needPreload = !isNode && isMultiThreaded && wasmModuleUrl && !isSameOrigin(wasmModuleUrl, prefixOverride);
        const url = needPreload ? await preload(wasmModuleUrl) : wasmModuleUrl ?? fallbackUrl(wasmModuleFilename, prefixOverride);
        return [needPreload ? url : void 0, await dynamicImportDefault(url)];
      }
    };
  }
});

// web/lib/wasm/wasm-factory.ts
var wasm, initialized, initializing, aborted, isMultiThreadSupported, isSimdSupported, initializeWebAssembly, getInstance;
var init_wasm_factory = __esm({
  "web/lib/wasm/wasm-factory.ts"() {
    "use strict";
    init_wasm_utils_import();
    initialized = false;
    initializing = false;
    aborted = false;
    isMultiThreadSupported = () => {
      if (typeof SharedArrayBuffer === "undefined") {
        return false;
      }
      try {
        if (typeof MessageChannel !== "undefined") {
          new MessageChannel().port1.postMessage(new SharedArrayBuffer(1));
        }
        return WebAssembly.validate(
          new Uint8Array([
            0,
            97,
            115,
            109,
            1,
            0,
            0,
            0,
            1,
            4,
            1,
            96,
            0,
            0,
            3,
            2,
            1,
            0,
            5,
            4,
            1,
            3,
            1,
            1,
            10,
            11,
            1,
            9,
            0,
            65,
            0,
            254,
            16,
            2,
            0,
            26,
            11
          ])
        );
      } catch (e) {
        return false;
      }
    };
    isSimdSupported = () => {
      try {
        return WebAssembly.validate(
          new Uint8Array([
            0,
            97,
            115,
            109,
            1,
            0,
            0,
            0,
            1,
            4,
            1,
            96,
            0,
            0,
            3,
            2,
            1,
            0,
            10,
            30,
            1,
            28,
            0,
            65,
            0,
            253,
            15,
            253,
            12,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            253,
            186,
            1,
            26,
            11
          ])
        );
      } catch (e) {
        return false;
      }
    };
    initializeWebAssembly = async (flags) => {
      if (initialized) {
        return Promise.resolve();
      }
      if (initializing) {
        throw new Error("multiple calls to 'initializeWebAssembly()' detected.");
      }
      if (aborted) {
        throw new Error("previous call to 'initializeWebAssembly()' failed.");
      }
      initializing = true;
      const timeout = flags.initTimeout;
      let numThreads = flags.numThreads;
      if (!isSimdSupported()) {
        throw new Error("WebAssembly SIMD is not supported in the current environment.");
      }
      const multiThreadSupported = isMultiThreadSupported();
      if (numThreads > 1 && !multiThreadSupported) {
        if (typeof self !== "undefined" && !self.crossOriginIsolated) {
          console.warn(
            "env.wasm.numThreads is set to " + numThreads + ", but this will not work unless you enable crossOriginIsolated mode. See https://web.dev/cross-origin-isolation-guide/ for more info."
          );
        }
        console.warn(
          "WebAssembly multi-threading is not supported in the current environment. Falling back to single-threading."
        );
        flags.numThreads = numThreads = 1;
      }
      const wasmPaths = flags.wasmPaths;
      const wasmPrefixOverride = typeof wasmPaths === "string" ? wasmPaths : void 0;
      const mjsPathOverrideFlag = wasmPaths?.mjs;
      const mjsPathOverride = mjsPathOverrideFlag?.href ?? mjsPathOverrideFlag;
      const wasmPathOverrideFlag = wasmPaths?.wasm;
      const wasmPathOverride = wasmPathOverrideFlag?.href ?? wasmPathOverrideFlag;
      const wasmBinaryOverride = flags.wasmBinary;
      const [objectUrl, ortWasmFactory] = await importWasmModule(mjsPathOverride, wasmPrefixOverride, numThreads > 1);
      let isTimeout = false;
      const tasks = [];
      if (timeout > 0) {
        tasks.push(
          new Promise((resolve) => {
            setTimeout(() => {
              isTimeout = true;
              resolve();
            }, timeout);
          })
        );
      }
      tasks.push(
        new Promise((resolve, reject) => {
          const config = {
            /**
             * The number of threads. WebAssembly will create (Module.numThreads - 1) workers. If it is 1, no worker will be
             * created.
             */
            numThreads
          };
          if (wasmBinaryOverride) {
            config.wasmBinary = wasmBinaryOverride;
          } else if (wasmPathOverride || wasmPrefixOverride) {
            config.locateFile = (fileName, scriptDirectory) => wasmPathOverride ?? (wasmPrefixOverride ?? scriptDirectory) + fileName;
          }
          ortWasmFactory(config).then(
            // wasm module initialized successfully
            (module) => {
              initializing = false;
              initialized = true;
              wasm = module;
              resolve();
              if (objectUrl) {
                URL.revokeObjectURL(objectUrl);
              }
            },
            // wasm module failed to initialize
            (what) => {
              initializing = false;
              aborted = true;
              reject(what);
            }
          );
        })
      );
      await Promise.race(tasks);
      if (isTimeout) {
        throw new Error(`WebAssembly backend initializing failed due to timeout: ${timeout}ms`);
      }
    };
    getInstance = () => {
      if (initialized && wasm) {
        return wasm;
      }
      throw new Error("WebAssembly is not initialized yet.");
    };
  }
});

// web/lib/wasm/wasm-utils.ts
var allocWasmString, iterateExtraOptions, checkLastError;
var init_wasm_utils = __esm({
  "web/lib/wasm/wasm-utils.ts"() {
    "use strict";
    init_wasm_factory();
    allocWasmString = (data, allocs) => {
      const wasm2 = getInstance();
      const dataLength = wasm2.lengthBytesUTF8(data) + 1;
      const dataOffset = wasm2._malloc(dataLength);
      wasm2.stringToUTF8(data, dataOffset, dataLength);
      allocs.push(dataOffset);
      return dataOffset;
    };
    iterateExtraOptions = (options, prefix, seen, handler) => {
      if (typeof options == "object" && options !== null) {
        if (seen.has(options)) {
          throw new Error("Circular reference in options");
        } else {
          seen.add(options);
        }
      }
      Object.entries(options).forEach(([key, value]) => {
        const name = prefix ? prefix + key : key;
        if (typeof value === "object") {
          iterateExtraOptions(value, name + ".", seen, handler);
        } else if (typeof value === "string" || typeof value === "number") {
          handler(name, value.toString());
        } else if (typeof value === "boolean") {
          handler(name, value ? "1" : "0");
        } else {
          throw new Error(`Can't handle extra config type: ${typeof value}`);
        }
      });
    };
    checkLastError = (message) => {
      const wasm2 = getInstance();
      const stack = wasm2.stackSave();
      try {
        const paramsOffset = wasm2.stackAlloc(8);
        wasm2._OrtGetLastError(paramsOffset, paramsOffset + 4);
        const errorCode = wasm2.HEAP32[paramsOffset / 4];
        const errorMessagePointer = wasm2.HEAPU32[paramsOffset / 4 + 1];
        const errorMessage = errorMessagePointer ? wasm2.UTF8ToString(errorMessagePointer) : "";
        throw new Error(`${message} ERROR_CODE: ${errorCode}, ERROR_MESSAGE: ${errorMessage}`);
      } finally {
        wasm2.stackRestore(stack);
      }
    };
  }
});

// web/lib/wasm/run-options.ts
var setRunOptions;
var init_run_options = __esm({
  "web/lib/wasm/run-options.ts"() {
    "use strict";
    init_wasm_factory();
    init_wasm_utils();
    setRunOptions = (options) => {
      const wasm2 = getInstance();
      let runOptionsHandle = 0;
      const allocs = [];
      const runOptions = options || {};
      try {
        if (options?.logSeverityLevel === void 0) {
          runOptions.logSeverityLevel = 2;
        } else if (typeof options.logSeverityLevel !== "number" || !Number.isInteger(options.logSeverityLevel) || options.logSeverityLevel < 0 || options.logSeverityLevel > 4) {
          throw new Error(`log serverity level is not valid: ${options.logSeverityLevel}`);
        }
        if (options?.logVerbosityLevel === void 0) {
          runOptions.logVerbosityLevel = 0;
        } else if (typeof options.logVerbosityLevel !== "number" || !Number.isInteger(options.logVerbosityLevel)) {
          throw new Error(`log verbosity level is not valid: ${options.logVerbosityLevel}`);
        }
        if (options?.terminate === void 0) {
          runOptions.terminate = false;
        }
        let tagDataOffset = 0;
        if (options?.tag !== void 0) {
          tagDataOffset = allocWasmString(options.tag, allocs);
        }
        runOptionsHandle = wasm2._OrtCreateRunOptions(
          runOptions.logSeverityLevel,
          runOptions.logVerbosityLevel,
          !!runOptions.terminate,
          tagDataOffset
        );
        if (runOptionsHandle === 0) {
          checkLastError("Can't create run options.");
        }
        if (options?.extra !== void 0) {
          iterateExtraOptions(options.extra, "", /* @__PURE__ */ new WeakSet(), (key, value) => {
            const keyDataOffset = allocWasmString(key, allocs);
            const valueDataOffset = allocWasmString(value, allocs);
            if (wasm2._OrtAddRunConfigEntry(runOptionsHandle, keyDataOffset, valueDataOffset) !== 0) {
              checkLastError(`Can't set a run config entry: ${key} - ${value}.`);
            }
          });
        }
        return [runOptionsHandle, allocs];
      } catch (e) {
        if (runOptionsHandle !== 0) {
          wasm2._OrtReleaseRunOptions(runOptionsHandle);
        }
        allocs.forEach((alloc) => wasm2._free(alloc));
        throw e;
      }
    };
  }
});

// web/lib/wasm/session-options.ts
var getGraphOptimzationLevel, getExecutionMode, appendDefaultOptions, setExecutionProviders, setSessionOptions;
var init_session_options = __esm({
  "web/lib/wasm/session-options.ts"() {
    "use strict";
    init_wasm_factory();
    init_wasm_utils();
    getGraphOptimzationLevel = (graphOptimizationLevel) => {
      switch (graphOptimizationLevel) {
        case "disabled":
          return 0;
        case "basic":
          return 1;
        case "extended":
          return 2;
        case "all":
          return 99;
        default:
          throw new Error(`unsupported graph optimization level: ${graphOptimizationLevel}`);
      }
    };
    getExecutionMode = (executionMode) => {
      switch (executionMode) {
        case "sequential":
          return 0;
        case "parallel":
          return 1;
        default:
          throw new Error(`unsupported execution mode: ${executionMode}`);
      }
    };
    appendDefaultOptions = (options) => {
      if (!options.extra) {
        options.extra = {};
      }
      if (!options.extra.session) {
        options.extra.session = {};
      }
      const session = options.extra.session;
      if (!session.use_ort_model_bytes_directly) {
        session.use_ort_model_bytes_directly = "1";
      }
      if (options.executionProviders && options.executionProviders.some((ep) => (typeof ep === "string" ? ep : ep.name) === "webgpu")) {
        options.enableMemPattern = false;
      }
    };
    setExecutionProviders = (sessionOptionsHandle, executionProviders, allocs) => {
      for (const ep of executionProviders) {
        let epName = typeof ep === "string" ? ep : ep.name;
        switch (epName) {
          case "webnn":
            epName = "WEBNN";
            if (typeof ep !== "string") {
              const webnnOptions = ep;
              const deviceType = webnnOptions?.deviceType;
              if (deviceType) {
                const keyDataOffset = allocWasmString("deviceType", allocs);
                const valueDataOffset = allocWasmString(deviceType, allocs);
                if (getInstance()._OrtAddSessionConfigEntry(sessionOptionsHandle, keyDataOffset, valueDataOffset) !== 0) {
                  checkLastError(`Can't set a session config entry: 'deviceType' - ${deviceType}.`);
                }
              }
            }
            break;
          case "webgpu":
            epName = "JS";
            if (typeof ep !== "string") {
              const webgpuOptions = ep;
              if (webgpuOptions?.preferredLayout) {
                if (webgpuOptions.preferredLayout !== "NCHW" && webgpuOptions.preferredLayout !== "NHWC") {
                  throw new Error(`preferredLayout must be either 'NCHW' or 'NHWC': ${webgpuOptions.preferredLayout}`);
                }
                const keyDataOffset = allocWasmString("preferredLayout", allocs);
                const valueDataOffset = allocWasmString(webgpuOptions.preferredLayout, allocs);
                if (getInstance()._OrtAddSessionConfigEntry(sessionOptionsHandle, keyDataOffset, valueDataOffset) !== 0) {
                  checkLastError(`Can't set a session config entry: 'preferredLayout' - ${webgpuOptions.preferredLayout}.`);
                }
              }
            }
            break;
          case "wasm":
          case "cpu":
            continue;
          default:
            throw new Error(`not supported execution provider: ${epName}`);
        }
        const epNameDataOffset = allocWasmString(epName, allocs);
        if (getInstance()._OrtAppendExecutionProvider(sessionOptionsHandle, epNameDataOffset) !== 0) {
          checkLastError(`Can't append execution provider: ${epName}.`);
        }
      }
    };
    setSessionOptions = (options) => {
      const wasm2 = getInstance();
      let sessionOptionsHandle = 0;
      const allocs = [];
      const sessionOptions = options || {};
      appendDefaultOptions(sessionOptions);
      try {
        const graphOptimizationLevel = getGraphOptimzationLevel(sessionOptions.graphOptimizationLevel ?? "all");
        const executionMode = getExecutionMode(sessionOptions.executionMode ?? "sequential");
        const logIdDataOffset = typeof sessionOptions.logId === "string" ? allocWasmString(sessionOptions.logId, allocs) : 0;
        const logSeverityLevel = sessionOptions.logSeverityLevel ?? 2;
        if (!Number.isInteger(logSeverityLevel) || logSeverityLevel < 0 || logSeverityLevel > 4) {
          throw new Error(`log serverity level is not valid: ${logSeverityLevel}`);
        }
        const logVerbosityLevel = sessionOptions.logVerbosityLevel ?? 0;
        if (!Number.isInteger(logVerbosityLevel) || logVerbosityLevel < 0 || logVerbosityLevel > 4) {
          throw new Error(`log verbosity level is not valid: ${logVerbosityLevel}`);
        }
        const optimizedModelFilePathOffset = typeof sessionOptions.optimizedModelFilePath === "string" ? allocWasmString(sessionOptions.optimizedModelFilePath, allocs) : 0;
        sessionOptionsHandle = wasm2._OrtCreateSessionOptions(
          graphOptimizationLevel,
          !!sessionOptions.enableCpuMemArena,
          !!sessionOptions.enableMemPattern,
          executionMode,
          !!sessionOptions.enableProfiling,
          0,
          logIdDataOffset,
          logSeverityLevel,
          logVerbosityLevel,
          optimizedModelFilePathOffset
        );
        if (sessionOptionsHandle === 0) {
          checkLastError("Can't create session options.");
        }
        if (sessionOptions.executionProviders) {
          setExecutionProviders(sessionOptionsHandle, sessionOptions.executionProviders, allocs);
        }
        if (sessionOptions.enableGraphCapture !== void 0) {
          if (typeof sessionOptions.enableGraphCapture !== "boolean") {
            throw new Error(`enableGraphCapture must be a boolean value: ${sessionOptions.enableGraphCapture}`);
          }
          const keyDataOffset = allocWasmString("enableGraphCapture", allocs);
          const valueDataOffset = allocWasmString(sessionOptions.enableGraphCapture.toString(), allocs);
          if (wasm2._OrtAddSessionConfigEntry(sessionOptionsHandle, keyDataOffset, valueDataOffset) !== 0) {
            checkLastError(
              `Can't set a session config entry: 'enableGraphCapture' - ${sessionOptions.enableGraphCapture}.`
            );
          }
        }
        if (sessionOptions.freeDimensionOverrides) {
          for (const [name, value] of Object.entries(sessionOptions.freeDimensionOverrides)) {
            if (typeof name !== "string") {
              throw new Error(`free dimension override name must be a string: ${name}`);
            }
            if (typeof value !== "number" || !Number.isInteger(value) || value < 0) {
              throw new Error(`free dimension override value must be a non-negative integer: ${value}`);
            }
            const nameOffset = allocWasmString(name, allocs);
            if (wasm2._OrtAddFreeDimensionOverride(sessionOptionsHandle, nameOffset, value) !== 0) {
              checkLastError(`Can't set a free dimension override: ${name} - ${value}.`);
            }
          }
        }
        if (sessionOptions.extra !== void 0) {
          iterateExtraOptions(sessionOptions.extra, "", /* @__PURE__ */ new WeakSet(), (key, value) => {
            const keyDataOffset = allocWasmString(key, allocs);
            const valueDataOffset = allocWasmString(value, allocs);
            if (wasm2._OrtAddSessionConfigEntry(sessionOptionsHandle, keyDataOffset, valueDataOffset) !== 0) {
              checkLastError(`Can't set a session config entry: ${key} - ${value}.`);
            }
          });
        }
        return [sessionOptionsHandle, allocs];
      } catch (e) {
        if (sessionOptionsHandle !== 0) {
          wasm2._OrtReleaseSessionOptions(sessionOptionsHandle);
        }
        allocs.forEach((alloc) => wasm2._free(alloc));
        throw e;
      }
    };
  }
});

// web/lib/wasm/wasm-common.ts
var tensorDataTypeStringToEnum, tensorDataTypeEnumToString, calculateTensorSizeInBytes, tensorTypeToTypedArrayConstructor, logLevelStringToEnum, isGpuBufferSupportedType, dataLocationStringToEnum;
var init_wasm_common = __esm({
  "web/lib/wasm/wasm-common.ts"() {
    "use strict";
    tensorDataTypeStringToEnum = (type) => {
      switch (type) {
        case "int8":
          return 3 /* int8 */;
        case "uint8":
          return 2 /* uint8 */;
        case "bool":
          return 9 /* bool */;
        case "int16":
          return 5 /* int16 */;
        case "uint16":
          return 4 /* uint16 */;
        case "int32":
          return 6 /* int32 */;
        case "uint32":
          return 12 /* uint32 */;
        case "float16":
          return 10 /* float16 */;
        case "float32":
          return 1 /* float */;
        case "float64":
          return 11 /* double */;
        case "string":
          return 8 /* string */;
        case "int64":
          return 7 /* int64 */;
        case "uint64":
          return 13 /* uint64 */;
        case "int4":
          return 22 /* int4 */;
        case "uint4":
          return 21 /* uint4 */;
        default:
          throw new Error(`unsupported data type: ${type}`);
      }
    };
    tensorDataTypeEnumToString = (typeProto) => {
      switch (typeProto) {
        case 3 /* int8 */:
          return "int8";
        case 2 /* uint8 */:
          return "uint8";
        case 9 /* bool */:
          return "bool";
        case 5 /* int16 */:
          return "int16";
        case 4 /* uint16 */:
          return "uint16";
        case 6 /* int32 */:
          return "int32";
        case 12 /* uint32 */:
          return "uint32";
        case 10 /* float16 */:
          return "float16";
        case 1 /* float */:
          return "float32";
        case 11 /* double */:
          return "float64";
        case 8 /* string */:
          return "string";
        case 7 /* int64 */:
          return "int64";
        case 13 /* uint64 */:
          return "uint64";
        case 22 /* int4 */:
          return "int4";
        case 21 /* uint4 */:
          return "uint4";
        default:
          throw new Error(`unsupported data type: ${typeProto}`);
      }
    };
    calculateTensorSizeInBytes = (dateType, dimsOrSize) => {
      const elementSize = [
        -1,
        // undefined = 0
        4,
        // float = 1
        1,
        // uint8 = 2
        1,
        // int8 = 3
        2,
        // uint16 = 4
        2,
        // int16 = 5
        4,
        // int32 = 6
        8,
        // int64 = 7
        -1,
        // string = 8
        1,
        // bool = 9
        2,
        // float16 = 10
        8,
        // double = 11
        4,
        // uint32 = 12
        8,
        // uint64 = 13
        -1,
        // complex64 = 14
        -1,
        // complex128 = 15
        -1,
        // bfloat16 = 16
        -1,
        // FLOAT8E4M3FN = 17
        -1,
        // FLOAT8E4M3FNUZ = 18
        -1,
        // FLOAT8E5M2 = 19
        -1,
        // FLOAT8E5M2FNUZ = 20
        0.5,
        // uint4 = 21
        0.5
        // int4 = 22
      ][dateType];
      const size = typeof dimsOrSize === "number" ? dimsOrSize : dimsOrSize.reduce((a, b) => a * b, 1);
      return elementSize > 0 ? Math.ceil(size * elementSize) : void 0;
    };
    tensorTypeToTypedArrayConstructor = (type) => {
      switch (type) {
        case "float16":
          return typeof Float16Array !== "undefined" && Float16Array.from ? Float16Array : Uint16Array;
        case "float32":
          return Float32Array;
        case "uint8":
          return Uint8Array;
        case "int8":
          return Int8Array;
        case "uint16":
          return Uint16Array;
        case "int16":
          return Int16Array;
        case "int32":
          return Int32Array;
        case "bool":
          return Uint8Array;
        case "float64":
          return Float64Array;
        case "uint32":
          return Uint32Array;
        case "int64":
          return BigInt64Array;
        case "uint64":
          return BigUint64Array;
        default:
          throw new Error(`unsupported type: ${type}`);
      }
    };
    logLevelStringToEnum = (logLevel) => {
      switch (logLevel) {
        case "verbose":
          return 0;
        case "info":
          return 1;
        case "warning":
          return 2;
        case "error":
          return 3;
        case "fatal":
          return 4;
        default:
          throw new Error(`unsupported logging level: ${logLevel}`);
      }
    };
    isGpuBufferSupportedType = (type) => type === "float32" || type === "float16" || type === "int32" || type === "int64" || type === "uint32" || type === "uint8" || type === "bool" || type === "uint4" || type === "int4";
    dataLocationStringToEnum = (location2) => {
      switch (location2) {
        case "none":
          return 0;
        case "cpu":
          return 1;
        case "cpu-pinned":
          return 2;
        case "texture":
          return 3;
        case "gpu-buffer":
          return 4;
        default:
          throw new Error(`unsupported data location: ${location2}`);
      }
    };
  }
});

// web/lib/wasm/wasm-utils-load-file.ts
var loadFile;
var init_wasm_utils_load_file = __esm({
  "web/lib/wasm/wasm-utils-load-file.ts"() {
    "use strict";
    init_wasm_utils_env();
    loadFile = async (file) => {
      if (typeof file === "string") {
        if (isNode) {
          try {
            const { readFile } = __require("node:fs/promises");
            return new Uint8Array(await readFile(file));
          } catch (e) {
            if (e.code === "ERR_FS_FILE_TOO_LARGE") {
              const { createReadStream } = __require("node:fs");
              const stream = createReadStream(file);
              const chunks = [];
              for await (const chunk of stream) {
                chunks.push(chunk);
              }
              return new Uint8Array(Buffer.concat(chunks));
            }
            throw e;
          }
        } else {
          const response = await fetch(file);
          if (!response.ok) {
            throw new Error(`failed to load external data file: ${file}`);
          }
          const contentLengthHeader = response.headers.get("Content-Length");
          const fileSize = contentLengthHeader ? parseInt(contentLengthHeader, 10) : 0;
          if (fileSize < 1073741824) {
            return new Uint8Array(await response.arrayBuffer());
          } else {
            if (!response.body) {
              throw new Error(`failed to load external data file: ${file}, no response body.`);
            }
            const reader = response.body.getReader();
            let buffer;
            try {
              buffer = new ArrayBuffer(fileSize);
            } catch (e) {
              if (e instanceof RangeError) {
                const pages = Math.ceil(fileSize / 65536);
                buffer = new WebAssembly.Memory({ initial: pages, maximum: pages }).buffer;
              } else {
                throw e;
              }
            }
            let offset = 0;
            while (true) {
              const { done, value } = await reader.read();
              if (done) {
                break;
              }
              const chunkSize = value.byteLength;
              const chunk = new Uint8Array(buffer, offset, chunkSize);
              chunk.set(value);
              offset += chunkSize;
            }
            return new Uint8Array(buffer, 0, fileSize);
          }
        }
      } else if (file instanceof Blob) {
        return new Uint8Array(await file.arrayBuffer());
      } else if (file instanceof Uint8Array) {
        return file;
      } else {
        return new Uint8Array(file);
      }
    };
  }
});

// web/lib/wasm/jsep/log.ts
var logLevelPrefix, doLog, configLogLevel, debug, configureLogger, LOG, LOG_DEBUG;
var init_log = __esm({
  "web/lib/wasm/jsep/log.ts"() {
    "use strict";
    init_wasm_common();
    logLevelPrefix = ["V", "I", "W", "E", "F"];
    doLog = (level, message) => {
      console.log(`[${logLevelPrefix[level]},${(/* @__PURE__ */ new Date()).toISOString()}]${message}`);
    };
    configureLogger = ($configLogLevel, $debug) => {
      configLogLevel = $configLogLevel;
      debug = $debug;
    };
    LOG = (logLevel, msg) => {
      const messageLevel = logLevelStringToEnum(logLevel);
      const configLevel = logLevelStringToEnum(configLogLevel);
      if (messageLevel >= configLevel) {
        doLog(messageLevel, typeof msg === "function" ? msg() : msg);
      }
    };
    LOG_DEBUG = (...args) => {
      if (debug) {
        LOG(...args);
      }
    };
  }
});

// web/lib/wasm/jsep/tensor-view.ts
var createView;
var init_tensor_view = __esm({
  "web/lib/wasm/jsep/tensor-view.ts"() {
    "use strict";
    init_wasm_common();
    createView = (dataBuffer, type) => new (tensorTypeToTypedArrayConstructor(type))(dataBuffer);
  }
});

// web/lib/wasm/jsep/webgpu/types.ts
var init_types = __esm({
  "web/lib/wasm/jsep/webgpu/types.ts"() {
    "use strict";
  }
});

// web/lib/wasm/jsep/webgpu/gpu-data-manager.ts
var bucketFreelist, bucketArr, calcNormalizedBufferSize, calcBucketBufferSize, guid, createNewGpuDataId, downloadGpuData, GpuDataManagerImpl, createGpuDataManager;
var init_gpu_data_manager = __esm({
  "web/lib/wasm/jsep/webgpu/gpu-data-manager.ts"() {
    "use strict";
    init_log();
    init_types();
    bucketFreelist = /* @__PURE__ */ new Map([
      [64, 250],
      [128, 200],
      [256, 200],
      [512, 200],
      [2048, 230],
      [4096, 200],
      [8192, 50],
      [16384, 50],
      [32768, 50],
      [65536, 50],
      [131072, 50],
      [262144, 50],
      [524288, 50],
      [1048576, 50],
      [2097152, 30],
      [4194304, 20],
      [8388608, 10],
      [12582912, 10],
      [16777216, 10],
      [26214400, 15],
      [33554432, 22],
      [44236800, 2],
      [58982400, 6],
      // we don't want to cache the bucket sizes below but not caching them
      // results in some major performance hits for models like sd-turbo.
      [67108864, 6],
      [134217728, 6],
      [167772160, 6]
    ]);
    bucketArr = [];
    calcNormalizedBufferSize = (size) => Math.ceil(size / 16) * 16;
    calcBucketBufferSize = (size) => {
      for (let idx = 0; idx < bucketArr.length; idx++) {
        const sizeForBucket = bucketArr[idx];
        if (size <= sizeForBucket) {
          return sizeForBucket;
        }
      }
      return Math.ceil(size / 16) * 16;
    };
    guid = 1;
    createNewGpuDataId = () => guid++;
    downloadGpuData = async (backend, gpuBuffer, originalSize, getTargetBuffer) => {
      const bufferSize = calcNormalizedBufferSize(originalSize);
      const gpuReadBuffer = backend.device.createBuffer(
        // eslint-disable-next-line no-bitwise
        { size: bufferSize, usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ }
      );
      try {
        const commandEncoder = backend.getCommandEncoder();
        backend.endComputePass();
        commandEncoder.copyBufferToBuffer(
          gpuBuffer,
          0,
          gpuReadBuffer,
          0,
          bufferSize
        );
        backend.flush();
        await gpuReadBuffer.mapAsync(GPUMapMode.READ);
        const arrayBuffer = gpuReadBuffer.getMappedRange();
        if (getTargetBuffer) {
          const targetBuffer = getTargetBuffer();
          targetBuffer.set(new Uint8Array(arrayBuffer, 0, originalSize));
          return targetBuffer;
        } else {
          return new Uint8Array(arrayBuffer.slice(0, originalSize));
        }
      } finally {
        gpuReadBuffer.destroy();
      }
    };
    GpuDataManagerImpl = class {
      constructor(backend) {
        this.backend = backend;
        this.storageCache = /* @__PURE__ */ new Map();
        this.freeBuffers = /* @__PURE__ */ new Map();
        this.freeUniformBuffers = /* @__PURE__ */ new Map();
        this.buffersForUploadingPending = [];
        this.buffersPending = [];
        this.externalBuffers = /* @__PURE__ */ new Map();
        this.capturedPendingBuffers = /* @__PURE__ */ new Map();
        for (const [key] of bucketFreelist) {
          bucketArr.push(key);
          this.freeBuffers.set(key, []);
          this.freeUniformBuffers.set(key, []);
        }
      }
      upload(id, data) {
        const srcArrayBuffer = data.buffer;
        const srcOffset = data.byteOffset;
        const srcLength = data.byteLength;
        const size = calcNormalizedBufferSize(srcLength);
        const gpuDataCache = this.storageCache.get(id);
        if (!gpuDataCache) {
          throw new Error("gpu data for uploading does not exist");
        }
        if (gpuDataCache.originalSize !== srcLength) {
          throw new Error(`inconsistent data size. gpu data size=${gpuDataCache.originalSize}, data size=${srcLength}`);
        }
        const gpuBufferForUploading = this.backend.device.createBuffer(
          // eslint-disable-next-line no-bitwise
          { mappedAtCreation: true, size, usage: GPUBufferUsage.MAP_WRITE | GPUBufferUsage.COPY_SRC }
        );
        const arrayBuffer = gpuBufferForUploading.getMappedRange();
        new Uint8Array(arrayBuffer).set(new Uint8Array(srcArrayBuffer, srcOffset, srcLength));
        gpuBufferForUploading.unmap();
        const commandEncoder = this.backend.getCommandEncoder();
        this.backend.endComputePass();
        commandEncoder.copyBufferToBuffer(gpuBufferForUploading, 0, gpuDataCache.gpuData.buffer, 0, size);
        LOG_DEBUG("verbose", () => `[WebGPU] GpuDataManager.upload(id=${id})`);
        this.buffersForUploadingPending.push(gpuBufferForUploading);
      }
      memcpy(sourceId, destinationId) {
        const sourceGpuDataCache = this.storageCache.get(sourceId);
        if (!sourceGpuDataCache) {
          throw new Error("source gpu data for memcpy does not exist");
        }
        const destinationGpuDataCache = this.storageCache.get(destinationId);
        if (!destinationGpuDataCache) {
          throw new Error("destination gpu data for memcpy does not exist");
        }
        if (sourceGpuDataCache.originalSize !== destinationGpuDataCache.originalSize) {
          throw new Error("inconsistent source and destination gpu data size");
        }
        const size = calcNormalizedBufferSize(sourceGpuDataCache.originalSize);
        const commandEncoder = this.backend.getCommandEncoder();
        this.backend.endComputePass();
        commandEncoder.copyBufferToBuffer(
          sourceGpuDataCache.gpuData.buffer,
          0,
          destinationGpuDataCache.gpuData.buffer,
          0,
          size
        );
      }
      registerExternalBuffer(buffer, originalSize, previousBuffer) {
        let id;
        if (previousBuffer) {
          id = this.externalBuffers.get(previousBuffer);
          if (id === void 0) {
            throw new Error("previous buffer is not registered");
          }
          if (buffer === previousBuffer) {
            LOG_DEBUG(
              "verbose",
              () => `[WebGPU] GpuDataManager.registerExternalBuffer(size=${originalSize}) => id=${id}, buffer is the same, skip.`
            );
            return id;
          } else if (this.backend.capturedCommandList.has(this.backend.currentSessionId)) {
            throw new Error(`Registering a different external buffer under graph capture mode is not supported yet.
             Please use the previous external buffer!`);
          }
          this.externalBuffers.delete(previousBuffer);
        } else {
          id = createNewGpuDataId();
        }
        this.storageCache.set(id, { gpuData: { id, type: 0 /* default */, buffer }, originalSize });
        this.externalBuffers.set(buffer, id);
        LOG_DEBUG(
          "verbose",
          () => `[WebGPU] GpuDataManager.registerExternalBuffer(size=${originalSize}) => id=${id}, registered.`
        );
        return id;
      }
      unregisterExternalBuffer(buffer) {
        const id = this.externalBuffers.get(buffer);
        if (id !== void 0) {
          this.storageCache.delete(id);
          this.externalBuffers.delete(buffer);
          LOG_DEBUG("verbose", () => `[WebGPU] GpuDataManager.unregisterExternalBuffer() => id=${id}`);
        }
      }
      // eslint-disable-next-line no-bitwise
      create(size, usage = GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_SRC | GPUBufferUsage.COPY_DST) {
        const bufferSize = calcBucketBufferSize(size);
        let gpuBuffer;
        const isStorage = (usage & GPUBufferUsage.STORAGE) === GPUBufferUsage.STORAGE;
        const isUniform = (usage & GPUBufferUsage.UNIFORM) === GPUBufferUsage.UNIFORM;
        if (isStorage || isUniform) {
          const freeBuffers = isStorage ? this.freeBuffers : this.freeUniformBuffers;
          const buffers = freeBuffers.get(bufferSize);
          if (!buffers) {
            gpuBuffer = this.backend.device.createBuffer({ size: bufferSize, usage });
          } else {
            if (buffers.length > 0) {
              gpuBuffer = buffers.pop();
            } else {
              gpuBuffer = this.backend.device.createBuffer({ size: bufferSize, usage });
            }
          }
        } else {
          gpuBuffer = this.backend.device.createBuffer({ size: bufferSize, usage });
        }
        const gpuData = { id: createNewGpuDataId(), type: 0 /* default */, buffer: gpuBuffer };
        this.storageCache.set(gpuData.id, { gpuData, originalSize: size });
        LOG_DEBUG("verbose", () => `[WebGPU] GpuDataManager.create(size=${size}) => id=${gpuData.id}`);
        return gpuData;
      }
      get(id) {
        return this.storageCache.get(id)?.gpuData;
      }
      release(id) {
        const cachedData = this.storageCache.get(id);
        if (!cachedData) {
          throw new Error("releasing data does not exist");
        }
        LOG_DEBUG("verbose", () => `[WebGPU] GpuDataManager.release(id=${id}), gpuDataId=${cachedData.gpuData.id}`);
        this.storageCache.delete(id);
        this.buffersPending.push(cachedData.gpuData.buffer);
        return cachedData.originalSize;
      }
      async download(id, getTargetBuffer) {
        const cachedData = this.storageCache.get(id);
        if (!cachedData) {
          throw new Error("data does not exist");
        }
        await downloadGpuData(this.backend, cachedData.gpuData.buffer, cachedData.originalSize, getTargetBuffer);
      }
      refreshPendingBuffers() {
        for (const buffer of this.buffersForUploadingPending) {
          buffer.destroy();
        }
        this.buffersForUploadingPending = [];
        if (this.buffersPending.length === 0) {
          return;
        }
        if (this.backend.sessionStatus === "default") {
          for (const buffer of this.buffersPending) {
            const maxInFreeList = bucketFreelist.get(buffer.size);
            if ((buffer.usage & GPUBufferUsage.STORAGE) === GPUBufferUsage.STORAGE) {
              const freelist = this.freeBuffers.get(buffer.size) || [];
              if (maxInFreeList === void 0 || freelist.length >= maxInFreeList) {
                buffer.destroy();
              } else {
                freelist.push(buffer);
              }
            } else if ((buffer.usage & GPUBufferUsage.UNIFORM) === GPUBufferUsage.UNIFORM) {
              const freelist = this.freeUniformBuffers.get(buffer.size) || [];
              if (maxInFreeList === void 0 || freelist.length >= maxInFreeList) {
                buffer.destroy();
              } else {
                freelist.push(buffer);
              }
            } else {
              buffer.destroy();
            }
          }
          this.buffersPending = [];
        } else {
          let capturedBuffers = this.capturedPendingBuffers.get(this.backend.currentSessionId);
          if (!capturedBuffers) {
            capturedBuffers = [];
            this.capturedPendingBuffers.set(this.backend.currentSessionId, capturedBuffers);
          }
          for (const buffer of this.buffersPending) {
            capturedBuffers.push(buffer);
          }
          this.buffersPending = [];
        }
      }
      dispose() {
        this.freeBuffers.forEach((buffers) => {
          buffers.forEach((buffer) => {
            buffer.destroy();
          });
        });
        this.freeUniformBuffers.forEach((buffers) => {
          buffers.forEach((buffer) => {
            buffer.destroy();
          });
        });
        this.storageCache.forEach((storage) => {
          storage.gpuData.buffer.destroy();
        });
        this.capturedPendingBuffers.forEach((buffers) => {
          buffers.forEach((buffer) => {
            buffer.destroy();
          });
        });
        this.storageCache = /* @__PURE__ */ new Map();
        this.freeBuffers = /* @__PURE__ */ new Map();
        this.freeUniformBuffers = /* @__PURE__ */ new Map();
        this.capturedPendingBuffers = /* @__PURE__ */ new Map();
      }
      onReleaseSession(sessionId) {
        const pendingBuffers = this.capturedPendingBuffers.get(sessionId);
        if (pendingBuffers) {
          pendingBuffers.forEach((buffer) => {
            buffer.destroy();
          });
          this.capturedPendingBuffers.delete(sessionId);
        }
      }
    };
    createGpuDataManager = (...args) => new GpuDataManagerImpl(...args);
  }
});

// web/lib/wasm/jsep/webgpu/attribute-with-cache-key.ts
var AttributeWithCacheKeyImpl, createAttributeWithCacheKey;
var init_attribute_with_cache_key = __esm({
  "web/lib/wasm/jsep/webgpu/attribute-with-cache-key.ts"() {
    "use strict";
    AttributeWithCacheKeyImpl = class {
      constructor(attribute) {
        Object.assign(this, attribute);
      }
      get cacheKey() {
        if (!this.key) {
          this.key = Object.getOwnPropertyNames(this).sort().map((name) => `${this[name]}`).join(";");
        }
        return this.key;
      }
    };
    createAttributeWithCacheKey = (attribute) => new AttributeWithCacheKeyImpl(attribute);
  }
});

// web/lib/wasm/jsep/util.ts
var MatMulUtil, BroadcastUtil, ShapeUtil, PoolConvUtil, GemmUtil, MIN_CLIP, MAX_CLIP;
var init_util = __esm({
  "web/lib/wasm/jsep/util.ts"() {
    "use strict";
    MatMulUtil = class {
      /**
       * Calculate the expected shape when matrix multiplication
       * @param a The shape of tensor A. Should be a tuple of 2 positive integers
       * @param b The shape of tensor B. Should be a tuple of 2 positive integers
       * @returns The expected shape of the result, or undefined if N/A
       */
      static calcMatMulShape(a, b) {
        return a[1] !== b[0] ? void 0 : [a[0], b[1]];
      }
    };
    BroadcastUtil = class {
      /**
       * Calculate the expected shape when broadcasting 2 tensors
       * @param a The shape of tensor A. Should be an array of positive integers
       * @param b The shape of tensor B. Should be an array of positive integers
       * @param isMatMul Whether the operation is MatMul
       * @returns The expected shape of the result, or undefined if N/A
       */
      static calcShape(adims, bdims, isMatMul = false) {
        const arank = adims.length;
        const brank = bdims.length;
        if (arank === 0) {
          return bdims;
        }
        if (brank === 0) {
          return adims;
        }
        const crank = Math.max(adims.length, bdims.length);
        const cdims = new Array(crank);
        if (isMatMul) {
          if (arank < 2 || brank < 2) {
            return void 0;
          }
          const cShapeMatMul = MatMulUtil.calcMatMulShape(
            [adims[arank - 2], adims[arank - 1]],
            [bdims[brank - 2], bdims[brank - 1]]
          );
          if (cShapeMatMul === void 0) {
            return void 0;
          }
          [cdims[crank - 2], cdims[crank - 1]] = cShapeMatMul;
        }
        for (let i = isMatMul ? 3 : 1; i <= crank; i++) {
          const aLen = arank - i < 0 ? 1 : adims[arank - i];
          const bLen = brank - i < 0 ? 1 : bdims[brank - i];
          if (aLen !== bLen && aLen > 1 && bLen > 1) {
            return void 0;
          }
          const max = Math.max(aLen, bLen);
          if (aLen && bLen) {
            cdims[crank - i] = Math.max(aLen, bLen);
          } else {
            if (max > 1) {
              return void 0;
            }
            cdims[crank - i] = 0;
          }
        }
        return cdims;
      }
      /**
       * Determine if a shape is unidirectional broadcastable to another shape
       * @param shape The input shape
       * @param finalShape The desired shape after broadcasting
       */
      static isValidBroadcast(shape, finalShape) {
        const inputRank = shape.length;
        const finalRank = finalShape.length;
        if (inputRank > finalRank) {
          return false;
        }
        for (let i = 1; i <= inputRank; i++) {
          if (shape[inputRank - i] !== 1 && shape[inputRank - i] !== finalShape[finalRank - i]) {
            return false;
          }
        }
        return true;
      }
    };
    ShapeUtil = class _ShapeUtil {
      /**
       * calculate the size (number of elements)
       */
      static size(dims) {
        return _ShapeUtil.getSizeFromDimensionRange(dims, 0, dims.length);
      }
      /**
       * convert dims corresponding to type change to pack. ex. uint8 data to uint32
       */
      static convertShape(dims, size = 4) {
        const rank = dims.length;
        if (rank === 0) {
          return [];
        }
        const newDims = new Array(rank);
        let i = rank - 1;
        while (i >= 0) {
          if (dims[i] % size === 0) {
            newDims[i] = dims[i] / size;
            break;
          }
          if (size % dims[i] !== 0) {
            throw new Error("cannot convert shape");
          }
          newDims[i] = 1;
          size /= dims[i];
          i--;
        }
        for (i--; i >= 0; i--) {
          newDims[i] = dims[i];
        }
        return newDims;
      }
      /**
       * calculate the size (number of elements) from the given axis (inclusive)
       */
      static sizeFromDimension(dims, axis) {
        if (axis < 0 || axis > dims.length) {
          throw new Error(`invalid dimension of ${axis} for sizeFromDimension as Tensor has ${dims.length} dimensions.`);
        }
        return _ShapeUtil.getSizeFromDimensionRange(dims, axis, dims.length);
      }
      /**
       * calculate the size (number of elements) to the given axis (exclusive)
       */
      static sizeToDimension(dims, axis) {
        if (axis < 0 || axis > dims.length) {
          throw new Error(`invalid dimension of ${axis} for sizeToDimension as Tensor has ${dims.length} dimensions.`);
        }
        return _ShapeUtil.getSizeFromDimensionRange(dims, 0, axis);
      }
      /**
       * calculate the size (number of elements) from and to the given axis [start, end)
       */
      static getSizeFromDimensionRange(dims, start, end) {
        let size = 1;
        for (let i = start; i < end; i++) {
          if (dims[i] < 0) {
            throw new Error(
              // eslint-disable-next-line max-len
              "cannot get valid size from specified dimension range. Most likely the range contains negative values in them."
            );
          }
          size *= dims[i];
        }
        return size;
      }
      static computeStrides(dims) {
        const rank = dims.length;
        if (rank === 0) {
          return [];
        } else if (rank === 1) {
          return [1];
        }
        const strides = new Array(rank);
        strides[rank - 1] = 1;
        strides[rank - 2] = dims[rank - 1];
        for (let i = rank - 3; i >= 0; --i) {
          strides[i] = strides[i + 1] * dims[i + 1];
        }
        return strides;
      }
      /**
       * normailze axis of range [-r, r) into [0, r).
       */
      static normalizeAxis(axis, tensorRank) {
        if (axis < -tensorRank && axis >= tensorRank) {
          throw new Error("unsupported axis for this operation.");
        }
        return axis < 0 ? axis + tensorRank : axis;
      }
      static normalizeAxes(axes, tensorRank) {
        return axes.map((x) => this.normalizeAxis(x, tensorRank ?? axes.length));
      }
      /**
       * Sorts a given array based on the indices in the Perm array
       * Used in Transpose
       * @param a Array to be sorted such as dims or strides
       * @param perm Perm given; if null a will be reversed
       */
      static sortBasedOnPerm(a, perm) {
        if (perm) {
          return perm.map((v) => a[v]);
        } else {
          return a.slice().reverse();
        }
      }
      /**
       * Pads a given shape according to the padding values
       * @param dims shape of the Tensor to be padded
       * @param pad pad values
       */
      static padShape(dims, pad2) {
        const rank = dims.length;
        return dims.map((v, i) => v + pad2[i] + pad2[i + rank]);
      }
      /**
       * Determines if the two shapes are identical
       * @param shape1
       * @param shape2
       */
      static areEqual(shape1, shape2) {
        if (shape1.length !== shape2.length) {
          return false;
        }
        return shape1.every((v, i) => v === shape2[i]);
      }
    };
    PoolConvUtil = class _PoolConvUtil {
      /**
       * Adjust the kernel, strides, pads to correct rank. Set to default value if not present
       * @param isGlobalOperator If true, perform global pooling.
       * @param inputDims The input tensor dimension.
       * @param kernelShape The size of the kernel along each axis.
       * @param strides Stride along each axis.
       * @param dilations Dilation along each axis.
       * @param pads Padding for the beginning and ending along each axis.
       */
      static adjustPoolAttributes(isGlobalOperator, inputDims, kernelShape, strides, dilations, pads) {
        if (!isGlobalOperator && kernelShape.length !== inputDims.length - 2) {
          throw new Error("length of specified kernel shapes should be 2 less than length of input dimensions");
        }
        if (isGlobalOperator) {
          for (let dim = 0; dim < inputDims.length - 2; dim++) {
            if (dim >= kernelShape.length) {
              kernelShape.push(inputDims[dim + 2]);
            } else {
              kernelShape[dim] = inputDims[dim + 2];
            }
          }
        }
        for (let dim = 0; dim < kernelShape.length; dim++) {
          if (dim < strides.length) {
            if (strides[dim] < 0) {
              throw new Error("strides should be greater than or equal to 1");
            }
          } else {
            strides.push(1);
          }
        }
        for (let dim = 0; dim < kernelShape.length; dim++) {
          if (dim < dilations.length) {
            if (dilations[dim] < 0) {
              throw new Error("dilations should be greater than or equal to 1");
            }
          } else {
            dilations.push(1);
          }
        }
        for (let dim = 0; dim < kernelShape.length * 2; dim++) {
          if (dim < pads.length) {
            if (pads[dim] < 0) {
              throw new Error("pad should be greater than or equal to 1");
            }
          } else {
            pads.push(0);
          }
        }
        for (let dim = 0; dim < kernelShape.length; dim++) {
          if (kernelShape[dim] <= 0) {
            throw new Error("kernel shapes need to be greater than 0");
          }
          if (pads[dim] >= kernelShape[dim] || pads[dim + kernelShape.length] >= kernelShape[dim]) {
            throw new Error("pads should be smaller than kernel");
          }
        }
      }
      // adjust pad values based on 'autoPad' attribute
      static adjustPadsBasedOnAutoPad(inputDims, strides, dilations, kernelShape, pads, isChannelLast, autoPad) {
        if (!autoPad) {
          return;
        }
        if (pads.length !== 2 * (inputDims.length - 2)) {
          throw new Error("length of pads should be twice the length of data dimensions");
        }
        if (strides.length !== inputDims.length - 2) {
          throw new Error("length of strides should be the length of data dimensions");
        }
        if (kernelShape.length !== inputDims.length - 2) {
          throw new Error("length of kernel shapes should be the length of data dimensions");
        }
        for (let dim = 0; dim < inputDims.length - 2; dim++) {
          _PoolConvUtil.adjustPadAndReturnShape(
            inputDims[dim + (isChannelLast ? 1 : 2)],
            strides[dim],
            dilations[dim],
            kernelShape[dim],
            pads,
            dim,
            dim + inputDims.length - 2,
            autoPad
          );
        }
      }
      /**
       * Calculate the output shape for Pool ops based on input attributes. (Should be used only for Pool ops)
       * @param isGlobalOperator If true, perform global pooling.
       * @param inputDims The input tensor dimension. (inputs[0].dims)
       * @param strides Stride along each axis.
       * @param dilations Dilation along each axis.
       * @param kernelShape The size of the kernel along each axis.
       * @param pads Padding for the beginning and ending along each axis.
       * @param autoPad DEPRECATED attribute supported for legacy models. Specifies how to implicitly calculate pads in each
       *     dimension. Can take values NOTSET, SAME_UPPER, SAME_LOWER, or VALID.
       */
      static computePoolOutputShape(isGlobalOperator, inputDims, strides, dilations, kernelShape, pads, autoPad) {
        if (inputDims.length <= 0) {
          throw new Error("input shape must be of size greater than 0");
        }
        const outputDims = [inputDims[0], inputDims[1]];
        _PoolConvUtil.computeShapeHelper(
          isGlobalOperator,
          inputDims,
          outputDims,
          strides,
          dilations,
          kernelShape,
          pads,
          autoPad
        );
        return outputDims;
      }
      /**
       * Calculate the output shape for Conv op based on input attributes. (Should be used only for Conv op)
       * @param inputDims The input tensor dimension. (inputs[0].dims)
       * @param filterDims The filter tensor dimension. (inputs[1].dims)
       * @param strides Stride along each axis.
       * @param kernelShape The size of the kernel along each axis.
       * @param pads Padding for the beginning and ending along each axis.
       * @param autoPad DEPRECATED attribute supported for legacy models. Specifies how to implicitly calculate pads in each
       *     dimension. Can take values NOTSET, SAME_UPPER, SAME_LOWER, or VALID.
       */
      static computeConvOutputShape(inputDims, filterDims, strides, dilations, kernelShape, pads, autoPad) {
        if (inputDims.length <= 0 || filterDims.length <= 0) {
          throw new Error("invalid input tensor dims or invalid filter tensor dims");
        }
        const outputDims = [inputDims[0], filterDims[0]];
        _PoolConvUtil.computeShapeHelper(false, inputDims, outputDims, strides, dilations, kernelShape, pads, autoPad);
        return outputDims;
      }
      // will compute output shapes for data dimensions ONLY (i.e.) no batch size and channels
      // called by computePoolOutputShape() and computeConvOutputShape()
      // adjust pads based on 'autoPad' attribute prior to shape computation
      static computeShapeHelper(isGlobalOperator, inputDims, outputDims, strides, dilations, kernelShape, pads, autoPad) {
        if (isGlobalOperator) {
          for (let dim = 0; dim < inputDims.length - 2; dim++) {
            outputDims.push(1);
          }
        } else {
          for (let dim = 0; dim < inputDims.length - 2; dim++) {
            outputDims.push(
              _PoolConvUtil.adjustPadAndReturnShape(
                inputDims[dim + 2],
                strides[dim],
                dilations[dim],
                kernelShape[dim],
                pads,
                dim,
                dim + inputDims.length - 2,
                autoPad
              )
            );
          }
        }
      }
      // helper for computeShapeHelper() and adjustPadsBasedOnAutoPad()
      // adjusts pad value for given 'autoPad' string and computes output shape along a particular dimension
      static adjustPadAndReturnShape(inSize, stride, dilation, kernel, pads, padHeadIndex, padTailIndex, autoPad) {
        const dkernel = dilation * (kernel - 1) + 1;
        if (autoPad && autoPad !== "NOTSET") {
          switch (autoPad) {
            case "VALID":
              pads[padHeadIndex] = 0;
              pads[padTailIndex] = 0;
              return Math.floor((inSize - dkernel) / stride + 1);
            case "SAME_LOWER":
            case "SAME_UPPER":
              if (dilation !== 1) {
                throw new Error("Dilation not supported for SAME_UPPER or SAME_LOWER");
              } else {
                const legacyTargetSize = (inSize + stride - 1) / stride;
                const padNeeded = (legacyTargetSize - 1) * stride + kernel - inSize;
                pads[padHeadIndex] = autoPad === "SAME_LOWER" ? Math.floor((padNeeded + 1) / 2) : Math.floor(padNeeded / 2);
                pads[padTailIndex] = padNeeded - pads[padHeadIndex];
                return Math.floor((inSize + padNeeded - kernel) / stride + 1);
              }
            default:
              throw new Error("Unsupported AutoPad type");
          }
        } else {
          return Math.floor((inSize + pads[padHeadIndex] + pads[padTailIndex] - dkernel) / stride + 1);
        }
      }
    };
    GemmUtil = class {
      // will make sure input shapes are compatible for this op
      // and return back the shape of the output in the form of a tuple
      // will throw exception if the input shapes are not compatible
      static getShapeOfGemmResult(leftShape, transLeft, rightShape, transRight, biasShape) {
        if (leftShape.length !== 2 || rightShape.length !== 2) {
          throw new Error("shape need to be of size 2");
        }
        let M;
        let K;
        let N;
        if (transLeft) {
          M = leftShape[1];
          K = leftShape[0];
        } else {
          M = leftShape[0];
          K = leftShape[1];
        }
        let kDim = -1;
        if (transRight) {
          N = rightShape[0];
          kDim = 1;
        } else {
          N = rightShape[1];
          kDim = 0;
        }
        if (rightShape[kDim] !== K) {
          throw new Error("dimension mismatch");
        }
        if (M <= 0 || N <= 0 || K <= 0) {
          throw new Error("invalid shape specified");
        }
        if (biasShape && !BroadcastUtil.isValidBroadcast(biasShape, [M, N])) {
          throw new Error("gemm: invalid bias shape for broadcast");
        }
        return [M, N, K];
      }
    };
    MIN_CLIP = -34028234663852886e22;
    MAX_CLIP = 34028234663852886e22;
  }
});

// web/lib/wasm/jsep/webgpu/ops/common.ts
var WORKGROUP_SIZE, getWgslMappedType, tensorTypeToWsglStorageType, tensorTypeToWsglValueType, createTensorShapeVariables, getMaxComponents, fillVector, castToF32, sumVector, getElementAt, createIndicesHelper, inputVariable, outputVariable, internalVariable, ShaderHelperImpl, createShaderHelper, getBroadcastDims;
var init_common = __esm({
  "web/lib/wasm/jsep/webgpu/ops/common.ts"() {
    "use strict";
    init_wasm_common();
    init_util();
    WORKGROUP_SIZE = 64;
    getWgslMappedType = (type, components) => {
      if (components === 3) {
        throw new Error("vec3 has same alignment as vec4, use vec4 instead");
      }
      switch (type) {
        case 10 /* float16 */:
          return components > 1 ? `vec${components}<f16>` : "f16";
        case 1 /* float */:
          return components > 1 ? `vec${components}<f32>` : "f32";
        case 6 /* int32 */:
          return components > 1 ? `vec${components}<i32>` : "i32";
        case 12 /* uint32 */:
          return components > 1 ? `vec${components}<u32>` : "u32";
        case 7 /* int64 */:
          if (components > 1) {
            throw new Error("currently not supported vecX of uint64 yet");
          }
          return ["vec2<u32>", "i32"];
        case 13 /* uint64 */:
          if (components > 1) {
            throw new Error("currently not supported vecX of uint64 yet");
          }
          return ["vec2<u32>", "u32"];
        case 9 /* bool */:
          if (components !== 4) {
            throw new Error("bool must be vec4");
          }
          return ["u32", "vec4<bool>"];
        case 22 /* int4 */:
          return "i32";
        case 21 /* uint4 */:
          return "u32";
        default:
          throw new Error(`Unknown data type: ${type}`);
      }
    };
    tensorTypeToWsglStorageType = (type, components = 1) => {
      const mappedType = getWgslMappedType(type, components);
      return typeof mappedType === "string" ? mappedType : mappedType[0];
    };
    tensorTypeToWsglValueType = (type, components = 1) => {
      const mappedType = getWgslMappedType(type, components);
      return typeof mappedType === "string" ? mappedType : mappedType[1];
    };
    createTensorShapeVariables = (...dims) => {
      const programUniforms = [];
      dims.forEach((dim) => {
        if (dim.length !== 0) {
          programUniforms.push(
            { type: 12 /* uint32 */, data: dim },
            { type: 12 /* uint32 */, data: ShapeUtil.computeStrides(dim) }
          );
        }
      });
      return programUniforms;
    };
    getMaxComponents = (size) => {
      if (size % 4 === 0) {
        return 4;
      } else if (size % 2 === 0) {
        return 2;
      }
      return 1;
    };
    fillVector = (dataType = "f32", components, value = "0") => {
      if (!components || components === 1) {
        return `${dataType}(${value})`;
      }
      return `vec${components}<${dataType}>(${value})`;
    };
    castToF32 = (dataType, components, value) => {
      if (dataType === "f32") {
        return value;
      }
      if (components === 1) {
        return `f32(${value})`;
      }
      return `vec${components}<f32>(${value})`;
    };
    sumVector = (name, components) => {
      if (components === 4) {
        return `(${name}.x + ${name}.y + ${name}.z + ${name}.w)`;
      } else if (components === 2) {
        return `(${name}.x + ${name}.y)`;
      } else if (components === 3) {
        return `(${name}.x + ${name}.y + ${name}.z)`;
      }
      return name;
    };
    getElementAt = (name, index, length, type) => {
      if (name.startsWith("uniforms.") && length > 4) {
        if (typeof index === "string") {
          if (type === "f16") {
            return `${name}[(${index}) / 8][(${index}) % 8 / 4][(${index}) % 8 % 4]`;
          } else {
            return `${name}[(${index}) / 4][(${index}) % 4]`;
          }
        } else {
          if (type === "f16") {
            return `${name}[${Math.floor(index / 8)}][${Math.floor(index % 8 / 4)}][${index % 8 % 4}]`;
          } else {
            return `${name}[${Math.floor(index / 4)}][${index % 4}]`;
          }
        }
      } else {
        return length > 1 ? `${name}[${index}]` : name;
      }
    };
    createIndicesHelper = (name, tensorType, shapeOrRank, usage, components) => {
      const useUniform = typeof shapeOrRank === "number";
      const rank = useUniform ? shapeOrRank : shapeOrRank.length;
      const rankIdentity = [...new Array(rank).keys()];
      const indicesType = rank < 2 ? "u32" : rank <= 4 ? `vec${rank}<u32>` : `array<u32, ${rank}>`;
      const mappedType = getWgslMappedType(tensorType, components);
      const valueType = typeof mappedType === "string" ? mappedType : mappedType[1];
      const storageType = typeof mappedType === "string" ? mappedType : mappedType[0];
      const type = { indices: indicesType, value: valueType, storage: storageType, tensor: tensorType };
      const normalizeDim = (dim) => typeof dim === "string" ? dim : `${dim}u`;
      const implementationUsed = {
        offsetToIndices: false,
        indicesToOffset: false,
        broadcastedIndicesToOffset: false,
        set: false,
        setByIndices: false,
        get: false,
        getByIndices: false
      };
      const uniformPrefix = useUniform ? "uniforms." : "";
      const shape = `${uniformPrefix}${name}_shape`;
      const strides = `${uniformPrefix}${name}_strides`;
      let o2iSnippet = "";
      for (let i = 0; i < rank - 1; i++) {
        o2iSnippet += `
    let dim${i} = current / ${getElementAt(strides, i, rank)};
    let rest${i} = current % ${getElementAt(strides, i, rank)};
    indices[${i}] = dim${i};
    current = rest${i};
    `;
      }
      o2iSnippet += `indices[${rank - 1}] = current;`;
      const offsetToIndicesImplementation = rank < 2 ? "" : `
  fn o2i_${name}(offset: u32) -> ${type.indices} {
    var indices: ${type.indices};
    var current = offset;
    ${o2iSnippet}
    return indices;
  }`;
      const offsetToIndices = (varOffset) => {
        implementationUsed.offsetToIndices = true;
        return rank < 2 ? varOffset : `o2i_${name}(${varOffset})`;
      };
      const offsets = [];
      if (rank >= 2) {
        for (let i = rank - 1; i >= 0; i--) {
          offsets.push(`${getElementAt(strides, i, rank)} * (indices[${i}])`);
        }
      }
      const indicesToOffsetImplementation = rank < 2 ? "" : `
  fn i2o_${name}(indices: ${type.indices}) -> u32 {
    return ${offsets.join("+")};
  }`;
      const indicesToOffset = (varIndices) => {
        implementationUsed.indicesToOffset = true;
        return rank < 2 ? varIndices : `i2o_${name}(${varIndices})`;
      };
      const indices = (...init2) => rank === 0 ? "0u" : `${type.indices}(${init2.map(normalizeDim).join(",")})`;
      const indicesGet = (varIndices, idx) => {
        if (rank < 2) {
          return `${varIndices}`;
        } else {
          return `${getElementAt(varIndices, idx, rank)}`;
        }
      };
      const indicesSet = (varIndices, idx, value) => {
        if (rank < 2) {
          return `${varIndices}=${value};`;
        } else {
          return `${getElementAt(varIndices, idx, rank)}=${value};`;
        }
      };
      const broadcastedIndicesToOffsetImplementation = {};
      const broadcastedIndicesToOffset = (varIndices, output) => {
        implementationUsed.broadcastedIndicesToOffset = true;
        const implKey = `${output.name}broadcastedIndicesTo${name}Offset`;
        if (implKey in broadcastedIndicesToOffsetImplementation) {
          return `${implKey}(${varIndices})`;
        }
        const offsets2 = [];
        for (let i = rank - 1; i >= 0; i--) {
          const idx = output.indicesGet("outputIndices", i + output.rank - rank);
          offsets2.push(`${indicesGet(strides, i)} * (${idx} % ${indicesGet(shape, i)})`);
        }
        broadcastedIndicesToOffsetImplementation[implKey] = `fn ${implKey}(outputIndices: ${output.type.indices}) -> u32 {
             return ${offsets2.length > 0 ? offsets2.join("+") : "0u"};
           }`;
        return `${implKey}(${varIndices})`;
      };
      const setByOffset = (offset, value) => (() => {
        if (type.storage === type.value) {
          return `${name}[${offset}]=${value};`;
        } else if (type.storage === "vec2<u32>" && type.value === "i32") {
          return `${name}[${offset}]=vec2<u32>(u32(${value}), select(0u, 0xFFFFFFFFu, ${value} < 0));`;
        } else if (type.storage === "vec2<u32>" && type.value === "u32") {
          return `${name}[${offset}]=vec2<u32>(u32(${value}), 0u);`;
        } else if (type.storage === "u32" && type.value === "vec4<bool>") {
          return `${name}[${offset}]=dot(vec4<u32>(0x1, 0x100, 0x10000, 0x1000000), vec4<u32>(${value}));`;
        } else {
          throw new Error(`not supported combination of storage type ${type.storage} and value type ${type.value} yet`);
        }
      })();
      const getByOffset = (offset) => (() => {
        if (type.storage === type.value) {
          return `${name}[${offset}]`;
        } else if (type.storage === "vec2<u32>" && type.value === "i32") {
          return `i32(${name}[${offset}].x)`;
        } else if (type.storage === "vec2<u32>" && type.value === "u32") {
          return `u32(${name}[${offset}].x)`;
        } else if (type.storage === "u32" && type.value === "vec4<bool>") {
          return `vec4<bool>(bool(${name}[${offset}] & 0xFFu), bool(${name}[${offset}] & 0xFF00u), bool(${name}[${offset}] & 0xFF0000u), bool(${name}[${offset}] & 0xFF000000u))`;
        } else {
          throw new Error(`not supported combination of storage type ${type.storage} and value type ${type.value} yet`);
        }
      })();
      const getByIndicesImplementation = rank < 2 ? "" : `
  fn get_${name}ByIndices(indices: ${type.indices}) -> ${valueType} {
    return ${getByOffset(`i2o_${name}(indices)`)};
  }`;
      const getImplementation = rank < 2 ? "" : (() => {
        const functionParams = rankIdentity.map((i) => `d${i}: u32`).join(", ");
        const dimsParams = rankIdentity.map((i) => `d${i}`).join(", ");
        return `
  fn get_${name}(${functionParams}) -> ${valueType} {
    return get_${name}ByIndices(${indices(dimsParams)});
  }`;
      })();
      const get = (...indices2) => {
        if (indices2.length !== rank) {
          throw new Error(`indices length must be ${rank}`);
        }
        const normalizedIndices = indices2.map(normalizeDim).join(",");
        if (rank === 0) {
          return getByOffset("0u");
        } else if (rank === 1) {
          return getByOffset(normalizedIndices[0]);
        } else {
          implementationUsed.get = true;
          implementationUsed.getByIndices = true;
          implementationUsed.indicesToOffset = true;
          return `get_${name}(${normalizedIndices})`;
        }
      };
      const getByIndices = (varIndices) => {
        if (rank < 2) {
          return getByOffset(varIndices);
        } else {
          implementationUsed.getByIndices = true;
          implementationUsed.indicesToOffset = true;
          return `get_${name}ByIndices(${varIndices})`;
        }
      };
      const setByIndicesImplementation = rank < 2 ? "" : `
  fn set_${name}ByIndices(indices: ${type.indices}, value: ${valueType}) {
    ${setByOffset(`i2o_${name}(indices)`, "value")}
  }`;
      const setImplementation = rank < 2 ? "" : (() => {
        const functionParams = rankIdentity.map((i) => `d${i}: u32`).join(", ");
        const dimsParams = rankIdentity.map((i) => `d${i}`).join(", ");
        return `
  fn set_${name}(${functionParams}, value: ${valueType}) {
    set_${name}ByIndices(${indices(dimsParams)}, value);
  }`;
      })();
      const set = (...indicesAndValue) => {
        if (indicesAndValue.length !== rank + 1) {
          throw new Error(`indices length must be ${rank}`);
        }
        const value = indicesAndValue[rank];
        if (typeof value !== "string") {
          throw new Error("value must be string");
        }
        const normalizedIndices = indicesAndValue.slice(0, rank).map(normalizeDim).join(",");
        if (rank === 0) {
          return setByOffset("0u", value);
        } else if (rank === 1) {
          return setByOffset(normalizedIndices[0], value);
        } else {
          implementationUsed.set = true;
          implementationUsed.setByIndices = true;
          implementationUsed.indicesToOffset = true;
          return `set_${name}(${normalizedIndices}, ${value})`;
        }
      };
      const setByIndices = (varIndices, value) => {
        if (rank < 2) {
          return setByOffset(varIndices, value);
        } else {
          implementationUsed.setByIndices = true;
          implementationUsed.indicesToOffset = true;
          return `set_${name}ByIndices(${varIndices}, ${value});`;
        }
      };
      const impl = () => {
        const impls = [];
        let needShapeStrides = false;
        if (implementationUsed.offsetToIndices) {
          impls.push(offsetToIndicesImplementation);
          needShapeStrides = true;
        }
        if (implementationUsed.indicesToOffset) {
          impls.push(indicesToOffsetImplementation);
          needShapeStrides = true;
        }
        if (implementationUsed.broadcastedIndicesToOffset) {
          Object.values(broadcastedIndicesToOffsetImplementation).forEach((impl2) => impls.push(impl2));
          needShapeStrides = true;
        }
        if (implementationUsed.set) {
          impls.push(setImplementation);
          needShapeStrides = true;
        }
        if (implementationUsed.setByIndices) {
          impls.push(setByIndicesImplementation);
          needShapeStrides = true;
        }
        if (implementationUsed.get) {
          impls.push(getImplementation);
          needShapeStrides = true;
        }
        if (implementationUsed.getByIndices) {
          impls.push(getByIndicesImplementation);
          needShapeStrides = true;
        }
        if (!useUniform && needShapeStrides) {
          impls.unshift(
            `const ${shape} = ${type.indices}(${shapeOrRank.join(",")});`,
            `const ${strides} = ${type.indices}(${ShapeUtil.computeStrides(shapeOrRank).join(",")});`
          );
        }
        return impls.join("\n");
      };
      return {
        impl,
        type,
        offsetToIndices,
        indicesToOffset,
        broadcastedIndicesToOffset,
        indices,
        indicesGet,
        indicesSet,
        set,
        setByOffset,
        setByIndices,
        get,
        getByOffset,
        getByIndices,
        // isVec4,
        usage,
        name,
        strides,
        shape,
        rank
      };
    };
    inputVariable = (name, type, shapeOrRank, components = 1) => createIndicesHelper(name, type, shapeOrRank, "input", components);
    outputVariable = (name, type, shapeOrRank, components = 1) => createIndicesHelper(name, type, shapeOrRank, "output", components);
    internalVariable = (name, type, shapeOrRank, components = 1) => createIndicesHelper(name, type, shapeOrRank, "internal", components);
    ShaderHelperImpl = class {
      constructor(normalizedDispatchGroup, limits) {
        this.normalizedDispatchGroup = normalizedDispatchGroup;
        this.limits = limits;
        this.internalVariables = [];
        this.variables = [];
        this.uniforms = [];
        this.variableIndex = 0;
      }
      guardAgainstOutOfBoundsWorkgroupSizes(size) {
        const sizeInCode = typeof size === "number" ? `${size}u` : size;
        return `if (global_idx >= ${sizeInCode}) { return; }`;
      }
      mainStart(workgroupSize = WORKGROUP_SIZE) {
        const workgroupSizeX = typeof workgroupSize === "number" ? workgroupSize : workgroupSize[0];
        const workgroupSizeY = typeof workgroupSize === "number" ? 1 : workgroupSize[1];
        const workgroupSizeZ = typeof workgroupSize === "number" ? 1 : workgroupSize[2];
        if (workgroupSizeX > this.limits.maxComputeWorkgroupSizeX || workgroupSizeY > this.limits.maxComputeWorkgroupSizeY || workgroupSizeZ > this.limits.maxComputeWorkgroupSizeZ) {
          throw new Error(
            `workgroup size [${workgroupSizeX}, ${workgroupSizeY}, ${workgroupSizeZ}] exceeds the maximum workgroup size [${this.limits.maxComputeWorkgroupSizeX}, ${this.limits.maxComputeWorkgroupSizeY}, ${this.limits.maxComputeWorkgroupSizeZ}].`
          );
        }
        if (workgroupSizeX * workgroupSizeY * workgroupSizeZ > this.limits.maxComputeInvocationsPerWorkgroup) {
          throw new Error(
            `workgroup size [${workgroupSizeX}, ${workgroupSizeY}, ${workgroupSizeZ}] exceeds the maximum workgroup invocations ${this.limits.maxComputeInvocationsPerWorkgroup}.`
          );
        }
        const is1DimensionDispatch = this.normalizedDispatchGroup[1] === 1 && this.normalizedDispatchGroup[2] === 1;
        const paramList = is1DimensionDispatch ? `@builtin(global_invocation_id) global_id : vec3<u32>,
    @builtin(workgroup_id) workgroup_id : vec3<u32>,
    @builtin(local_invocation_id) local_id : vec3<u32>` : `@builtin(global_invocation_id) global_id : vec3<u32>,
                                             @builtin(local_invocation_id) local_id : vec3<u32>,
    @builtin(local_invocation_index) local_idx : u32,
    @builtin(workgroup_id) workgroup_id : vec3<u32>,
    @builtin(num_workgroups) num_workgroups : vec3<u32>`;
        const globalIdxDefinition = is1DimensionDispatch ? "let global_idx = global_id.x; let local_idx = local_id.x;" : `let global_idx = (workgroup_id.z * num_workgroups[0] * num_workgroups[1] +
          workgroup_id.y * num_workgroups[0] + workgroup_id.x) * ${workgroupSizeX * workgroupSizeY * workgroupSizeZ}u + local_idx;`;
        return `@compute @workgroup_size(${workgroupSizeX}, ${workgroupSizeY}, ${workgroupSizeZ})
  fn main(${paramList}) {
    ${globalIdxDefinition}
  `;
      }
      appendVariableUniforms(variable) {
        if (variable.rank !== 0) {
          if (variable.shape.startsWith("uniforms.")) {
            this.uniforms.push({ name: variable.shape.replace("uniforms.", ""), type: "u32", length: variable.rank });
          }
          if (variable.strides.startsWith("uniforms.")) {
            this.uniforms.push({ name: variable.strides.replace("uniforms.", ""), type: "u32", length: variable.rank });
          }
        }
      }
      declareVariable(variable, bindingIndex) {
        if (variable.usage === "internal") {
          throw new Error("cannot use internal variable with declareVariable(). use registerInternalVariables() instead.");
        }
        this.variables.push(variable);
        this.appendVariableUniforms(variable);
        const access = variable.usage === "input" ? "read" : "read_write";
        const storageType = variable.type.storage;
        return `@group(0) @binding(${bindingIndex}) var<storage, ${access}> ${variable.name}: array<${storageType}>;`;
      }
      declareVariables(...variables) {
        return variables.map((v) => this.declareVariable(v, this.variableIndex++)).join("\n");
      }
      registerInternalVariable(variable) {
        if (variable.usage !== "internal") {
          throw new Error(
            "cannot use input or output variable with registerInternalVariable(). use declareVariables() instead."
          );
        }
        this.internalVariables.push(variable);
        this.appendVariableUniforms(variable);
      }
      registerInternalVariables(...variables) {
        variables.forEach((v) => this.registerInternalVariable(v));
        return this;
      }
      registerUniform(name, type, length = 1) {
        this.uniforms.push({ name, type, length });
        return this;
      }
      registerUniforms(additionalUniforms) {
        this.uniforms = this.uniforms.concat(additionalUniforms);
        return this;
      }
      uniformDeclaration() {
        if (this.uniforms.length === 0) {
          return "";
        }
        const uniformSnippets = [];
        for (const { name, type, length } of this.uniforms) {
          if (length && length > 4) {
            if (type === "f16") {
              uniformSnippets.push(`@align(16) ${name}:array<mat2x4<${type}>, ${Math.ceil(length / 8)}>`);
            } else {
              uniformSnippets.push(`${name}:array<vec4<${type}>, ${Math.ceil(length / 4)}>`);
            }
          } else {
            const typeTemp = length == null || length === 1 ? type : `vec${length}<${type}>`;
            uniformSnippets.push(`${name}:${typeTemp}`);
          }
        }
        return `
      struct Uniforms { ${uniformSnippets.join(", ")} };
      @group(0) @binding(${this.variableIndex}) var<uniform> uniforms: Uniforms;`;
      }
      /**
       * Get additional implementation that needs to be added to the shader source.
       */
      get additionalImplementations() {
        return this.uniformDeclaration() + this.variables.map((i) => i.impl()).join("\n") + this.internalVariables.map((i) => i.impl()).join("\n");
      }
      /**
       * Get the variable info of the shader program.
       */
      get variablesInfo() {
        if (this.uniforms.length === 0) {
          return void 0;
        }
        const uniformWgslTypeToDataType = (type) => [12 /* uint32 */, 10 /* float16 */, 1 /* float */, 6 /* int32 */][["u32", "f16", "f32", "i32"].indexOf(type)];
        return this.uniforms.map((u) => [uniformWgslTypeToDataType(u.type), u.length ?? 1]);
      }
    };
    createShaderHelper = (dispatchGroup, limits) => new ShaderHelperImpl(dispatchGroup, limits);
    getBroadcastDims = (inShape, outShape) => {
      const inRank = inShape.length;
      const dims = [];
      for (let i = 0; i < inRank; i++) {
        const dim = inRank - 1 - i;
        const a = inShape[dim] || 1;
        const b = outShape[outShape.length - 1 - i] || 1;
        if (b > 1 && a === 1) {
          dims.unshift(dim);
        }
      }
      return dims;
    };
  }
});

// web/lib/wasm/jsep/webgpu/ops/transpose.ts
var validateInputs, getAdjustedPerm, getOutputShape, permFunctionBody, createTransposeProgramInfo, transpose, parseTransposeAttributes;
var init_transpose = __esm({
  "web/lib/wasm/jsep/webgpu/ops/transpose.ts"() {
    "use strict";
    init_wasm_common();
    init_util();
    init_attribute_with_cache_key();
    init_common();
    validateInputs = (inputs) => {
      if (!inputs || inputs.length !== 1) {
        throw new Error("Transpose requires 1 input.");
      }
    };
    getAdjustedPerm = (inputRank, perm) => perm && perm.length !== inputRank ? [...new Array(inputRank).keys()].reverse() : perm;
    getOutputShape = (inputShape, perm) => ShapeUtil.sortBasedOnPerm(inputShape, getAdjustedPerm(inputShape.length, perm));
    permFunctionBody = (perm, rank, input, output) => {
      const reverseFunc = [];
      reverseFunc.push(`fn perm(i: ${output.type.indices}) -> ${input.type.indices} {
    var a: ${input.type.indices};`);
      for (let i = 0; i < rank; ++i) {
        reverseFunc.push(input.indicesSet("a", perm[i], `i[${i}]`));
      }
      reverseFunc.push("return a;}");
      return reverseFunc.join("\n");
    };
    createTransposeProgramInfo = (inputTensor, permAttr) => {
      const inputDataType = inputTensor.dataType;
      const inputRank = inputTensor.dims.length;
      const perm = getAdjustedPerm(inputRank, permAttr);
      const outputShape = getOutputShape(inputTensor.dims, perm);
      const output = outputVariable("output", inputDataType, outputShape.length);
      const input = inputVariable("a", inputDataType, inputRank);
      let getShaderSource;
      if (perm.length === 2 && perm[0] === 1 && perm[1] === 0) {
        const wgslType = output.type.value;
        const workgroupSize = [16, 16, 1];
        getShaderSource = (shaderHelper) => `
  ${shaderHelper.registerUniform("output_size", "u32").declareVariables(input, output)}
  var<workgroup> tile : array<array<${wgslType}, ${workgroupSize[0] + 1}>, ${workgroupSize[0]}>;
  ${shaderHelper.mainStart(workgroupSize)}
    var x = workgroup_id.x * ${workgroupSize[0]}u + local_id.x;
    var y = workgroup_id.y * ${workgroupSize[0]}u + local_id.y;
    let width = uniforms.output_shape[0];
    let height = uniforms.output_shape[1];
    if (x < width && y < height) {
      tile[local_id.y][local_id.x] = ${input.getByOffset("y * width + x")};
    }
    workgroupBarrier();
    x = workgroup_id.y * ${workgroupSize[0]}u + local_id.x;
    y = workgroup_id.x * ${workgroupSize[0]}u + local_id.y;
    if (x < height && y < width) {
      ${output.setByOffset("y * height + x", "tile[local_id.x][local_id.y]")}
    }
  }`;
      } else {
        getShaderSource = (shaderHelper) => `
  ${shaderHelper.registerUniform("output_size", "u32").declareVariables(input, output)}

  ${permFunctionBody(perm, inputRank, input, output)}

  ${shaderHelper.mainStart()}
    ${shaderHelper.guardAgainstOutOfBoundsWorkgroupSizes("uniforms.output_size")}

    let indices = ${output.offsetToIndices("global_idx")};
    let aIndices = perm(indices);

    ${output.setByOffset("global_idx", input.getByIndices("aIndices"))}
  }`;
      }
      return {
        name: "Transpose",
        shaderCache: { hint: `${permAttr}`, inputDependencies: ["rank"] },
        getRunData: () => {
          const outputSize = ShapeUtil.size(outputShape);
          return {
            outputs: [{ dims: outputShape, dataType: inputTensor.dataType }],
            dispatchGroup: { x: Math.ceil(
              outputSize / 64
              /* workgroup size */
            ) },
            programUniforms: [
              { type: 12 /* uint32 */, data: outputSize },
              ...createTensorShapeVariables(inputTensor.dims, outputShape)
            ]
          };
        },
        getShaderSource
      };
    };
    transpose = (context, attributes) => {
      validateInputs(context.inputs);
      context.compute(createTransposeProgramInfo(context.inputs[0], attributes.perm));
    };
    parseTransposeAttributes = (attributes) => createAttributeWithCacheKey({ perm: attributes.perm });
  }
});

// web/lib/wasm/jsep/webgpu/ops/reduce-shared.ts
var reduceOps, reduceSharedOps, reduceInitValues, reduceOutputValues, getInnerMostAxes, computeOutAndReduceShapes, expandShapeToKeepDim, areAxesInnerMostDims, getAxesPermutation, createReduceSharedProgramInfo, reduceCommon, reduceMeanShared, reduceL1Shared, reduceL2Shared, reduceLogSumExpShared, reduceMaxShared, reduceMinShared, reduceProdShared, reduceSumShared, reduceSumSquareShared, reduceLogSumShared;
var init_reduce_shared = __esm({
  "web/lib/wasm/jsep/webgpu/ops/reduce-shared.ts"() {
    "use strict";
    init_wasm_common();
    init_util();
    init_common();
    init_reduce();
    init_transpose();
    reduceOps = {
      max: "select(bestValue, candidate, candidate > bestValue)",
      min: "select(bestValue, candidate, candidate < bestValue)",
      mean: "bestValue + candidate",
      sum: "bestValue + candidate",
      prod: "bestValue * candidate",
      sumSquare: "bestValue + candidate * candidate",
      logSumExp: "bestValue + exp(candidate)",
      l1: "bestValue + abs(candidate)",
      l2: "bestValue + candidate * candidate",
      logSum: "bestValue + candidate"
    };
    reduceSharedOps = {
      max: "select(bestValue, candidate, candidate > bestValue)",
      min: "select(bestValue, candidate, candidate < bestValue)",
      mean: "bestValue + candidate",
      sum: "bestValue + candidate",
      prod: "bestValue * candidate",
      sumSquare: "bestValue + candidate",
      logSumExp: "bestValue + candidate",
      l1: "bestValue + candidate",
      l2: "bestValue + candidate",
      logSum: "bestValue + candidate"
    };
    reduceInitValues = {
      max: "_A[offset]",
      min: "_A[offset]",
      mean: "0",
      sum: "0",
      prod: "1",
      sumSquare: "0",
      logSumExp: "0",
      l1: "0",
      l2: "0",
      logSum: "0"
    };
    reduceOutputValues = {
      max: "bestValue",
      min: "bestValue",
      sum: "bestValue",
      prod: "bestValue",
      sumSquare: "bestValue",
      logSumExp: "log(bestValue)",
      l1: "bestValue",
      l2: "sqrt(bestValue)",
      logSum: "log(bestValue)"
    };
    getInnerMostAxes = (numInnerAxes, rank) => {
      const res = [];
      for (let i = rank - numInnerAxes; i < rank; ++i) {
        res.push(i);
      }
      return res;
    };
    computeOutAndReduceShapes = (shape, axes) => {
      const outputShape = [];
      const rank = shape.length;
      for (let dim = 0; dim < rank; dim++) {
        if (axes.indexOf(dim) === -1) {
          outputShape.push(shape[dim]);
        }
      }
      const reduceShape = axes.map((dim) => shape[dim]);
      return [outputShape, reduceShape];
    };
    expandShapeToKeepDim = (shape, axes) => {
      const rank = shape.length + axes.length;
      const expandShape = [];
      let shapeIdx = 0;
      for (let dim = 0; dim < rank; dim++) {
        if (axes.indexOf(dim) === -1) {
          expandShape.push(shape[shapeIdx++]);
        } else {
          expandShape.push(1);
        }
      }
      return expandShape;
    };
    areAxesInnerMostDims = (axes, rank) => {
      for (let i = 0; i < axes.length; ++i) {
        if (axes[axes.length - i - 1] !== rank - 1 - i) {
          return false;
        }
      }
      return true;
    };
    getAxesPermutation = (axes, rank) => {
      const res = [];
      if (!areAxesInnerMostDims(axes, rank)) {
        for (let i = 0; i < rank; ++i) {
          if (axes.indexOf(i) === -1) {
            res.push(i);
          }
        }
        axes.forEach((axis) => res.push(axis));
      }
      return res;
    };
    createReduceSharedProgramInfo = (name, shaderCache, inputs, reduceType, outputDataType, outputShape, reduceShape) => {
      const inputShape = inputs[0].dims;
      const outputSize = ShapeUtil.size(outputShape);
      const reduceSize = ShapeUtil.size(reduceShape);
      const input = inputVariable("_A", inputs[0].dataType, inputShape);
      const output = outputVariable("output", outputDataType, outputShape);
      const workgroupSize = 32;
      const sharedMemorySnippet = `
          var<workgroup> aBestValues : array<f32, ${workgroupSize}>;
       `;
      const getShaderSource = (shaderHelper) => `
        ${shaderHelper.registerUniform("reduceSize", "u32").declareVariables(input, output)}
        ${sharedMemorySnippet}
        fn DIV_CEIL(a : u32, b : u32) -> u32 {
          return ((a - 1u) / b + 1u);
         }
         ${shaderHelper.mainStart(workgroupSize)}

          let outputIndex = global_idx / ${workgroupSize};
          let offset = outputIndex * uniforms.reduceSize;

          var bestValue = f32(${reduceInitValues[reduceType]});
          let Length = uniforms.reduceSize;
          for (var k = local_idx; k < Length; k = k + ${workgroupSize}) {
           let candidate = f32(${input.getByOffset("offset + k")});
           bestValue = ${reduceOps[reduceType]};
          }
          aBestValues[local_idx] = bestValue;
          workgroupBarrier();

         var reduceSize = min(Length, ${workgroupSize}u);
         for (var currentSize = reduceSize / 2u; reduceSize > 1u;
             currentSize = reduceSize / 2u) {
           let interval = DIV_CEIL(reduceSize, 2u);
           if (local_idx < currentSize) {
            let candidate = aBestValues[local_idx + interval];
            bestValue = ${reduceSharedOps[reduceType]};
            aBestValues[local_idx] = bestValue;
           }
           reduceSize = interval;
           workgroupBarrier();
         }

         if (local_idx == 0u) {
          ${output.setByOffset(
        "outputIndex",
        `${reduceType === "mean" ? `${output.type.storage}(bestValue / f32(uniforms.reduceSize))` : `${output.type.storage}(${reduceOutputValues[reduceType]})`}`
      )};
         }
        }`;
      return {
        name,
        shaderCache,
        getShaderSource,
        getRunData: () => ({
          outputs: [{ dims: outputShape, dataType: outputDataType }],
          dispatchGroup: { x: outputSize },
          programUniforms: [{ type: 12 /* uint32 */, data: reduceSize }]
        })
      };
    };
    reduceCommon = (context, name, attributes, reduceType) => {
      const updatedAttributes = context.inputs.length === 1 ? attributes : createReduceAttributesFromInputs(context.inputs, attributes);
      let updatedAxes = updatedAttributes.axes;
      if (updatedAxes.length === 0 && !updatedAttributes.noopWithEmptyAxes) {
        updatedAxes = context.inputs[0].dims.map((_dim, i) => i);
      }
      const normalizeAxes = ShapeUtil.normalizeAxes(updatedAxes, context.inputs[0].dims.length);
      let axes = normalizeAxes;
      let input = context.inputs[0];
      const permutedAxes = getAxesPermutation(axes, context.inputs[0].dims.length);
      if (permutedAxes.length > 0) {
        input = context.compute(createTransposeProgramInfo(context.inputs[0], permutedAxes), {
          inputs: [0],
          outputs: [-1]
        })[0];
        axes = getInnerMostAxes(axes.length, input.dims.length);
      }
      const [outputShape, reduceShape] = computeOutAndReduceShapes(input.dims, axes);
      let finalOutputShape = outputShape;
      if (updatedAttributes.keepDims) {
        finalOutputShape = expandShapeToKeepDim(outputShape, normalizeAxes);
      }
      context.compute(
        createReduceSharedProgramInfo(
          name,
          { hint: updatedAttributes.cacheKey, inputDependencies: ["type"] },
          [input],
          reduceType,
          context.inputs[0].dataType,
          finalOutputShape,
          reduceShape
        ),
        { inputs: [input] }
      );
    };
    reduceMeanShared = (context, attributes) => {
      reduceCommon(context, "ReduceMeanShared", attributes, "mean");
    };
    reduceL1Shared = (context, attributes) => {
      reduceCommon(context, "ReduceL1Shared", attributes, "l1");
    };
    reduceL2Shared = (context, attributes) => {
      reduceCommon(context, "ReduceL2Shared", attributes, "l2");
    };
    reduceLogSumExpShared = (context, attributes) => {
      reduceCommon(context, "ReduceLogSumExpShared", attributes, "logSumExp");
    };
    reduceMaxShared = (context, attributes) => {
      reduceCommon(context, "ReduceMaxShared", attributes, "max");
    };
    reduceMinShared = (context, attributes) => {
      reduceCommon(context, "ReduceMinShared", attributes, "min");
    };
    reduceProdShared = (context, attributes) => {
      reduceCommon(context, "ReduceProdShared", attributes, "prod");
    };
    reduceSumShared = (context, attributes) => {
      reduceCommon(context, "ReduceSumShared", attributes, "sum");
    };
    reduceSumSquareShared = (context, attributes) => {
      reduceCommon(context, "ReduceSumSquareShared", attributes, "sumSquare");
    };
    reduceLogSumShared = (context, attributes) => {
      reduceCommon(context, "ReduceLogSumShared", attributes, "logSum");
    };
  }
});

// web/lib/wasm/jsep/webgpu/ops/reduce.ts
var validateInputs2, noOp, createReduceProgramInfo, createReduceAttributesFromInputs, runReduceProgram, reduceLogSumNaive, reduceL1Naive, reduceL2Naive, reduceLogSumExpNaive, reduceMaxNaive, reduceMeanNaive, reduceMinNaive, reduceProdNaive, reduceSumNaive, reduceSumSquareNaive, useNaiveReduceMethod, reduceMean, reduceL1, reduceL2, reduceLogSumExp, reduceMax, reduceMin, reduceProd, reduceSum, reduceSumSquare, reduceLogSum;
var init_reduce = __esm({
  "web/lib/wasm/jsep/webgpu/ops/reduce.ts"() {
    "use strict";
    init_wasm_common();
    init_util();
    init_attribute_with_cache_key();
    init_common();
    init_reduce_shared();
    validateInputs2 = (inputs) => {
      if (!inputs || inputs.length === 0 || inputs.length > 2) {
        throw new Error("Reduce op requires 1 or 2 inputs.");
      }
      if (inputs.length === 2 && inputs[1].dims.length !== 1) {
        throw new Error("Invalid axes input dims.");
      }
    };
    noOp = (input) => ["", "", `var value = ${input.getByIndices("input_indices")};`, ""];
    createReduceProgramInfo = (name, shaderCache, inputs, reduceOp, axesInput, outputDataType, keepDims = false, noopWithEmptyAxes = false) => {
      const outputShape = [];
      const inputShape = inputs[0].dims;
      const inputRank = inputShape.length;
      const axes = ShapeUtil.normalizeAxes(axesInput, inputRank);
      const reduceOnAllAxes = !noopWithEmptyAxes && axes.length === 0;
      inputShape.forEach((d, i) => {
        if (reduceOnAllAxes || axes.indexOf(i) >= 0) {
          if (keepDims) {
            outputShape.push(1);
          }
        } else {
          outputShape.push(d);
        }
      });
      const outputRank = outputShape.length;
      const outputSize = ShapeUtil.size(outputShape);
      const getShaderSource = (shaderHelper) => {
        const idxCopy = [];
        const input = inputVariable("_A", inputs[0].dataType, inputRank);
        const output = outputVariable("output", outputDataType, outputRank);
        const ops = reduceOp(input, output, axes);
        let reduceOps2 = ops[2];
        for (let k = 0, l = 0; k < inputRank; k++) {
          if (reduceOnAllAxes || axes.indexOf(k) >= 0) {
            if (keepDims) {
              l++;
            }
            reduceOps2 = `for(var j${k}: u32 = 0; j${k} < ${inputShape[k]}; j${k}++) {
                  ${ops[2].includes("last_index") ? `let last_index = j${k};` : ""}
                  ${input.indicesSet("input_indices", k, `j${k}`)}
                  ${reduceOps2}
                }`;
          } else {
            idxCopy.push(`${input.indicesSet("input_indices", k, output.indicesGet("output_indices", l))};`);
            l++;
          }
        }
        return `

        ${shaderHelper.registerUniform("output_size", "u32").declareVariables(input, output)}

        ${shaderHelper.mainStart()}
          ${shaderHelper.guardAgainstOutOfBoundsWorkgroupSizes("uniforms.output_size")}
          var input_indices: ${input.type.indices};
          let output_indices = ${output.offsetToIndices("global_idx")};

          ${idxCopy.join("\n")}
          ${ops[0]}       // init ops for reduce max/min
          ${ops[1]}
          ${reduceOps2}
          ${ops[3]}
          ${ops.length === 4 ? output.setByOffset("global_idx", "value") : ops.slice(4).join("\n")}
        }`;
      };
      return {
        name,
        shaderCache,
        getShaderSource,
        getRunData: () => ({
          outputs: [{ dims: outputShape, dataType: outputDataType }],
          dispatchGroup: { x: Math.ceil(
            outputSize / 64
            /* workgroup size */
          ) },
          programUniforms: [
            { type: 12 /* uint32 */, data: outputSize },
            ...createTensorShapeVariables(inputShape, outputShape)
          ]
        })
      };
    };
    createReduceAttributesFromInputs = (inputs, attributes) => {
      const axes = [];
      if (inputs[1].dims[0] > 0) {
        inputs[1].getBigInt64Array().forEach((v) => axes.push(Number(v)));
      }
      return createAttributeWithCacheKey({
        axes,
        keepDims: attributes.keepDims,
        noopWithEmptyAxes: attributes.noopWithEmptyAxes
      });
    };
    runReduceProgram = (context, name, attributes, reduceOp) => {
      const inputs = context.inputs;
      const updatedAttributes = inputs.length === 1 ? attributes : createReduceAttributesFromInputs(inputs, attributes);
      context.compute(
        createReduceProgramInfo(
          name,
          { hint: updatedAttributes.cacheKey, inputDependencies: ["rank"] },
          [inputs[0]],
          updatedAttributes.noopWithEmptyAxes && updatedAttributes.axes.length === 0 ? noOp : reduceOp,
          updatedAttributes.axes,
          inputs[0].dataType,
          updatedAttributes.keepDims,
          updatedAttributes.noopWithEmptyAxes
        ),
        { inputs: [0] }
      );
    };
    reduceLogSumNaive = (context, attributes) => {
      validateInputs2(context.inputs);
      const reduceOp = (input, output) => [
        `var value = ${output.type.storage}(0);`,
        "",
        `value += ${input.getByIndices("input_indices")};`,
        "value = log(value);"
      ];
      runReduceProgram(context, "ReduceLogSum", attributes, reduceOp);
    };
    reduceL1Naive = (context, attributes) => {
      validateInputs2(context.inputs);
      const reduceOp = (input, output) => [
        `var value = ${output.type.storage}(0);`,
        "",
        `value += abs(${input.getByIndices("input_indices")});`,
        ""
      ];
      runReduceProgram(context, "ReduceL1", attributes, reduceOp);
    };
    reduceL2Naive = (context, attributes) => {
      validateInputs2(context.inputs);
      const reduceOp = (input, output) => [
        `var t = ${output.type.value}(0); var value = ${output.type.value}(0);`,
        "",
        `t = ${input.getByIndices("input_indices")}; value += (t * t);`,
        "value = sqrt(value);"
      ];
      runReduceProgram(context, "ReduceL2", attributes, reduceOp);
    };
    reduceLogSumExpNaive = (context, attributes) => {
      validateInputs2(context.inputs);
      const reduceOp = (input, output) => [
        `var value = ${output.type.storage}(0);`,
        "",
        `value += exp(${input.getByIndices("input_indices")});`,
        "value = log(value);"
      ];
      runReduceProgram(context, "ReduceLogSumExp", attributes, reduceOp);
    };
    reduceMaxNaive = (context, attributes) => {
      validateInputs2(context.inputs);
      const reduceOp = (input, _output, axes) => {
        const idxZero = [];
        for (let k = 0; k < input.rank; k++) {
          if (axes.indexOf(k) >= 0 || axes.length === 0) {
            idxZero.push(input.indicesSet("input_indices", k, 0));
          }
        }
        return [
          `${idxZero.join("\n")}`,
          `var value = ${input.getByIndices("input_indices")};`,
          `value = max(value, ${input.getByIndices("input_indices")});`,
          ""
        ];
      };
      runReduceProgram(context, "ReduceMax", attributes, reduceOp);
    };
    reduceMeanNaive = (context, attributes) => {
      validateInputs2(context.inputs);
      const reduceOp = (input, output, axes) => {
        let size = 1;
        for (let k = 0; k < input.rank; k++) {
          if (axes.indexOf(k) >= 0 || axes.length === 0) {
            size *= context.inputs[0].dims[k];
          }
        }
        return [
          "var sum = f32(0);",
          "",
          `sum += f32(${input.getByIndices("input_indices")});`,
          `let value = ${output.type.value}(sum / ${size});`
        ];
      };
      runReduceProgram(context, "ReduceMean", attributes, reduceOp);
    };
    reduceMinNaive = (context, attributes) => {
      validateInputs2(context.inputs);
      const reduceOp = (input, _output, axes) => {
        const idxZero = [];
        for (let k = 0; k < input.rank; k++) {
          if (axes.indexOf(k) >= 0 || axes.length === 0) {
            idxZero.push(`input_indices[${k}] = 0;`);
          }
        }
        return [
          `${idxZero.join("\n")}`,
          `var value = ${input.getByIndices("input_indices")};`,
          `value = min(value, ${input.getByIndices("input_indices")});`,
          ""
        ];
      };
      runReduceProgram(context, "ReduceMin", attributes, reduceOp);
    };
    reduceProdNaive = (context, attributes) => {
      validateInputs2(context.inputs);
      const reduceOp = (input, output) => [
        `var value = ${output.type.storage}(1);`,
        "",
        `value *= ${input.getByIndices("input_indices")};`,
        ""
      ];
      runReduceProgram(context, "ReduceProd", attributes, reduceOp);
    };
    reduceSumNaive = (context, attributes) => {
      validateInputs2(context.inputs);
      const reduceOp = (input, output) => [
        `var value = ${output.type.storage}(0);`,
        "",
        `value += ${input.getByIndices("input_indices")};`,
        ""
      ];
      runReduceProgram(context, "ReduceSum", attributes, reduceOp);
    };
    reduceSumSquareNaive = (context, attributes) => {
      validateInputs2(context.inputs);
      const reduceOp = (input, output) => [
        `var t = ${output.type.value}(0); var value = ${output.type.value}(0);`,
        "",
        `t = ${input.getByIndices("input_indices")}; value += t * t;`,
        ""
      ];
      runReduceProgram(context, "ReduceSumSquare", attributes, reduceOp);
    };
    useNaiveReduceMethod = (shape, axes, noopWithEmptyAxes) => {
      if (axes.length === 0) {
        return noopWithEmptyAxes;
      }
      let outputSize = 1;
      let reduceSize = 1;
      for (let dim = 0; dim < axes.length; dim++) {
        if (axes.indexOf(dim) === -1) {
          outputSize *= shape[dim];
        } else {
          reduceSize *= shape[dim];
        }
      }
      return reduceSize < 32 && outputSize > 1024;
    };
    reduceMean = (context, attributes) => {
      if (useNaiveReduceMethod(context.inputs[0].dims, attributes.axes, attributes.noopWithEmptyAxes)) {
        reduceMeanNaive(context, attributes);
      } else {
        reduceMeanShared(context, attributes);
      }
    };
    reduceL1 = (context, attributes) => {
      if (useNaiveReduceMethod(context.inputs[0].dims, attributes.axes, attributes.noopWithEmptyAxes)) {
        reduceL1Naive(context, attributes);
      } else {
        reduceL1Shared(context, attributes);
      }
    };
    reduceL2 = (context, attributes) => {
      if (useNaiveReduceMethod(context.inputs[0].dims, attributes.axes, attributes.noopWithEmptyAxes)) {
        reduceL2Naive(context, attributes);
      } else {
        reduceL2Shared(context, attributes);
      }
    };
    reduceLogSumExp = (context, attributes) => {
      if (useNaiveReduceMethod(context.inputs[0].dims, attributes.axes, attributes.noopWithEmptyAxes)) {
        reduceLogSumExpNaive(context, attributes);
      } else {
        reduceLogSumExpShared(context, attributes);
      }
    };
    reduceMax = (context, attributes) => {
      if (useNaiveReduceMethod(context.inputs[0].dims, attributes.axes, attributes.noopWithEmptyAxes)) {
        reduceMaxNaive(context, attributes);
      } else {
        reduceMaxShared(context, attributes);
      }
    };
    reduceMin = (context, attributes) => {
      if (useNaiveReduceMethod(context.inputs[0].dims, attributes.axes, attributes.noopWithEmptyAxes)) {
        reduceMinNaive(context, attributes);
      } else {
        reduceMinShared(context, attributes);
      }
    };
    reduceProd = (context, attributes) => {
      if (useNaiveReduceMethod(context.inputs[0].dims, attributes.axes, attributes.noopWithEmptyAxes)) {
        reduceProdNaive(context, attributes);
      } else {
        reduceProdShared(context, attributes);
      }
    };
    reduceSum = (context, attributes) => {
      if (useNaiveReduceMethod(context.inputs[0].dims, attributes.axes, attributes.noopWithEmptyAxes)) {
        reduceSumNaive(context, attributes);
      } else {
        reduceSumShared(context, attributes);
      }
    };
    reduceSumSquare = (context, attributes) => {
      if (useNaiveReduceMethod(context.inputs[0].dims, attributes.axes, attributes.noopWithEmptyAxes)) {
        reduceSumSquareNaive(context, attributes);
      } else {
        reduceSumSquareShared(context, attributes);
      }
    };
    reduceLogSum = (context, attributes) => {
      if (useNaiveReduceMethod(context.inputs[0].dims, attributes.axes, attributes.noopWithEmptyAxes)) {
        reduceLogSumNaive(context, attributes);
      } else {
        reduceLogSumShared(context, attributes);
      }
    };
  }
});

// web/lib/wasm/jsep/webgpu/ops/argminmax.ts
var validateInputs3, argMin, argMax, parseArgMinMaxAttributes;
var init_argminmax = __esm({
  "web/lib/wasm/jsep/webgpu/ops/argminmax.ts"() {
    "use strict";
    init_wasm_common();
    init_attribute_with_cache_key();
    init_reduce();
    validateInputs3 = (inputs) => {
      if (!inputs || inputs.length === 0 || inputs.length > 2) {
        throw new Error("ArgMinMaxOp op requires 1 or 2 inputs.");
      }
      if (inputs[0].dataType !== 1 /* float */) {
        throw new Error("Invalid input type.");
      }
    };
    argMin = (context, attributes) => {
      validateInputs3(context.inputs);
      const argMinMaxOp = (input, output, axes) => {
        const idxZero = [];
        for (let k = 0; k < input.rank; k++) {
          if (axes.indexOf(k) >= 0 || axes.length === 0) {
            idxZero.push(`input_indices[${k}] = 0;`);
          }
        }
        return [
          `${idxZero.join("\n")}`,
          `var value = ${input.getByIndices("input_indices")};
var best_index : i32 = 0;`,
          `if (${input.getByIndices("input_indices")} ${attributes.selectLastIndex > 0 ? "<=" : "<"} value) {
         value = ${input.getByIndices("input_indices")};
         best_index = i32(last_index);
       }`,
          "",
          output.setByOffset("global_idx", "best_index")
        ];
      };
      context.compute(
        createReduceProgramInfo(
          "ArgMin",
          { hint: attributes.cacheKey, inputDependencies: ["rank"] },
          [context.inputs[0]],
          argMinMaxOp,
          [attributes.axis],
          7 /* int64 */,
          attributes.keepDims
        ),
        { inputs: [0] }
      );
    };
    argMax = (context, attributes) => {
      validateInputs3(context.inputs);
      const argMinMaxOp = (input, output, axes) => {
        const idxZero = [];
        for (let k = 0; k < input.rank; k++) {
          if (axes.indexOf(k) >= 0 || axes.length === 0) {
            idxZero.push(`input_indices[${k}] = 0;`);
          }
        }
        return [
          `${idxZero.join("\n")}`,
          `var value = ${input.getByIndices("input_indices")};
var best_index : i32 = 0;`,
          `if (${input.getByIndices("input_indices")} ${attributes.selectLastIndex > 0 ? ">=" : ">"} value) {
         value = ${input.getByIndices("input_indices")};
         best_index = i32(last_index);
       }`,
          "",
          output.setByOffset("global_idx", "best_index")
        ];
      };
      context.compute(
        createReduceProgramInfo(
          "argMax",
          { hint: attributes.cacheKey, inputDependencies: ["rank"] },
          [context.inputs[0]],
          argMinMaxOp,
          [attributes.axis],
          7 /* int64 */,
          attributes.keepDims
        ),
        { inputs: [0] }
      );
    };
    parseArgMinMaxAttributes = (attributes) => createAttributeWithCacheKey(attributes);
  }
});

// web/lib/wasm/jsep/webgpu/ops/attention.ts
var validateAttentionInputs, createInPlaceSoftmaxProgramInfo, createAttentionProbsProgramInfo, createVxAttentionScoreProgramInfo, applyAttention, prepare, attention;
var init_attention = __esm({
  "web/lib/wasm/jsep/webgpu/ops/attention.ts"() {
    "use strict";
    init_wasm_common();
    init_util();
    init_types();
    init_common();
    validateAttentionInputs = (inputs, attributes) => {
      const input = inputs[0];
      const weights = inputs[1];
      const bias = inputs[2];
      const maskIndex = inputs[3];
      const past = inputs[4];
      const attentionBias = inputs[5];
      if (past && attentionBias) {
        throw new Error("Attention cannot have both past and attention_bias");
      }
      if (input.dims.length !== 3) {
        throw new Error('Input "input" must have 3 dimensions');
      }
      const batchSize = input.dims[0];
      const sequenceLength = input.dims[1];
      const inputHiddenSize = input.dims[2];
      if (bias.dims.length !== 1) {
        throw new Error('Input "bias" is expected to have 1 dimensions');
      }
      if (weights.dims.length !== 2) {
        throw new Error('Input "weights" is expected to have 2 dimensions');
      }
      if (weights.dims[0] !== inputHiddenSize) {
        throw new Error("Input 1 dimension 0 should have same length as dimension 2 of input 0");
      }
      if (bias.dims[0] !== weights.dims[1]) {
        throw new Error('Input "bias" dimension 0 should have same length as dimension 1 of input "weights"');
      }
      let qHiddenSize = bias.dims[0] / 3;
      let kHiddenSize = qHiddenSize;
      let vHiddenSize = kHiddenSize;
      if (attributes.qkvHiddenSizes.length > 0) {
        if (attributes.qkvHiddenSizes.length !== 3) {
          throw new Error("qkv_hidden_sizes attribute should have 3 elements");
        }
        for (const sz of attributes.qkvHiddenSizes) {
          if (sz % attributes.numHeads !== 0) {
            throw new Error("qkv_hidden_sizes should be divisible by num_heads");
          }
        }
        qHiddenSize = attributes.qkvHiddenSizes[0];
        kHiddenSize = attributes.qkvHiddenSizes[1];
        vHiddenSize = attributes.qkvHiddenSizes[2];
      }
      const kvSequenceLength = sequenceLength;
      if (qHiddenSize !== kHiddenSize) {
        throw new Error("qkv_hidden_sizes first element should be same as the second");
      }
      if (bias.dims[0] !== qHiddenSize + kHiddenSize + vHiddenSize) {
        throw new Error('Input "bias" dimension 0 should have same length as sum of Q/K/V hidden sizes');
      }
      let pastSequenceLength = 0;
      if (past) {
        if (kHiddenSize !== vHiddenSize) {
          throw new Error('Input "past" expect k_hidden_size == v_hidden_size');
        }
        if (past.dims.length !== 5) {
          throw new Error('Input "past" must have 5 dimensions');
        }
        if (past.dims[0] !== 2) {
          throw new Error('Input "past" first dimension must be 2');
        }
        if (past.dims[1] !== batchSize) {
          throw new Error('Input "past" second dimension must be batch_size');
        }
        if (past.dims[2] !== attributes.numHeads) {
          throw new Error('Input "past" third dimension must be num_heads');
        }
        if (past.dims[4] !== kHiddenSize / attributes.numHeads) {
          throw new Error('Input "past" fifth dimension must be k_hidden_size / num_heads');
        }
        if (!attributes.pastPresentShareBuffer) {
          pastSequenceLength = past.dims[3];
        }
      }
      const totalSequenceLength = kvSequenceLength + pastSequenceLength;
      const maxSequenceLength = -1;
      const maskType = 0 /* none */;
      if (maskIndex) {
        throw new Error("Mask not supported");
      }
      if (past) {
        throw new Error("past is not supported");
      }
      if (attentionBias) {
        if (attentionBias.dims.length !== 4) {
          throw new Error('Input "attention_bias" must have 4 dimensions');
        }
        if (attentionBias.dims[0] !== batchSize || attentionBias.dims[1] !== attributes.numHeads || attentionBias.dims[2] !== sequenceLength || attentionBias.dims[3] !== totalSequenceLength) {
          throw new Error('Expect "attention_bias" shape (batch_size, num_heads, sequence_length, total_sequence_length)');
        }
      }
      return {
        batchSize,
        sequenceLength,
        pastSequenceLength,
        kvSequenceLength,
        totalSequenceLength,
        maxSequenceLength,
        inputHiddenSize,
        hiddenSize: qHiddenSize,
        vHiddenSize,
        headSize: Math.floor(qHiddenSize / attributes.numHeads),
        vHeadSize: Math.floor(vHiddenSize / attributes.numHeads),
        numHeads: attributes.numHeads,
        isUnidirectional: false,
        pastPresentShareBuffer: false,
        maskFilterValue: attributes.maskFilterValue,
        maskType,
        scale: attributes.scale,
        broadcastResPosBias: false,
        passPastInKv: false,
        qkvFormat: 1 /* qkvBNSH */
      };
    };
    createInPlaceSoftmaxProgramInfo = (input, n, d) => {
      const components = getMaxComponents(d);
      let WG = 64;
      const dComp = d / components;
      if (dComp < WG) {
        WG = 32;
      }
      const elementsPerThread = Math.ceil(d / components / WG);
      const programUniforms = [
        { type: 1 /* float */, data: 1 / d },
        { type: 12 /* uint32 */, data: dComp },
        { type: 12 /* uint32 */, data: elementsPerThread }
      ];
      const dataType = tensorTypeToWsglStorageType(input.dataType, components);
      const f32Type = tensorTypeToWsglValueType(1 /* float */, components);
      const inputDependencies = ["type"];
      const getShaderSource = (shaderHelper) => {
        const inputHelper = outputVariable("x", input.dataType, input.dims, components);
        const elemValueType = tensorTypeToWsglValueType(input.dataType);
        const uniforms = [
          { name: "d_inv", type: "f32" },
          { name: "d_comp", type: "u32" },
          { name: "elements_per_thread", type: "u32" }
        ];
        return `
  var<workgroup> thread_max: array<f32, ${WG}>;
  var<workgroup> thread_sum: array<f32, ${WG}>;
  ${shaderHelper.registerUniforms(uniforms).declareVariables(inputHelper)}
  ${shaderHelper.mainStart([WG, 1, 1])}
    let local_offset = local_idx * uniforms.elements_per_thread;
    let offset = (global_idx / ${WG}) * uniforms.d_comp + local_offset;

    var thread_max_vector = ${f32Type}(-3.402823e+38f);
    for (var i: u32 = 0; i < uniforms.elements_per_thread && i + local_offset < uniforms.d_comp; i++) {
      thread_max_vector = max(${f32Type}(x[offset + i]), thread_max_vector);
    }
    thread_max[local_idx] = ${(() => {
          switch (components) {
            case 1:
              return "thread_max_vector";
            case 2:
              return "max(thread_max_vector.x, thread_max_vector.y)";
            case 4:
              return "max(max(thread_max_vector.x, thread_max_vector.y), max(thread_max_vector.z, thread_max_vector.w))";
            default:
              throw new Error(`Unsupported components: ${components}`);
          }
        })()};
    workgroupBarrier();

    var max_value =  f32(-3.402823e+38f);
    for (var i = 0u; i < ${WG}; i++) {
      max_value = max(thread_max[i], max_value);
    }

    var sum_vector = ${f32Type}(0);
    for (var i: u32 = 0; i < uniforms.elements_per_thread && i + local_offset < uniforms.d_comp; i++) {
      sum_vector += exp(${f32Type}(x[offset + i]) - max_value);
    }
    thread_sum[local_idx] = ${(() => {
          switch (components) {
            case 1:
              return "sum_vector";
            case 2:
              return "sum_vector.x + sum_vector.y";
            case 4:
              return "sum_vector.x + sum_vector.y + sum_vector.z + sum_vector.w";
            default:
              throw new Error(`Unsupported components: ${components}`);
          }
        })()};
    workgroupBarrier();

    var sum: f32 = 0;
    for (var i = 0u; i < ${WG}; i++) {
      sum += thread_sum[i];
    }

    if (sum == 0) {
      for (var i: u32 = 0; i < uniforms.elements_per_thread && i + local_offset < uniforms.d_comp; i++) {
        x[offset + i] = ${inputHelper.type.value}(${elemValueType}(uniforms.d_inv));
      }
    } else {
      for (var i: u32 = 0; i < uniforms.elements_per_thread && i + local_offset < uniforms.d_comp; i++) {
        var f32input = ${f32Type}(x[offset + i]);
        x[offset + i] = ${inputHelper.type.value}(exp(f32input - max_value) / sum);
      }
    }
  }`;
      };
      return {
        name: "AttentionProbsSoftmax",
        shaderCache: { hint: `${WG};${dataType};${components}`, inputDependencies },
        getShaderSource,
        getRunData: () => ({ outputs: [], dispatchGroup: { x: n }, programUniforms })
      };
    };
    createAttentionProbsProgramInfo = (outputCount, q, key, pastKey, attentionBias, parameters, attributes, pastSequenceLength) => {
      const totalSequenceLength = pastSequenceLength + parameters.kvSequenceLength;
      const probsShape = [parameters.batchSize, parameters.numHeads, parameters.sequenceLength, totalSequenceLength];
      const presentKey = parameters.kvNumHeads === void 0 && outputCount > 1 && pastKey;
      const presentKeyShape = presentKey ? [parameters.batchSize, parameters.numHeads, totalSequenceLength, parameters.headSize] : void 0;
      const alpha = attributes.scale === 0 ? 1 / Math.sqrt(parameters.headSize) : attributes.scale;
      const components = getMaxComponents(parameters.headSize);
      const vectorizedHeadSize = parameters.headSize / components;
      const TILE_SIZE = 12;
      const dispatch = {
        x: Math.ceil(totalSequenceLength / TILE_SIZE),
        y: Math.ceil(parameters.sequenceLength / TILE_SIZE),
        z: parameters.batchSize * parameters.numHeads
      };
      const programUniforms = [
        { type: 12 /* uint32 */, data: parameters.sequenceLength },
        { type: 12 /* uint32 */, data: vectorizedHeadSize },
        { type: 12 /* uint32 */, data: totalSequenceLength },
        { type: 12 /* uint32 */, data: parameters.numHeads },
        { type: 1 /* float */, data: alpha },
        { type: 12 /* uint32 */, data: pastSequenceLength },
        { type: 12 /* uint32 */, data: parameters.kvSequenceLength }
      ];
      const feedPastKey = presentKey && pastKey && ShapeUtil.size(pastKey.dims) > 0;
      const inputDependencies = ["type", "type"];
      if (feedPastKey) {
        inputDependencies.push("type");
      }
      if (attentionBias) {
        inputDependencies.push("type");
      }
      const outputs = [{ dims: probsShape, dataType: q.dataType, gpuDataType: 0 /* default */ }];
      if (presentKey) {
        outputs.push({ dims: presentKeyShape, dataType: q.dataType, gpuDataType: 0 /* default */ });
      }
      const getShaderSource = (shaderHelper) => {
        const qInput = inputVariable("q", q.dataType, q.dims, components);
        const kInput = inputVariable("key", key.dataType, key.dims, components);
        const inputVars = [qInput, kInput];
        if (feedPastKey) {
          const pastKeyInput = inputVariable("past_key", pastKey.dataType, pastKey.dims, components);
          inputVars.push(pastKeyInput);
        }
        if (attentionBias) {
          inputVars.push(inputVariable("attention_bias", attentionBias.dataType, attentionBias.dims));
        }
        const output = outputVariable("output", q.dataType, probsShape);
        const outputVars = [output];
        if (presentKey) {
          outputVars.push(outputVariable("present_key", q.dataType, presentKeyShape, components));
        }
        const f32Type = tensorTypeToWsglValueType(1 /* float */, components);
        const uniforms = [
          { name: "M", type: "u32" },
          { name: "K", type: "u32" },
          { name: "N", type: "u32" },
          { name: "num_heads", type: "u32" },
          { name: "alpha", type: "f32" },
          { name: "past_sequence_length", type: "u32" },
          { name: "kv_sequence_length", type: "u32" }
        ];
        return `
  const TILE_SIZE = ${TILE_SIZE}u;

  var<workgroup> tileQ: array<${qInput.type.storage}, ${TILE_SIZE * TILE_SIZE}>;
  var<workgroup> tileK: array<${qInput.type.storage}, ${TILE_SIZE * TILE_SIZE}>;
  ${shaderHelper.registerUniforms(uniforms).declareVariables(...inputVars, ...outputVars)}
  ${shaderHelper.mainStart([TILE_SIZE, TILE_SIZE, 1])}
    // x holds the N and y holds the M
    let headIdx = workgroup_id.z;
    let m = workgroup_id.y * TILE_SIZE;
    let n = workgroup_id.x * TILE_SIZE;
    let qOffset = uniforms.M * uniforms.K * headIdx + m * uniforms.K;
    ${(() => {
          if (feedPastKey && presentKey) {
            return `
    let kOffset = uniforms.kv_sequence_length * uniforms.K * headIdx;
    let pastKeyOffset = uniforms.past_sequence_length * uniforms.K * headIdx;`;
          } else {
            return `
    let kOffset = uniforms.N * uniforms.K * headIdx + n * uniforms.K;`;
          }
        })()}
    ${presentKey ? "let presentKeyOffset = headIdx * uniforms.N * uniforms.K;" : ""}
    var value = ${f32Type}(0);
    for (var w: u32 = 0u; w < uniforms.K; w += TILE_SIZE) {
      if (global_id.y < uniforms.M && w + local_id.x < uniforms.K) {
        tileQ[TILE_SIZE * local_id.y + local_id.x] = q[qOffset + local_id.y * uniforms.K + w + local_id.x];
      }
      if (n + local_id.y < uniforms.N && w + local_id.x < uniforms.K) {
        var idx = TILE_SIZE * local_id.y + local_id.x;
      ${(() => {
          if (feedPastKey && presentKey) {
            return `
              if (n + local_id.y < uniforms.past_sequence_length) {
                tileK[idx] = past_key[pastKeyOffset + (n + local_id.y) * uniforms.K + w + local_id.x];
              } else {
                tileK[idx] =
                         key[kOffset + (n + local_id.y - uniforms.past_sequence_length) * uniforms.K + w + local_id.x];
              }`;
          } else {
            return "tileK[idx] = key[kOffset + local_id.y * uniforms.K + w + local_id.x];";
          }
        })()}
      ${presentKey ? "present_key[presentKeyOffset + (n + local_id.y) * uniforms.K + w + local_id.x] = tileK[idx];" : ""}
      }
      workgroupBarrier();

      for (var k: u32 = 0u; k < TILE_SIZE && w+k < uniforms.K; k++) {
        value += ${f32Type}(tileQ[TILE_SIZE * local_id.y + k] * tileK[TILE_SIZE * local_id.x + k]);
      }

      workgroupBarrier();
    }

    let headOffset = headIdx * uniforms.M * uniforms.N;
    if (global_id.y < uniforms.M && global_id.x < uniforms.N) {
      let outputIdx = headOffset + global_id.y * uniforms.N + global_id.x;
      var sum: f32 = ${(() => {
          switch (components) {
            case 1:
              return "value";
            case 2:
              return "value.x + value.y";
            case 4:
              return "value.x + value.y + value.z + value.w";
            default:
              throw new Error(`Unsupported components: ${components}`);
          }
        })()};
        output[outputIdx] = ${output.type.value} (sum * uniforms.alpha) + ${attentionBias ? "attention_bias[outputIdx]" : "0.0"};
    }
  }`;
      };
      return {
        name: "AttentionProbs",
        shaderCache: {
          hint: `${components};${attentionBias !== void 0};${pastKey !== void 0};${outputCount}`,
          inputDependencies
        },
        getRunData: () => ({ outputs, dispatchGroup: dispatch, programUniforms }),
        getShaderSource
      };
    };
    createVxAttentionScoreProgramInfo = (outputCount, probs, v, pastValue, params, pastSequenceLength) => {
      const totalSequenceLength = pastSequenceLength + params.kvSequenceLength;
      const nReps = params.nReps ? params.nReps : 1;
      const repeatedVHiddenSize = params.vHiddenSize * nReps;
      const presentValue = params.kvNumHeads == null && outputCount > 1 && pastValue;
      const presentValueShape = presentValue ? [params.batchSize, params.numHeads, totalSequenceLength, params.headSize] : void 0;
      const outputShape = [params.batchSize, params.sequenceLength, repeatedVHiddenSize];
      const TILE_SIZE = 12;
      const dispatch = {
        x: Math.ceil(params.vHeadSize / TILE_SIZE),
        y: Math.ceil(params.sequenceLength / TILE_SIZE),
        z: params.batchSize * params.numHeads
      };
      const programUniforms = [
        { type: 12 /* uint32 */, data: params.sequenceLength },
        { type: 12 /* uint32 */, data: totalSequenceLength },
        { type: 12 /* uint32 */, data: params.vHeadSize },
        { type: 12 /* uint32 */, data: params.numHeads },
        { type: 12 /* uint32 */, data: repeatedVHiddenSize },
        { type: 12 /* uint32 */, data: pastSequenceLength },
        { type: 12 /* uint32 */, data: params.kvSequenceLength }
      ];
      const feedPastValue = presentValue && pastValue && ShapeUtil.size(pastValue.dims) > 0;
      const inputDependencies = ["type", "type"];
      if (feedPastValue) {
        inputDependencies.push("type");
      }
      const outputs = [{ dims: outputShape, dataType: probs.dataType, gpuDataType: 0 /* default */ }];
      if (presentValue) {
        outputs.push({ dims: presentValueShape, dataType: probs.dataType, gpuDataType: 0 /* default */ });
      }
      const getShaderSource = (shaderHelper) => {
        const probsHelper = inputVariable("probs", probs.dataType, probs.dims);
        const vHelper = inputVariable("v", v.dataType, v.dims);
        const inputVars = [probsHelper, vHelper];
        if (feedPastValue) {
          inputVars.push(inputVariable("past_value", pastValue.dataType, pastValue.dims));
        }
        const output = outputVariable("output", probs.dataType, outputShape);
        const outputVars = [output];
        if (presentValue) {
          outputVars.push(outputVariable("present_value", probs.dataType, presentValueShape));
        }
        const uniforms = [
          { name: "M", type: "u32" },
          { name: "K", type: "u32" },
          { name: "N", type: "u32" },
          { name: "num_heads", type: "u32" },
          { name: "v_hidden_size", type: "u32" },
          { name: "past_sequence_length", type: "u32" },
          { name: "kv_sequence_length", type: "u32" }
        ];
        return `
  const TILE_SIZE = ${TILE_SIZE}u;
  var<workgroup> tileQ: array<${probsHelper.type.value}, ${TILE_SIZE * TILE_SIZE}>;
  var<workgroup> tileK: array<${probsHelper.type.value}, ${TILE_SIZE * TILE_SIZE}>;
  ${shaderHelper.registerUniforms(uniforms).declareVariables(...inputVars, ...outputVars)}
  ${shaderHelper.mainStart([TILE_SIZE, TILE_SIZE, 1])}
   let headIdx = workgroup_id.z;
   let m = global_id.y;
   let n = global_id.x;

   let offsetA = headIdx * (uniforms.M * uniforms.K) + m * uniforms.K;
   ${(() => {
          if (feedPastValue && presentValue) {
            return `
    let pastValueOffset = headIdx * uniforms.N * uniforms.past_sequence_length + n;
    let vOffset = headIdx * uniforms.N * uniforms.kv_sequence_length + n;
      `;
          } else {
            return `
   let offsetB = headIdx * uniforms.N * uniforms.K + n;
            `;
          }
        })()}
    ${presentValue ? "let presentValueOffset = headIdx * uniforms.N * uniforms.K + n;" : ""}
   var value = ${probsHelper.type.storage}(0);
   for (var w: u32 = 0u; w < uniforms.K; w += TILE_SIZE) {
      if (m < uniforms.M && w + local_id.x < uniforms.K) {
        tileQ[TILE_SIZE * local_id.y + local_id.x] = probs[offsetA + w + local_id.x];
      }
      if (n < uniforms.N && w + local_id.y < uniforms.K) {
        var idx = TILE_SIZE * local_id.y + local_id.x;
        ${(() => {
          if (feedPastValue && presentValue) {
            return `
        if (w + local_id.y < uniforms.past_sequence_length) {
          tileK[idx] = past_value[pastValueOffset + (w + local_id.y) * uniforms.N];
        } else {
          tileK[idx] = v[vOffset + (w + local_id.y - uniforms.past_sequence_length) * uniforms.N];
        }
      `;
          } else {
            return `
        tileK[idx] = v[offsetB + (w + local_id.y) * uniforms.N];
      `;
          }
        })()}
        ${presentValue ? "present_value[presentValueOffset + (w + local_id.y) * uniforms.N] = tileK[idx];" : ""}
      }
     workgroupBarrier();
     for (var k: u32 = 0u; k < TILE_SIZE && w+k < uniforms.K; k++) {
       value += tileQ[TILE_SIZE * local_id.y + k] * tileK[TILE_SIZE * k + local_id.x];
     }
     workgroupBarrier();
   }

   // we need to transpose output from BNSH_v to BSND_v
   let batchIdx = workgroup_id.z / uniforms.num_heads;
   let currentBatchHeadNumber = workgroup_id.z % uniforms.num_heads;
   if (m < uniforms.M && n < uniforms.N) {
     let outputIdx = batchIdx * uniforms.M * uniforms.v_hidden_size + m * uniforms.v_hidden_size
       + currentBatchHeadNumber * uniforms.N + n;
     output[outputIdx] = value;
   }
  }`;
      };
      return {
        name: "AttentionScore",
        shaderCache: { hint: `${pastValue !== void 0};${outputCount}`, inputDependencies },
        getRunData: () => ({ outputs, dispatchGroup: dispatch, programUniforms }),
        getShaderSource
      };
    };
    applyAttention = (context, q, k, v, _maskIndex, _past, pastKey, pastValue, attentionBiasInput, parameters, attributes) => {
      const outputCount = Math.min(context.outputCount, 1 + (pastKey ? 1 : 0) + (pastValue ? 1 : 0));
      const pastSequenceLength = parameters.kvNumHeads !== void 0 || outputCount > 1 ? parameters.pastSequenceLength : 0;
      const totalSequenceLength = pastSequenceLength + parameters.kvSequenceLength;
      const attentionBias = attentionBiasInput && ShapeUtil.size(attentionBiasInput.dims) > 0 ? attentionBiasInput : void 0;
      const inputsK = [q, k];
      if (parameters.kvNumHeads === void 0 && outputCount > 1 && pastKey && ShapeUtil.size(pastKey.dims) > 0) {
        inputsK.push(pastKey);
      }
      if (attentionBias) {
        inputsK.push(attentionBias);
      }
      const probs = context.compute(
        createAttentionProbsProgramInfo(
          outputCount,
          q,
          k,
          pastKey,
          attentionBias,
          parameters,
          attributes,
          pastSequenceLength
        ),
        { inputs: inputsK, outputs: parameters.kvNumHeads === void 0 && outputCount > 1 ? [-1, 1] : [-1] }
      )[0];
      context.compute(
        createInPlaceSoftmaxProgramInfo(
          probs,
          parameters.batchSize * parameters.numHeads * parameters.sequenceLength,
          totalSequenceLength
        ),
        { inputs: [probs], outputs: [] }
      );
      const inputsV = [probs, v];
      if (parameters.kvNumHeads === void 0 && outputCount > 1 && pastValue && ShapeUtil.size(pastValue.dims) > 0) {
        inputsV.push(pastValue);
      }
      context.compute(createVxAttentionScoreProgramInfo(outputCount, probs, v, pastValue, parameters, pastSequenceLength), {
        inputs: inputsV,
        outputs: parameters.kvNumHeads === void 0 && outputCount > 1 ? [0, 2] : [0]
      });
    };
    prepare = (context, parameters) => {
      const outputShape = [parameters.batchSize, parameters.numHeads, parameters.sequenceLength, parameters.headSize];
      const M = parameters.sequenceLength;
      const K = parameters.inputHiddenSize;
      const N = parameters.headSize;
      const TILE_SIZE = 12;
      const dispatch = {
        x: Math.ceil(parameters.headSize / TILE_SIZE),
        y: Math.ceil(parameters.sequenceLength / TILE_SIZE),
        z: parameters.batchSize * parameters.numHeads
      };
      const inputs = [context.inputs[0], context.inputs[1], context.inputs[2]];
      const programUniforms = [
        { type: 12 /* uint32 */, data: M },
        { type: 12 /* uint32 */, data: K },
        { type: 12 /* uint32 */, data: N },
        { type: 12 /* uint32 */, data: parameters.numHeads },
        { type: 12 /* uint32 */, data: parameters.headSize },
        { type: 12 /* uint32 */, data: parameters.hiddenSize },
        { type: 12 /* uint32 */, data: parameters.hiddenSize + parameters.hiddenSize + parameters.vHiddenSize }
      ];
      const getShaderSource = (shaderHelper) => {
        const outputQ = outputVariable("output_q", inputs[0].dataType, outputShape);
        const outputK = outputVariable("output_k", inputs[0].dataType, outputShape);
        const outputV = outputVariable("output_v", inputs[0].dataType, outputShape);
        const input = inputVariable("input", inputs[0].dataType, inputs[0].dims);
        const weight = inputVariable("weight", inputs[1].dataType, inputs[1].dims);
        const bias = inputVariable("bias", inputs[2].dataType, inputs[2].dims);
        const dataType = input.type.storage;
        const uniforms = [
          { name: "M", type: "u32" },
          { name: "K", type: "u32" },
          { name: "N", type: "u32" },
          { name: "num_heads", type: "u32" },
          { name: "head_size", type: "u32" },
          { name: "hidden_size", type: "u32" },
          { name: "ldb", type: "u32" }
        ];
        return `
  const TILE_SIZE = ${TILE_SIZE}u;
  var<workgroup> tileInput: array<${dataType}, ${TILE_SIZE * TILE_SIZE}>;
  var<workgroup> tileWeightQ: array<${dataType}, ${TILE_SIZE * TILE_SIZE}>;
  var<workgroup> tileWeightK: array<${dataType}, ${TILE_SIZE * TILE_SIZE}>;
  var<workgroup> tileWeightV: array<${dataType}, ${TILE_SIZE * TILE_SIZE}>;
  ${shaderHelper.registerUniforms(uniforms).declareVariables(input, weight, bias, outputQ, outputK, outputV)}
  ${shaderHelper.mainStart([TILE_SIZE, TILE_SIZE, 1])}
    let batchIndex = workgroup_id.z / uniforms.num_heads;
    let headNumber = workgroup_id.z % uniforms.num_heads;
    let m = global_id.y;
    let n = global_id.x;

    let inputOffset = batchIndex * (uniforms.M * uniforms.K) + m * uniforms.K;
    let biasOffsetQ = headNumber * uniforms.head_size;
    let biasOffsetK = uniforms.hidden_size + biasOffsetQ;
    let biasOffsetV = uniforms.hidden_size + biasOffsetK;

    var valueQ = ${dataType}(0);
    var valueK = ${dataType}(0);
    var valueV = ${dataType}(0);
    for (var w: u32 = 0u; w < uniforms.K; w += TILE_SIZE) {
      if (m < uniforms.M && w + local_id.x < uniforms.K) {
        tileInput[TILE_SIZE * local_id.y + local_id.x] = input[inputOffset + w + local_id.x];
      }
      if (n < uniforms.N && w + local_id.y < uniforms.K) {
        let offset = n + (w + local_id.y) * uniforms.ldb;
        tileWeightQ[TILE_SIZE * local_id.y + local_id.x] = weight[biasOffsetQ + offset];
        tileWeightK[TILE_SIZE * local_id.y + local_id.x] = weight[biasOffsetK + offset];
        tileWeightV[TILE_SIZE * local_id.y + local_id.x] = weight[biasOffsetV + offset];
      }
      workgroupBarrier();
      for (var k: u32 = 0u; k<TILE_SIZE && w+k < uniforms.K; k++) {
        let inputTileOffset = TILE_SIZE * local_id.y + k;
        let weightTileOffset = TILE_SIZE * k + local_id.x;
        valueQ += tileInput[inputTileOffset] * tileWeightQ[weightTileOffset];
        valueK += tileInput[inputTileOffset] * tileWeightK[weightTileOffset];
        valueV += tileInput[inputTileOffset] * tileWeightV[weightTileOffset];
      }

      workgroupBarrier();
    }

    let headOffset = (m * uniforms.N + n) % uniforms.head_size;
    valueQ += bias[headOffset + biasOffsetQ];
    valueK += bias[headOffset + biasOffsetK];
    valueV += bias[headOffset + biasOffsetV];

    let offset = workgroup_id.z * uniforms.M * uniforms.N;
    if (m < uniforms.M && n < uniforms.N) {
      let outputIdx = offset + m * uniforms.N + n;
      output_q[outputIdx] = valueQ;
      output_k[outputIdx] = valueK;
      output_v[outputIdx] = valueV;
    }
  }`;
      };
      return context.compute(
        {
          name: "AttentionPrepare",
          shaderCache: { inputDependencies: ["type", "type", "type"] },
          getRunData: () => ({
            outputs: [
              { dims: outputShape, dataType: context.inputs[0].dataType, gpuDataType: 0 /* default */ },
              { dims: outputShape, dataType: context.inputs[0].dataType, gpuDataType: 0 /* default */ },
              { dims: outputShape, dataType: context.inputs[0].dataType, gpuDataType: 0 /* default */ }
            ],
            dispatchGroup: dispatch,
            programUniforms
          }),
          getShaderSource
        },
        { inputs, outputs: [-1, -1, -1] }
      );
    };
    attention = (context, attributes) => {
      const params = validateAttentionInputs(context.inputs, attributes);
      const [q, k, v] = prepare(context, params);
      return applyAttention(
        context,
        q,
        k,
        v,
        context.inputs[4],
        void 0,
        void 0,
        void 0,
        context.inputs[5],
        params,
        attributes
      );
    };
  }
});

// web/lib/wasm/jsep/webgpu/ops/batch-norm.ts
var validateInputs4, createBatchNormInferenceProgramInfo, parseBatchNormAttributes, batchNorm;
var init_batch_norm = __esm({
  "web/lib/wasm/jsep/webgpu/ops/batch-norm.ts"() {
    "use strict";
    init_esm();
    init_wasm_common();
    init_util();
    init_attribute_with_cache_key();
    init_common();
    validateInputs4 = (inputs, attributes) => {
      if (!inputs || inputs.length !== 5) {
        throw new Error("BatchNormalization requires 5 inputs");
      }
      const checkShapeEqual = (actual, expected, message) => {
        const r = expected.length;
        if (r !== actual.length) {
          throw new Error(`${message}: num dimensions != ${r}`);
        }
        expected.forEach((v, i) => {
          if (v !== actual[i]) {
            throw new Error(`${message}: dim[${i}] do not match`);
          }
        });
      };
      if (inputs[0].dims.length > 1) {
        const shape = attributes.format === "NHWC" ? attributes.spatial ? inputs[0].dims.slice(-1) : inputs[0].dims.slice(-1).concat(inputs[0].dims.slice(1, inputs[0].dims.length - 1)) : inputs[0].dims.slice(1, attributes.spatial ? 2 : void 0);
        checkShapeEqual(inputs[1].dims, shape, "Invalid input scale");
        checkShapeEqual(inputs[2].dims, shape, "Invalid input B");
        checkShapeEqual(inputs[3].dims, shape, "Invalid input mean");
        checkShapeEqual(inputs[4].dims, shape, "Invalid input var");
      } else {
        checkShapeEqual(inputs[1].dims, [1], "Invalid input scale");
        checkShapeEqual(inputs[2].dims, [1], "Invalid input B");
        checkShapeEqual(inputs[3].dims, [1], "Invalid input mean");
        checkShapeEqual(inputs[4].dims, [1], "Invalid input var");
      }
    };
    createBatchNormInferenceProgramInfo = (inputs, attributes) => {
      const { epsilon, spatial, format } = attributes;
      const yShape = inputs[0].dims;
      const components = spatial ? getMaxComponents(yShape[yShape.length - 1]) : 1;
      const cComponents = format === "NHWC" && yShape.length > 1 ? components : 1;
      const outputSize = ShapeUtil.size(yShape) / components;
      const useShapesUniforms = spatial;
      const shapeOrRank = useShapesUniforms ? yShape.length : yShape;
      const x = inputVariable("x", inputs[0].dataType, inputs[0].dims, components);
      const scale = inputVariable("scale", inputs[1].dataType, inputs[1].dims, cComponents);
      const bias = inputVariable("bias", inputs[2].dataType, inputs[2].dims, cComponents);
      const inputMean = inputVariable("inputMean", inputs[3].dataType, inputs[3].dims, cComponents);
      const inputVar = inputVariable("inputVar", inputs[4].dataType, inputs[4].dims, cComponents);
      const y = outputVariable("y", inputs[0].dataType, shapeOrRank, components);
      const calcCOffset = () => {
        let cOffset = "";
        if (spatial) {
          cOffset = `let cOffset = ${yShape.length === 1 ? "0u" : format === "NHWC" ? `outputIndices[${yShape.length - 1}] / ${components}` : "outputIndices[1]"};`;
        } else {
          if (format === "NCHW") {
            cOffset = `
            ${y.indicesSet("outputIndices", "0", "0")}
            let cOffset = ${y.indicesToOffset("outputIndices")};`;
          } else {
            cOffset = `var cIndices = ${scale.type.indices}(0);
                       cIndices[0] = outputIndices[${yShape.length - 1}];`;
            for (let i = 1; i < scale.rank; i++) {
              cOffset += `cIndices[${i}] = outputIndices[${i}];`;
            }
            cOffset += `let cOffset = ${scale.indicesToOffset("cIndices")};`;
          }
        }
        return cOffset;
      };
      const getInferenceModeShaderSource = (helper) => `
  const epsilon = ${epsilon};
  ${helper.registerUniform("outputSize", "u32").declareVariables(x, scale, bias, inputMean, inputVar, y)}
  ${helper.mainStart()}
  ${helper.guardAgainstOutOfBoundsWorkgroupSizes("uniforms.outputSize")}
    var outputIndices = ${y.offsetToIndices(`global_idx * ${components}`)};
    ${calcCOffset()}
    let scale = ${scale.getByOffset("cOffset")};
    let bias = ${bias.getByOffset("cOffset")};
    let inputMean = ${inputMean.getByOffset("cOffset")};
    let inputVar = ${inputVar.getByOffset("cOffset")};
    let x = ${x.getByOffset("global_idx")};
    let value = (x - inputMean) * inverseSqrt(inputVar + epsilon) * scale + bias;
    ${y.setByOffset("global_idx", "value")}
  }`;
      return {
        name: "BatchNormalization",
        shaderCache: {
          hint: `${attributes.epsilon}_${attributes.format}_${spatial}_${components}`,
          inputDependencies: useShapesUniforms ? ["rank", "type", "type", "type", "type"] : void 0
        },
        getShaderSource: getInferenceModeShaderSource,
        getRunData: () => ({
          outputs: [{ dims: inputs[0].dims, dataType: inputs[0].dataType }],
          dispatchGroup: { x: Math.ceil(
            outputSize / 64
            /* workgroup size */
          ) },
          programUniforms: useShapesUniforms ? [{ type: 12 /* uint32 */, data: outputSize }, ...createTensorShapeVariables(yShape)] : [{ type: 12 /* uint32 */, data: outputSize }]
        })
      };
    };
    parseBatchNormAttributes = (attributes) => createAttributeWithCacheKey(attributes);
    batchNorm = (context, attributes) => {
      const { inputs, outputCount } = context;
      const updatedAttributes = parseBatchNormAttributes({ ...attributes, outputCount });
      if (env2.webgpu.validateInputContent) {
        validateInputs4(inputs, updatedAttributes);
      }
      if (attributes.trainingMode) {
        throw new Error("BatchNormalization trainingMode is not supported yet.");
      } else {
        context.compute(createBatchNormInferenceProgramInfo(inputs, updatedAttributes));
      }
    };
  }
});

// web/lib/wasm/jsep/webgpu/ops/bias-add.ts
var validateInputs5, createBiasAddProgramInfo, biasAdd;
var init_bias_add = __esm({
  "web/lib/wasm/jsep/webgpu/ops/bias-add.ts"() {
    "use strict";
    init_util();
    init_common();
    validateInputs5 = (inputs) => {
      if (inputs[0].dims.length !== 3) {
        throw new Error("input should have 3 dimensions");
      }
      if (![320, 640, 1280].includes(inputs[0].dims[2])) {
        throw new Error("number of channels should be 320, 640 or 1280");
      }
      if (inputs[1].dims.length !== 1) {
        throw new Error("bias is expected to have 1 dimensions");
      }
      if (inputs[0].dims[2] !== inputs[1].dims[0]) {
        throw new Error("last dimension of input and bias are not the same");
      }
    };
    createBiasAddProgramInfo = (inputs) => {
      const outputShape = inputs[0].dims;
      const channels = inputs[0].dims[2];
      const outputSize = ShapeUtil.size(outputShape) / 4;
      const dataType = inputs[0].dataType;
      const input = inputVariable("input", dataType, outputShape, 4);
      const bias = inputVariable("bias", dataType, [channels], 4);
      const residual = inputVariable("residual", dataType, outputShape, 4);
      const output = outputVariable("output", dataType, outputShape, 4);
      const getShaderSource = (shaderHelper) => `
  const channels = ${channels}u / 4;
  ${shaderHelper.declareVariables(input, bias, residual, output)}

  ${shaderHelper.mainStart()}
    ${shaderHelper.guardAgainstOutOfBoundsWorkgroupSizes(outputSize)}
    let value = ${input.getByOffset("global_idx")}
      + ${bias.getByOffset("global_idx % channels")} + ${residual.getByOffset("global_idx")};
    ${output.setByOffset("global_idx", "value")}
  }`;
      return {
        name: "BiasAdd",
        getRunData: () => ({
          outputs: [{ dims: outputShape, dataType: inputs[0].dataType }],
          dispatchGroup: { x: Math.ceil(
            outputSize / 64
            /* workgroup size */
          ) }
        }),
        getShaderSource
      };
    };
    biasAdd = (context) => {
      validateInputs5(context.inputs);
      context.compute(createBiasAddProgramInfo(context.inputs));
    };
  }
});

// web/lib/wasm/jsep/webgpu/ops/unary-op.ts
var createElementwiseProgramShader, createElementwiseProgramInfo, abs, acos, acosh, asin, asinh, atan, atanh, parseCastAttributes, cast, generateClipAttributesFromInputs, clip, ceil, cos, cosh, parseAlphaAttributes, elu, erfImpl, erf, exp, floor, gelu, leakyRelu, not, neg, reciprocal, relu, sigmoid, parseHardSigmoidAttributes, hardSigmoid, sin, sinh, sqrt, tan, tanhExpression, tanh, fastGeluImpl, fastGeluExpression, fastGelu, thresholdedRelu, log, quickGeluImpl, quickGeluExpression, quickgelu;
var init_unary_op = __esm({
  "web/lib/wasm/jsep/webgpu/ops/unary-op.ts"() {
    "use strict";
    init_wasm_common();
    init_util();
    init_attribute_with_cache_key();
    init_common();
    createElementwiseProgramShader = (shaderHelper, datasize, inputDataType, outputDataType, funcCall, additionalImplementation) => {
      const vecSize = Math.ceil(datasize / 4);
      let expression = "";
      if (typeof funcCall === "string") {
        expression = `${funcCall}(a)`;
      } else {
        expression = funcCall("a");
      }
      const input = inputVariable("inputData", inputDataType, [vecSize], 4);
      const output = outputVariable("outputData", outputDataType, [vecSize], 4);
      return `
      ${shaderHelper.registerUniform("vec_size", "u32").declareVariables(input, output)}

  ${additionalImplementation ?? ""}

  ${shaderHelper.mainStart()}
    ${shaderHelper.guardAgainstOutOfBoundsWorkgroupSizes("uniforms.vec_size")}

    let a = ${input.getByOffset("global_idx")};
    ${output.setByOffset("global_idx", expression)}
  }`;
    };
    createElementwiseProgramInfo = (input, name, funcCall, additionalImplementation, cacheKey, outputDataType = input.dataType) => ({
      name,
      shaderCache: { hint: cacheKey, inputDependencies: ["type"] },
      getShaderSource: (shaderHelper) => createElementwiseProgramShader(
        shaderHelper,
        ShapeUtil.size(input.dims),
        input.dataType,
        outputDataType,
        funcCall,
        additionalImplementation
      ),
      getRunData: (inputTensors) => ({
        outputs: [{ dims: input.dims, dataType: outputDataType }],
        dispatchGroup: { x: Math.ceil(
          ShapeUtil.size(inputTensors[0].dims) / 64 / 4
          /* vec size */
        ) },
        programUniforms: [{ type: 12 /* uint32 */, data: Math.ceil(ShapeUtil.size(input.dims) / 4) }]
      })
    });
    abs = (context) => {
      context.compute(createElementwiseProgramInfo(context.inputs[0], "Abs", "abs"));
    };
    acos = (context) => {
      context.compute(createElementwiseProgramInfo(context.inputs[0], "Acos", "acos"));
    };
    acosh = (context) => {
      context.compute(createElementwiseProgramInfo(context.inputs[0], "Acosh", "acosh"));
    };
    asin = (context) => {
      context.compute(createElementwiseProgramInfo(context.inputs[0], "Asin", "asin"));
    };
    asinh = (context) => {
      context.compute(createElementwiseProgramInfo(context.inputs[0], "Asinh", "asinh"));
    };
    atan = (context) => {
      context.compute(createElementwiseProgramInfo(context.inputs[0], "Atan", "atan"));
    };
    atanh = (context) => {
      context.compute(createElementwiseProgramInfo(context.inputs[0], "Atanh", "atanh"));
    };
    parseCastAttributes = (attributes) => createAttributeWithCacheKey(attributes);
    cast = (context, attributes) => {
      let func;
      switch (attributes.to) {
        case 10 /* float16 */:
          func = "vec4<f16>";
          break;
        case 1 /* float */:
          func = "vec4<f32>";
          break;
        case 12 /* uint32 */:
          func = "vec4<u32>";
          break;
        case 6 /* int32 */:
          func = "vec4<i32>";
          break;
        case 9 /* bool */:
          func = "vec4<bool>";
          break;
        default:
          throw new RangeError(`not supported type (specified in attribute 'to' from 'Cast' operator): ${attributes.to}`);
      }
      context.compute(
        createElementwiseProgramInfo(context.inputs[0], "Cast", func, void 0, attributes.cacheKey, attributes.to)
      );
    };
    generateClipAttributesFromInputs = (inputs) => {
      const min = inputs.length >= 2 && inputs[1].data !== 0 ? inputs[1].getFloat32Array()[0] : MIN_CLIP;
      const max = inputs.length >= 3 && inputs[2].data !== 0 ? inputs[2].getFloat32Array()[0] : MAX_CLIP;
      return createAttributeWithCacheKey({ min, max });
    };
    clip = (context, clipAttributes) => {
      const attributes = context.inputs.length === 1 ? clipAttributes : generateClipAttributesFromInputs(context.inputs);
      const dataType = tensorTypeToWsglValueType(context.inputs[0].dataType);
      context.compute(
        createElementwiseProgramInfo(
          context.inputs[0],
          "Clip",
          (a) => `clamp(${a}, clip_min_, clip_max_)`,
          `
    const clip_min_: vec4<${dataType}> = vec4(${dataType}(${attributes.min}));
    const clip_max_: vec4<${dataType}> = vec4(${dataType}(${attributes.max}));
`,
          attributes.cacheKey
        ),
        { inputs: [0] }
      );
    };
    ceil = (context) => {
      context.compute(createElementwiseProgramInfo(context.inputs[0], "Ceil", "ceil"));
    };
    cos = (context) => {
      context.compute(createElementwiseProgramInfo(context.inputs[0], "Cos", "cos"));
    };
    cosh = (context) => {
      context.compute(createElementwiseProgramInfo(context.inputs[0], "Cosh", "cosh"));
    };
    parseAlphaAttributes = (attributes) => createAttributeWithCacheKey(attributes);
    elu = (context, attributes) => {
      const dataType = tensorTypeToWsglValueType(context.inputs[0].dataType);
      context.compute(
        createElementwiseProgramInfo(
          context.inputs[0],
          "Elu",
          (a) => `elu_vf32(${a})`,
          `
  const elu_alpha_ = ${dataType}(${attributes.alpha});

  fn elu_f32(a: ${dataType}) -> ${dataType} {
  return select((exp(a) - 1.0) * elu_alpha_, a, a >= 0.0);
  }

  fn elu_vf32(v: vec4<${dataType}>) -> vec4<${dataType}> {
  return vec4(elu_f32(v.x), elu_f32(v.y), elu_f32(v.z), elu_f32(v.w));
  }`,
          attributes.cacheKey
        )
      );
    };
    erfImpl = (varType = "f32") => `
const r0: ${varType} = 0.3275911;
const r1: ${varType} = 0.254829592;
const r2: ${varType} = -0.284496736;
const r3: ${varType} = 1.421413741;
const r4: ${varType} = -1.453152027;
const r5: ${varType} = 1.061405429;

fn erf_vf32(v: vec4<${varType}>) -> vec4<${varType}> {
  let absv = abs(v);
  let x = 1.0 / (1.0 + r0 * absv);
  return sign(v) * (1.0 - ((((r5 * x + r4) * x + r3) * x + r2) * x + r1) * x * exp(-absv * absv));
}`;
    erf = (context) => {
      const dataType = tensorTypeToWsglValueType(context.inputs[0].dataType);
      context.compute(createElementwiseProgramInfo(context.inputs[0], "Erf", (a) => `erf_vf32(${a})`, erfImpl(dataType)));
    };
    exp = (context) => {
      context.compute(createElementwiseProgramInfo(context.inputs[0], "Exp", "exp"));
    };
    floor = (context) => {
      context.compute(createElementwiseProgramInfo(context.inputs[0], "Floor", "floor"));
    };
    gelu = (context) => {
      const dataType = tensorTypeToWsglValueType(context.inputs[0].dataType);
      context.compute(
        createElementwiseProgramInfo(
          context.inputs[0],
          "Gelu",
          (a) => `0.5 * ${a} * (1.0 + erf_vf32(${a} * 0.7071067811865475))`,
          erfImpl(dataType)
        )
      );
    };
    leakyRelu = (context, attributes) => {
      const dataType = tensorTypeToWsglValueType(context.inputs[0].dataType);
      context.compute(
        createElementwiseProgramInfo(
          context.inputs[0],
          "LeakyRelu",
          (a) => `select(leaky_relu_alpha_ * ${a}, ${a}, ${a} >= vec4<${dataType}>(0.0))`,
          `const leaky_relu_alpha_ = ${dataType}(${attributes.alpha});`,
          attributes.cacheKey
        )
      );
    };
    not = (context) => {
      context.compute(createElementwiseProgramInfo(context.inputs[0], "Not", (a) => `!${a}`));
    };
    neg = (context) => {
      context.compute(createElementwiseProgramInfo(context.inputs[0], "Neg", (a) => `-${a}`));
    };
    reciprocal = (context) => {
      context.compute(createElementwiseProgramInfo(context.inputs[0], "Reciprocal", (a) => `1.0/${a}`));
    };
    relu = (context) => {
      const dataType = tensorTypeToWsglValueType(context.inputs[0].dataType);
      context.compute(
        createElementwiseProgramInfo(
          context.inputs[0],
          "Relu",
          (a) => `select(vec4<${dataType}>(0.0), ${a}, ${a} > vec4<${dataType}>(0.0))`
        )
      );
    };
    sigmoid = (context) => {
      context.compute(createElementwiseProgramInfo(context.inputs[0], "Sigmoid", (a) => `(1.0 / (1.0 + exp(-${a})))`));
    };
    parseHardSigmoidAttributes = (attributes) => createAttributeWithCacheKey(
      attributes
    );
    hardSigmoid = (context, attributes) => {
      const dataType = tensorTypeToWsglValueType(context.inputs[0].dataType);
      context.compute(
        createElementwiseProgramInfo(
          context.inputs[0],
          "HardSigmoid",
          (a) => `max(vec4<${dataType}>(0.0), min(vec4<${dataType}>(1.0), ${attributes.alpha} * ${a} + vec4<${dataType}>(${attributes.beta})))`,
          void 0,
          attributes.cacheKey
        )
      );
    };
    sin = (context) => {
      context.compute(createElementwiseProgramInfo(context.inputs[0], "Sin", "sin"));
    };
    sinh = (context) => {
      context.compute(createElementwiseProgramInfo(context.inputs[0], "Sinh", "sinh"));
    };
    sqrt = (context) => {
      context.compute(createElementwiseProgramInfo(context.inputs[0], "Sqrt", "sqrt"));
    };
    tan = (context) => {
      context.compute(createElementwiseProgramInfo(context.inputs[0], "Tan", "tan"));
    };
    tanhExpression = (a) => `sign(${a}) * (1 - exp(-2 * abs(${a}))) / (1 + exp(-2 * abs(${a})))`;
    tanh = (context) => {
      context.compute(createElementwiseProgramInfo(context.inputs[0], "Tanh", tanhExpression));
    };
    fastGeluImpl = (varType = "f32") => `
const fast_gelu_a: ${varType} = 0.5;
const fast_gelu_b: ${varType} = 0.7978845608028654;
const fast_gelu_c: ${varType} = 0.035677408136300125;

fn tanh_v(v: vec4<${varType}>) -> vec4<${varType}> {
  return ${tanhExpression("v")};
}
`;
    fastGeluExpression = (x) => `(fast_gelu_a + fast_gelu_a * tanh_v(${x} * (fast_gelu_c * ${x} * ${x} + fast_gelu_b))) * ${x}`;
    fastGelu = (context) => {
      const dataType = tensorTypeToWsglValueType(context.inputs[0].dataType);
      context.compute(
        createElementwiseProgramInfo(
          context.inputs[0],
          "FastGelu",
          fastGeluExpression,
          fastGeluImpl(dataType),
          void 0,
          context.inputs[0].dataType
        )
      );
    };
    thresholdedRelu = (context, attributes) => {
      const dataType = tensorTypeToWsglValueType(context.inputs[0].dataType);
      context.compute(
        createElementwiseProgramInfo(
          context.inputs[0],
          "ThresholdedRelu",
          (a) => `select(vec4<${dataType}>(0.0), ${a}, ${a} > thresholded_relu_alpha_)`,
          `const thresholded_relu_alpha_ = vec4<${dataType}>(${attributes.alpha});`,
          attributes.cacheKey
        )
      );
      return 0;
    };
    log = (context) => {
      context.compute(createElementwiseProgramInfo(context.inputs[0], "Log", "log"));
    };
    quickGeluImpl = (varType, alpha) => `
const alpha = vec4<${varType}>(${alpha});
const one = ${varType}(1.0);
const zero = ${varType}(0.0);

fn quick_gelu_impl(x: vec4<${varType}>) -> vec4<${varType}> {
  let v = x *alpha;
  var x1 : vec4<${varType}>;
  for (var i = 0; i < 4; i = i + 1) {
    if (v[i] >= zero) {
      x1[i] = one / (one + exp(-v[i]));
    } else {
      x1[i] = one - one / (one + exp(v[i]));
    }
  }
  return x * x1;
}
`;
    quickGeluExpression = (x) => `quick_gelu_impl(${x})`;
    quickgelu = (context, attributes) => {
      const dType = tensorTypeToWsglValueType(context.inputs[0].dataType);
      context.compute(
        createElementwiseProgramInfo(
          context.inputs[0],
          "QuickGelu",
          quickGeluExpression,
          quickGeluImpl(dType, attributes.alpha),
          attributes.cacheKey,
          context.inputs[0].dataType
        )
      );
    };
  }
});

// web/lib/wasm/jsep/webgpu/ops/bias-split-gelu.ts
var validateInputs6, createBiasSplitGeluProgramInfo, biasSplitGelu;
var init_bias_split_gelu = __esm({
  "web/lib/wasm/jsep/webgpu/ops/bias-split-gelu.ts"() {
    "use strict";
    init_util();
    init_common();
    init_unary_op();
    validateInputs6 = (inputs) => {
      if (inputs[0].dims.length !== 3) {
        throw new Error("input should have 3 dimensions");
      }
      if (![2560, 5120, 10240].includes(inputs[0].dims[2])) {
        throw new Error("hidden state should be 2560, 5120 or 10240");
      }
      if (inputs[1].dims.length !== 1) {
        throw new Error("bias is expected to have 1 dimensions");
      }
      if (inputs[0].dims[2] !== inputs[1].dims[0]) {
        throw new Error("last dimension of input and bias are not the same");
      }
    };
    createBiasSplitGeluProgramInfo = (inputs) => {
      const outputShape = inputs[0].dims.slice();
      outputShape[2] = outputShape[2] / 2;
      const input = inputVariable("input", inputs[0].dataType, inputs[0].dims, 4);
      const bias = inputVariable("bias", inputs[0].dataType, [inputs[0].dims[2]], 4);
      const output = outputVariable("output", inputs[0].dataType, outputShape, 4);
      const outputSize = ShapeUtil.size(outputShape) / 4;
      const dataType = tensorTypeToWsglStorageType(inputs[0].dataType);
      const getShaderSource = (shaderHelper) => `
  const M_SQRT2 = sqrt(2.0);
  const halfChannels = ${inputs[0].dims[2] / 4 / 2}u;

  ${shaderHelper.declareVariables(input, bias, output)}

  ${erfImpl(dataType)}

  ${shaderHelper.mainStart()}
    ${shaderHelper.guardAgainstOutOfBoundsWorkgroupSizes(outputSize)}
    let biasIdx = global_idx % halfChannels;
    let batchIndex = global_idx / halfChannels;
    let inputOffset = biasIdx + batchIndex * halfChannels * 2;
    let valueLeft = input[inputOffset] + bias[biasIdx];
    let valueRight = input[inputOffset + halfChannels] + bias[biasIdx + halfChannels];
    let geluRight = valueRight * 0.5 * (erf_vf32(valueRight / M_SQRT2) + 1);

    ${output.setByOffset("global_idx", "valueLeft * geluRight")}
  }`;
      return {
        name: "BiasSplitGelu",
        getRunData: () => ({
          outputs: [{ dims: outputShape, dataType: inputs[0].dataType }],
          dispatchGroup: { x: Math.ceil(
            outputSize / 64
            /* workgroup size */
          ) }
        }),
        getShaderSource
      };
    };
    biasSplitGelu = (context) => {
      validateInputs6(context.inputs);
      context.compute(createBiasSplitGeluProgramInfo(context.inputs));
    };
  }
});

// web/lib/wasm/jsep/webgpu/ops/binary-op.ts
var createBinaryOpProgramShader, createBinaryOpProgramInfo, runBinaryOp, add, div, equal, mul, pow, sub, greater, less, greaterOrEqual, lessOrEqual;
var init_binary_op = __esm({
  "web/lib/wasm/jsep/webgpu/ops/binary-op.ts"() {
    "use strict";
    init_wasm_common();
    init_util();
    init_common();
    createBinaryOpProgramShader = (shaderHelper, dimsA, dimsB, dimsOutput, vectorize, doBroadcast, sharedDimensionDivisibleBy4, funcCall, typeA, typeB, typeOutput, additionalImplementation) => {
      let expressionScalar;
      let expressionVector;
      if (typeof funcCall === "string") {
        expressionScalar = expressionVector = (a2, b2) => `${funcCall}((${a2}),(${b2}))`;
      } else if (typeof funcCall === "function") {
        expressionScalar = expressionVector = funcCall;
      } else {
        expressionScalar = funcCall.scalar;
        expressionVector = funcCall.vector;
      }
      const output = outputVariable("outputData", typeOutput, dimsOutput.length, 4);
      const a = inputVariable("aData", typeA, dimsA.length, 4);
      const b = inputVariable("bData", typeB, dimsB.length, 4);
      let assignment;
      if (vectorize) {
        if (doBroadcast) {
          const isAOneElement = ShapeUtil.size(dimsA) === 1;
          const isBOneElement = ShapeUtil.size(dimsB) === 1;
          const aLastDimDivisibleBy4 = dimsA.length > 0 && dimsA[dimsA.length - 1] % 4 === 0;
          const bLastDimDivisibleBy4 = dimsB.length > 0 && dimsB[dimsB.length - 1] % 4 === 0;
          if (isAOneElement || isBOneElement) {
            assignment = output.setByOffset(
              "global_idx",
              expressionVector(
                isAOneElement ? `${a.type.value}(${a.getByOffset("0")}.x)` : a.getByOffset("global_idx"),
                isBOneElement ? `${b.type.value}(${b.getByOffset("0")}.x)` : b.getByOffset("global_idx")
              )
            );
          } else {
            assignment = `
            let outputIndices = ${output.offsetToIndices("global_idx * 4u")};
            let offsetA = ${a.broadcastedIndicesToOffset("outputIndices", output)};
            let offsetB = ${b.broadcastedIndicesToOffset("outputIndices", output)};
            ${output.setByOffset(
              "global_idx",
              expressionVector(
                sharedDimensionDivisibleBy4 || aLastDimDivisibleBy4 ? a.getByOffset("offsetA / 4u") : `${a.type.value}(${a.getByOffset("offsetA / 4u")}[offsetA % 4u])`,
                sharedDimensionDivisibleBy4 || bLastDimDivisibleBy4 ? b.getByOffset("offsetB / 4u") : `${b.type.value}(${b.getByOffset("offsetB / 4u")}[offsetB % 4u])`
              )
            )}
          `;
          }
        } else {
          assignment = output.setByOffset(
            "global_idx",
            expressionVector(a.getByOffset("global_idx"), b.getByOffset("global_idx"))
          );
        }
      } else {
        if (!doBroadcast) {
          throw new Error("no necessary to use scalar implementation for element-wise binary op implementation.");
        }
        const singleAssignment = (resStr, x, typeCast = "") => {
          const expressionA = `aData[indexA${x}][componentA${x}]`;
          const expressionB = `bData[indexB${x}][componentB${x}]`;
          return `
            let outputIndices${x} = ${output.offsetToIndices(`global_idx * 4u + ${x}u`)};
            let offsetA${x} = ${a.broadcastedIndicesToOffset(`outputIndices${x}`, output)};
            let offsetB${x} = ${b.broadcastedIndicesToOffset(`outputIndices${x}`, output)};
            let indexA${x} = offsetA${x} / 4u;
            let indexB${x} = offsetB${x} / 4u;
            let componentA${x} = offsetA${x} % 4u;
            let componentB${x} = offsetB${x} % 4u;
            ${resStr}[${x}] = ${typeCast}(${expressionScalar(expressionA, expressionB)});
          `;
        };
        if (typeOutput === 9 /* bool */) {
          assignment = `
            var data = vec4<u32>(0);
            ${singleAssignment("data", 0, "u32")}
            ${singleAssignment("data", 1, "u32")}
            ${singleAssignment("data", 2, "u32")}
            ${singleAssignment("data", 3, "u32")}
            outputData[global_idx] = dot(vec4<u32>(0x1, 0x100, 0x10000, 0x1000000), vec4<u32>(data));`;
        } else {
          assignment = `
            ${singleAssignment("outputData[global_idx]", 0)}
            ${singleAssignment("outputData[global_idx]", 1)}
            ${singleAssignment("outputData[global_idx]", 2)}
            ${singleAssignment("outputData[global_idx]", 3)}
          `;
        }
      }
      return `
        ${shaderHelper.registerUniform("vec_size", "u32").declareVariables(a, b, output)}

        ${additionalImplementation ?? ""}

        ${shaderHelper.mainStart()}
        ${shaderHelper.guardAgainstOutOfBoundsWorkgroupSizes("uniforms.vec_size")}
        ${assignment}
      }`;
    };
    createBinaryOpProgramInfo = (name, cacheKey, a, b, funcCall, additionalImplementation, outputDataType = a.dataType) => {
      const isBroadcast = !ShapeUtil.areEqual(a.dims, b.dims);
      let outputShape = a.dims;
      let outputSize = ShapeUtil.size(a.dims);
      let vectorize = false;
      let sharedDimensionDivisibleBy4 = false;
      const cacheKeyAux = [isBroadcast];
      if (isBroadcast) {
        const calculatedShape = BroadcastUtil.calcShape(a.dims, b.dims, false);
        if (!calculatedShape) {
          throw new Error("Can't perform binary op on the given tensors");
        }
        outputShape = calculatedShape;
        outputSize = ShapeUtil.size(outputShape);
        const isAOneElement = ShapeUtil.size(a.dims) === 1;
        const isBOneElement = ShapeUtil.size(b.dims) === 1;
        const aLastDimDivisibleBy4 = a.dims.length > 0 && a.dims[a.dims.length - 1] % 4 === 0;
        const bLastDimDivisibleBy4 = b.dims.length > 0 && b.dims[b.dims.length - 1] % 4 === 0;
        cacheKeyAux.push(isAOneElement);
        cacheKeyAux.push(isBOneElement);
        cacheKeyAux.push(aLastDimDivisibleBy4);
        cacheKeyAux.push(bLastDimDivisibleBy4);
        let sharedDimension = 1;
        for (let i = 1; i < outputShape.length; i++) {
          const dimA = a.dims[a.dims.length - i] ?? 1;
          const dimB = b.dims[b.dims.length - i] ?? 1;
          if (dimA === dimB) {
            sharedDimension *= dimA;
          } else {
            break;
          }
        }
        if (sharedDimension % 4 === 0) {
          sharedDimensionDivisibleBy4 = true;
          vectorize = true;
        } else if (isAOneElement || isBOneElement || aLastDimDivisibleBy4 || bLastDimDivisibleBy4) {
          vectorize = true;
        }
      } else {
        vectorize = true;
      }
      cacheKeyAux.push(vectorize);
      return {
        name,
        shaderCache: {
          hint: cacheKey + cacheKeyAux.map((x) => x.toString()).join("_"),
          inputDependencies: ["rank", "rank"]
        },
        getShaderSource: (shaderHelper) => createBinaryOpProgramShader(
          shaderHelper,
          a.dims,
          b.dims,
          outputShape,
          vectorize,
          isBroadcast,
          sharedDimensionDivisibleBy4,
          funcCall,
          a.dataType,
          b.dataType,
          outputDataType,
          additionalImplementation
        ),
        getRunData: () => ({
          outputs: [{ dims: outputShape, dataType: outputDataType }],
          dispatchGroup: { x: Math.ceil(
            outputSize / 64 / 4
            /* component size */
          ) },
          programUniforms: [
            { type: 12 /* uint32 */, data: Math.ceil(ShapeUtil.size(outputShape) / 4) },
            ...createTensorShapeVariables(a.dims, b.dims, outputShape)
          ]
        })
      };
    };
    runBinaryOp = (context, name, funcCall, additionalImplementation, cacheKey, outputDataType) => {
      context.compute(
        createBinaryOpProgramInfo(
          name,
          cacheKey ?? "",
          context.inputs[0],
          context.inputs[1],
          funcCall,
          additionalImplementation,
          outputDataType
        )
      );
    };
    add = (context) => {
      runBinaryOp(context, "Add", (a, b) => `${a}+${b}`);
    };
    div = (context) => {
      runBinaryOp(context, "Div", (a, b) => `${a}/${b}`);
    };
    equal = (context) => {
      runBinaryOp(
        context,
        "Equal",
        { scalar: (a, b) => `u32(${a}==${b})`, vector: (a, b) => `vec4<u32>(${a}==${b})` },
        void 0,
        void 0,
        9 /* bool */
      );
    };
    mul = (context) => {
      runBinaryOp(context, "Mul", (a, b) => `${a}*${b}`);
    };
    pow = (context) => {
      const type = inputVariable("input", context.inputs[0].dataType, context.inputs[0].dims).type.value;
      const roundStr = type === "i32" ? "round" : "";
      runBinaryOp(
        context,
        "Pow",
        { scalar: (a, b) => `pow_custom(${a},${b})`, vector: (a, b) => `pow_vector_custom(${a},${b})` },
        `
    fn pow_custom(a : ${type}, b : ${type}) -> ${type} {
      if (b == ${type}(0.0)) {
        return ${type}(1.0);
      } else if (a < ${type}(0.0) && f32(b) != floor(f32(b))) {
        return ${type}(pow(f32(a), f32(b))); // NaN
      }
      return select(sign(a), ${type}(1.0), round(f32(abs(b) % ${type}(2.0))) != 1.0) * ${type}(${roundStr}(pow(f32(abs(a)), f32(b))));
    }
    fn pow_vector_custom(a : vec4<${type}>, b : vec4<${type}>) -> vec4<${type}> {
      // TODO: implement vectorized pow
      return vec4<${type}>(pow_custom(a.x, b.x), pow_custom(a.y, b.y), pow_custom(a.z, b.z), pow_custom(a.w, b.w));
    }
      `
      );
    };
    sub = (context) => {
      runBinaryOp(context, "Sub", (a, b) => `${a}-${b}`);
    };
    greater = (context) => {
      runBinaryOp(
        context,
        "Greater",
        { scalar: (a, b) => `u32(${a}>${b})`, vector: (a, b) => `vec4<u32>(${a}>${b})` },
        void 0,
        void 0,
        9 /* bool */
      );
    };
    less = (context) => {
      runBinaryOp(
        context,
        "Less",
        { scalar: (a, b) => `u32(${a}<${b})`, vector: (a, b) => `vec4<u32>(${a}<${b})` },
        void 0,
        void 0,
        9 /* bool */
      );
    };
    greaterOrEqual = (context) => {
      runBinaryOp(
        context,
        "GreaterOrEqual",
        { scalar: (a, b) => `u32(${a}>=${b})`, vector: (a, b) => `vec4<u32>(${a}>=${b})` },
        void 0,
        void 0,
        9 /* bool */
      );
    };
    lessOrEqual = (context) => {
      runBinaryOp(
        context,
        "LessOrEqual",
        { scalar: (a, b) => `u32(${a}<=${b})`, vector: (a, b) => `vec4<u32>(${a}<=${b})` },
        void 0,
        void 0,
        9 /* bool */
      );
    };
  }
});

// web/lib/wasm/jsep/webgpu/ops/concat.ts
var validateInputs7, calculateInputIndexImpl, assignOutputData, createConcatProgramInfo, concat, parseConcatAttributes;
var init_concat = __esm({
  "web/lib/wasm/jsep/webgpu/ops/concat.ts"() {
    "use strict";
    init_wasm_common();
    init_util();
    init_attribute_with_cache_key();
    init_common();
    validateInputs7 = (inputs, axis) => {
      if (!inputs || inputs.length < 1) {
        throw new Error("too few inputs");
      }
      const referenceIndex = 0;
      const referenceInput = inputs[referenceIndex];
      const inputType = referenceInput.dataType;
      const inputRank = referenceInput.dims.length;
      inputs.forEach((input, i) => {
        if (i === referenceIndex) {
          return;
        }
        if (input.dataType !== inputType) {
          throw new Error("input tensors should be one type");
        }
        if (input.dims.length !== inputRank) {
          throw new Error("input tensors should have the same shape");
        }
        input.dims.forEach((dim, i2) => {
          if (i2 !== axis && dim !== referenceInput.dims[i2]) {
            throw new Error("non concat dimensions must match");
          }
        });
      });
    };
    calculateInputIndexImpl = (numberOfTensors, sizeInConcatAxisStr) => `
  fn calculateInputIndex(index: u32) -> u32 {
    let sizeInConcatAxis = array<u32, ${numberOfTensors}u>(${sizeInConcatAxisStr});
    for (var i: u32 = 0u; i < ${numberOfTensors}; i += 1u ) {
      if (index < sizeInConcatAxis[i]) {
        return i;
      }
    }
    return ${numberOfTensors}u;
  }`;
    assignOutputData = (inputs, output) => {
      const numberOfTensors = inputs.length;
      const codeLines = [];
      for (let i = 0; i < numberOfTensors; ++i) {
        const returnSnippet = output.setByOffset("global_idx", inputs[i].getByIndices("indices"));
        if (numberOfTensors === 1) {
          codeLines.push(returnSnippet);
        } else if (i === 0) {
          codeLines.push(`if (inputIndex == ${i}u) { ${returnSnippet} }`);
        } else if (i === numberOfTensors - 1) {
          codeLines.push(`else { ${returnSnippet} }`);
        } else {
          codeLines.push(`else if (inputIndex == ${i}) { ${returnSnippet} }`);
        }
      }
      return codeLines.join("\n");
    };
    createConcatProgramInfo = (inputs, adjustedAxis, outputShape, dataType) => {
      const outputSize = ShapeUtil.size(outputShape);
      const sizeInConcatAxis = new Array(inputs.length);
      const inputVars = new Array(inputs.length);
      let previousSum = 0;
      const inputDependencies = [];
      const inputRanks = [];
      const programUniforms = [{ type: 12 /* uint32 */, data: outputSize }];
      for (let i = 0; i < inputs.length; ++i) {
        previousSum += inputs[i].dims[adjustedAxis];
        sizeInConcatAxis[i] = previousSum;
        inputRanks.push(inputs[i].dims.length);
        inputVars[i] = inputVariable(`input${i}`, dataType, inputRanks[i]);
        inputDependencies.push("rank");
        programUniforms.push({ type: 12 /* uint32 */, data: sizeInConcatAxis[i] });
      }
      for (let i = 0; i < inputs.length; ++i) {
        programUniforms.push(...createTensorShapeVariables(inputs[i].dims));
      }
      programUniforms.push(...createTensorShapeVariables(outputShape));
      const output = outputVariable("output", dataType, outputShape.length);
      const indicesAxis = output.indicesGet("indices", adjustedAxis);
      const sizeInConcatAxisStr = Array.from(Array(sizeInConcatAxis.length).keys()).map((i) => `uniforms.sizeInConcatAxis${i}`).join(",");
      const getShaderSource = (shaderHelper) => `

  ${(() => {
        shaderHelper.registerUniform("outputSize", "u32");
        for (let i = 0; i < inputs.length; i++) {
          shaderHelper.registerUniform(`sizeInConcatAxis${i}`, "u32");
        }
        return shaderHelper.declareVariables(...inputVars, output);
      })()}

  ${calculateInputIndexImpl(sizeInConcatAxis.length, sizeInConcatAxisStr)}

  ${shaderHelper.mainStart()}
    ${shaderHelper.guardAgainstOutOfBoundsWorkgroupSizes("uniforms.outputSize")}

    var indices = ${output.offsetToIndices("global_idx")};

    let inputIndex = calculateInputIndex(${indicesAxis});
    if (inputIndex != 0u) {
      let sizeInConcatAxis = array<u32, ${sizeInConcatAxis.length}u>(${sizeInConcatAxisStr});
      ${indicesAxis} -= sizeInConcatAxis[inputIndex - 1u];
    }

    ${assignOutputData(inputVars, output)}
  }`;
      return {
        name: "Concat",
        shaderCache: { hint: `${adjustedAxis}`, inputDependencies },
        getRunData: () => ({
          outputs: [{ dims: outputShape, dataType }],
          dispatchGroup: { x: Math.ceil(
            outputSize / 64
            /* workgroup size */
          ) },
          programUniforms
        }),
        getShaderSource
      };
    };
    concat = (context, attributes) => {
      const inputs = context.inputs;
      const inputShape = inputs[0].dims;
      const adjustedAxis = ShapeUtil.normalizeAxis(attributes.axis, inputShape.length);
      validateInputs7(inputs, adjustedAxis);
      const outputShape = inputShape.slice();
      outputShape[adjustedAxis] = inputs.reduce(
        (sum, input) => sum + (input.dims.length > adjustedAxis ? input.dims[adjustedAxis] : 0),
        0
      );
      const nonEmptyInputs = inputs.filter((input) => ShapeUtil.size(input.dims) > 0);
      context.compute(createConcatProgramInfo(nonEmptyInputs, adjustedAxis, outputShape, inputs[0].dataType), {
        inputs: nonEmptyInputs
      });
    };
    parseConcatAttributes = (attributes) => createAttributeWithCacheKey({ axis: attributes.axis });
  }
});

// web/lib/wasm/jsep/webgpu/ops/fuse-utils.ts
var getActivationSnippet, appendActivationUniformsData, appendActivationUniforms, parseInternalActivationAttributes;
var init_fuse_utils = __esm({
  "web/lib/wasm/jsep/webgpu/ops/fuse-utils.ts"() {
    "use strict";
    init_wasm_common();
    init_util();
    getActivationSnippet = (attributes, valueType, baseType = "f32") => {
      switch (attributes.activation) {
        case "Relu":
          return `value = max(value, ${valueType}(0.0));`;
        case "Sigmoid":
          return `value = (${valueType}(1.0) / (${valueType}(1.0) + exp(-value)));`;
        case "Clip":
          return `value = clamp(value, ${valueType}(${baseType}(uniforms.clip_min)), ${valueType}(${baseType}(uniforms.clip_max)));`;
        case "HardSigmoid":
          return `value = max(${valueType}(0.0), min(${valueType}(1.0), ${baseType}(uniforms.alpha) * value + ${baseType}(uniforms.beta)));`;
        case "LeakyRelu":
          return `value = select(${baseType}(uniforms.alpha) * value, value, value >= ${valueType}(0.0));`;
        case "Tanh":
          return `let e2x = exp(-2.0 * abs(value));
              value = sign(value) * (1.0 - e2x) / (1.0 + e2x);
        `;
        case "":
          return "";
        default:
          throw new Error(`Unsupported activation ${attributes.activation}`);
      }
    };
    appendActivationUniformsData = (attributes, programUniform) => {
      if (attributes.activation === "Clip") {
        programUniform.push(
          { type: 1 /* float */, data: attributes.clipMax },
          { type: 1 /* float */, data: attributes.clipMin }
        );
      } else if (attributes.activation === "HardSigmoid") {
        programUniform.push(
          { type: 1 /* float */, data: attributes.alpha },
          { type: 1 /* float */, data: attributes.beta }
        );
      } else if (attributes.activation === "LeakyRelu") {
        programUniform.push({ type: 1 /* float */, data: attributes.alpha });
      }
    };
    appendActivationUniforms = (attributes, uniforms) => {
      if (attributes.activation === "Clip") {
        uniforms.push({ name: "clip_max", type: "f32" }, { name: "clip_min", type: "f32" });
      } else if (attributes.activation === "HardSigmoid") {
        uniforms.push({ name: "alpha", type: "f32" }, { name: "beta", type: "f32" });
      } else if (attributes.activation === "LeakyRelu") {
        uniforms.push({ name: "alpha", type: "f32" });
      }
    };
    parseInternalActivationAttributes = (attributes) => {
      const activation = attributes?.activation || "";
      if (activation === "HardSigmoid") {
        const [alpha, beta] = attributes?.activation_params || [0.2, 0.5];
        return { activation, alpha, beta };
      } else if (activation === "Clip") {
        const [clipMin, clipMax] = attributes?.activation_params || [MIN_CLIP, MAX_CLIP];
        return { activation, clipMax, clipMin };
      } else if (activation === "LeakyRelu") {
        const [alpha] = attributes?.activation_params || [0.01];
        return { activation, alpha };
      }
      return { activation };
    };
  }
});

// web/lib/wasm/jsep/webgpu/ops/3rd-party/activation_util.ts
var typeSnippet, biasSnippet;
var init_activation_util = __esm({
  "web/lib/wasm/jsep/webgpu/ops/3rd-party/activation_util.ts"() {
    "use strict";
    typeSnippet = (component, dataType) => {
      switch (component) {
        case 1:
          return dataType;
        case 2:
          return `vec2<${dataType}>`;
        case 3:
          return `vec3<${dataType}>`;
        case 4:
          return `vec4<${dataType}>`;
        default:
          throw new Error(`${component}-component is not supported.`);
      }
    };
    biasSnippet = (hasBias) => `
      ${hasBias ? "value = value + getBiasByOutputCoords(coords);" : ""}
      `;
  }
});

// web/lib/wasm/jsep/webgpu/ops/3rd-party/conv_util.ts
var utilFunctions;
var init_conv_util = __esm({
  "web/lib/wasm/jsep/webgpu/ops/3rd-party/conv_util.ts"() {
    "use strict";
    utilFunctions = (strideStr) => `
fn getIndexFromCoords4D(coords : vec4<i32>, shape : vec4<i32>) -> i32 {
  return dot(coords, vec4<i32>(
      shape.y * shape.z * shape.w, shape.z * shape.w, shape.w, 1));
}
fn getOutputIndexFromCoords(coords : vec4<i32>) -> i32 {
  return dot(coords, vec4<i32>(
    i32(${strideStr}.x), i32(${strideStr}.y), i32(${strideStr}.z), 1));
}
`;
  }
});

// web/lib/wasm/jsep/webgpu/ops/3rd-party/matmul_packed_webgpu.ts
var writeDataToSubAVec4Snippet, calculateResultSnippet, makeMatMulPackedVec4Source, writeDataToSubASnippet, readDataFromSubASnippet, makeMatMulPackedSource, matMulReadWriteFnSource, createMatmulProgramInfo;
var init_matmul_packed_webgpu = __esm({
  "web/lib/wasm/jsep/webgpu/ops/3rd-party/matmul_packed_webgpu.ts"() {
    "use strict";
    init_wasm_common();
    init_util();
    init_common();
    init_fuse_utils();
    init_activation_util();
    writeDataToSubAVec4Snippet = (transpose2, batchDims) => {
      if (transpose2) {
        return `
        mm_Asub[inputRow][inputCol] = mm_readA(batch,
          kStart + inputRow,
          globalRowStart / innerElementSize + inputCol${batchDims ? ", batchIndices" : ""});
        `;
      } else {
        return `
        mm_Asub[inputRow][inputCol] = mm_readA(batch,
          globalRow + innerRow,
          kStart / innerElementSize + inputCol${batchDims ? ", batchIndices" : ""});
        `;
      }
    };
    calculateResultSnippet = (transposeA, innerElementSize) => {
      if (transposeA) {
        return `
        let ACached0 = mm_Asub[k * innerElementSize][localRow];
        let ACached1 = mm_Asub[k * innerElementSize + 1][localRow];
        let ACached2 = mm_Asub[k * innerElementSize + 2][localRow];
        ${innerElementSize === 3 ? "" : "let ACached3 = mm_Asub[k * innerElementSize + 3][localRow];"}
        for (var i = 0; i < rowPerThread; i = i + 1) {
          acc[i] = BCached0 * ACached0[i] + acc[i];
          acc[i] = BCached1 * ACached1[i] + acc[i];
          acc[i] = BCached2 * ACached2[i] + acc[i];
          ${innerElementSize === 3 ? "" : "acc[i] = BCached3 * ACached3[i] + acc[i];"}
        }`;
      } else {
        return `
        for (var i = 0; i < rowPerThread; i = i + 1) {
          let ACached = mm_Asub[tileRow + i][k];
          acc[i] = BCached0 * ACached.x + acc[i];
          acc[i] = BCached1 * ACached.y + acc[i];
          acc[i] = BCached2 * ACached.z + acc[i];
          ${innerElementSize === 3 ? "" : "acc[i] = BCached3 * ACached.w + acc[i];"}
        }`;
      }
    };
    makeMatMulPackedVec4Source = (workPerThread, workgroupSize, type = "f32", batchDims, transposeA = false, tileInner = 32, splitK = false, splitedDimInner = 32) => {
      const tileAOuter = workgroupSize[1] * workPerThread[1];
      const tileBOuter = workgroupSize[0] * workPerThread[0];
      const tileAWidth = transposeA ? tileAOuter : tileInner;
      const tileAHight = transposeA ? tileInner : tileAOuter;
      const innerElementSize = tileAWidth / workgroupSize[0];
      const rowPerThreadB = tileInner / workgroupSize[1];
      if (!((transposeA && innerElementSize === 4 && workPerThread[1] === 4 || !transposeA && (innerElementSize === 3 || innerElementSize === 4)) && tileAWidth % workgroupSize[0] === 0 && tileInner % workgroupSize[1] === 0 && workPerThread[0] === 4)) {
        throw new Error(`If transposeA ${transposeA} is true, innerElementSize ${innerElementSize} and workPerThread[1] ${workPerThread[1]} must be 4.
      Otherwise, innerElementSize ${innerElementSize} must be 3 or 4.
  tileAWidth ${tileAWidth} must be divisible by workgroupSize[0]${workgroupSize[0]}. tileInner ${tileInner} must be divisible by workgroupSize[1] ${workgroupSize[1]}. colPerThread ${workPerThread[0]} must be 4.`);
      }
      return `
var<workgroup> mm_Asub: array<array<vec${innerElementSize}<${type}>, ${tileAWidth / innerElementSize}>, ${tileAHight}>;
var<workgroup> mm_Bsub: array<array<vec4<${type}>, ${tileBOuter / workPerThread[0]}>, ${tileInner}>;

const rowPerThread = ${workPerThread[1]};
const colPerThread = ${workPerThread[0]};
const innerElementSize = ${innerElementSize};
const tileInner = ${tileInner};

@compute @workgroup_size(${workgroupSize[0]}, ${workgroupSize[1]}, ${workgroupSize[2]})
fn main(@builtin(local_invocation_id) localId : vec3<u32>,
        @builtin(global_invocation_id) globalId : vec3<u32>,
        @builtin(workgroup_id) workgroupId : vec3<u32>) {
  let localRow = i32(localId.y);
  let tileRow = localRow * rowPerThread;
  let tileCol = i32(localId.x);

  let globalRow =i32(globalId.y) * rowPerThread;
  let globalCol = i32(globalId.x);
  let batch = ${splitK ? "0" : "i32(globalId.z)"};
  ${batchDims ? `let batchIndices = ${batchDims.offsetToIndices("u32(batch)")};` : ""}
  let globalRowStart = i32(workgroupId.y) * ${tileAOuter};

  let num_tiles = ${splitK ? `${Math.ceil(splitedDimInner / tileInner)}` : "(uniforms.dim_inner - 1) / tileInner + 1"};
  var kStart = ${splitK ? `i32(globalId.z) * ${splitedDimInner}` : "0"};

  var acc: array<vec4<${type}>, rowPerThread>;

  // Loop over shared dimension.
  let tileRowB = localRow * ${rowPerThreadB};
  for (var t = 0; t < num_tiles; t = t + 1) {
      // Load one tile of A into local memory.
      for (var innerRow = 0; innerRow < rowPerThread; innerRow = innerRow + 1) {
          let inputRow = tileRow + innerRow;
          let inputCol = tileCol;
          ${writeDataToSubAVec4Snippet(transposeA, batchDims)}
      }

      // Load one tile of B into local memory.
      for (var innerRow = 0; innerRow < ${rowPerThreadB}; innerRow = innerRow + 1) {
          let inputRow = tileRowB + innerRow;
          let inputCol = tileCol;
          mm_Bsub[inputRow][inputCol] = mm_readB(batch, kStart + inputRow, globalCol${batchDims ? ", batchIndices" : ""});
      }
      kStart = kStart + tileInner;
      workgroupBarrier();

      // Compute acc values for a single thread.
      for (var k = 0; k < tileInner / innerElementSize; k = k + 1) {
          let BCached0 = mm_Bsub[k * innerElementSize][tileCol];
          let BCached1 = mm_Bsub[k * innerElementSize + 1][tileCol];
          let BCached2 = mm_Bsub[k * innerElementSize + 2][tileCol];
          ${innerElementSize === 3 ? "" : "let BCached3 = mm_Bsub[k * innerElementSize + 3][tileCol];"}

          ${calculateResultSnippet(transposeA, innerElementSize)}
      }

      workgroupBarrier();
  }

  for (var innerRow = 0; innerRow < rowPerThread; innerRow = innerRow + 1) {
      mm_write(batch, globalRow + innerRow, globalCol, acc[innerRow]);
  }
}`;
    };
    writeDataToSubASnippet = (transpose2, batchDims) => {
      if (transpose2) {
        return `
            mm_Asub[inputRow][inputCol] = mm_readA(batch,
              kStart + inputRow,
              globalRowStart + inputCol${batchDims ? ", batchIndices" : ""});
            `;
      } else {
        return `
            mm_Asub[inputRow][inputCol] = mm_readA(batch,
              globalRowStart + inputRow,
              kStart + inputCol${batchDims ? ", batchIndices" : ""});
            `;
      }
    };
    readDataFromSubASnippet = (transposeA) => transposeA ? "let ACached = mm_Asub[k][tileRow + innerRow];" : "let ACached = mm_Asub[tileRow + innerRow][k];";
    makeMatMulPackedSource = (workPerThread, workgroupSize, type = "f32", batchDims, transposeA = false, tileInner = 32, splitK = false, splitedDimInner = 32, sequentialAccessByThreads = false) => {
      const tileAOuter = workPerThread[1] * workgroupSize[1];
      const tileBOuter = workPerThread[0] * workgroupSize[0];
      const tileAWidth = transposeA ? tileAOuter : tileInner;
      const tileAHight = transposeA ? tileInner : tileAOuter;
      if (!(tileAHight % workgroupSize[1] === 0 && tileAWidth % workgroupSize[0] === 0 && tileInner % workgroupSize[1] === 0)) {
        throw new Error(
          `tileAHight ${tileAHight} must be divisible by workgroupSize[1]${workgroupSize[1]}, tileAWidth ${tileAWidth} must be divisible by workgroupSize[0]${workgroupSize[0]}, tileInner ${tileInner} must be divisible by workgroupSize[1]${workgroupSize[1]}`
        );
      }
      const rowPerThreadA = tileAHight / workgroupSize[1];
      const colPerThreadA = tileAWidth / workgroupSize[0];
      const rowPerThreadB = tileInner / workgroupSize[1];
      const matmulSnippet = sequentialAccessByThreads ? `
    let localRow = i32(localId.y);
    let localCol = i32(localId.x);
    let globalRowStart = i32(workgroupId.y) * ${tileAOuter};
    let globalColStart = i32(workgroupId.x) * ${tileBOuter};

    // Loop over shared dimension.
    for (var t = 0; t < num_tiles; t = t + 1) {
      // Load one tile of A into local memory.
      for (var inputRow = localRow; inputRow < ${tileAHight}; inputRow = inputRow + ${workgroupSize[1]}) {
        for (var inputCol = localCol; inputCol < ${tileAWidth}; inputCol = inputCol + ${workgroupSize[0]}) {
          ${writeDataToSubASnippet(transposeA, batchDims)}
        }
      }
      // Load one tile of B into local memory.
      for (var inputRow = localRow; inputRow < ${tileInner}; inputRow = inputRow + ${workgroupSize[1]}) {
            for (var inputCol = localCol; inputCol < ${tileBOuter}; inputCol = inputCol + ${workgroupSize[0]}) {
          mm_Bsub[inputRow][inputCol] = mm_readB(batch,
            kStart + inputRow,
            globalColStart + inputCol${batchDims ? ", batchIndices" : ""});
        }
      }
      kStart = kStart + tileInner;
      workgroupBarrier();

      // Compute acc values for a single thread.
      var BCached : array<${type}, colPerThread>;
      for (var k = 0; k < tileInner; k = k + 1) {
        for (var inner = 0; inner < colPerThread; inner = inner + 1) {
          BCached[inner] = mm_Bsub[k][localCol + inner * ${workgroupSize[0]}];
        }
        for (var innerRow = 0; innerRow < rowPerThread; innerRow = innerRow + 1) {
          let ACached = ${transposeA ? `mm_Asub[k][localRow + innerRow * ${workgroupSize[1]}];` : `mm_Asub[localRow + innerRow * ${workgroupSize[1]}][k];`}
          for (var innerCol = 0; innerCol < colPerThread; innerCol = innerCol + 1) {
            acc[innerRow][innerCol] = acc[innerRow][innerCol] +
                ACached * BCached[innerCol];
          }
        }
      }
      workgroupBarrier();
    }
    for (var innerRow = 0; innerRow < rowPerThread; innerRow = innerRow + 1) {
      let gRow = globalRowStart + localRow + innerRow * ${workgroupSize[1]};
      for (var innerCol = 0; innerCol < colPerThread; innerCol = innerCol + 1) {
        let gCol = globalColStart + localCol + innerCol * ${workgroupSize[0]};
        mm_write(batch, gRow, gCol, acc[innerRow][innerCol]);
      }
    }
    ` : `
let tileRow = i32(localId.y) * rowPerThread;
let tileCol = i32(localId.x) * colPerThread;

let globalRow = i32(globalId.y) * rowPerThread;
let globalCol = i32(globalId.x) * colPerThread;
let globalRowStart = i32(workgroupId.y) * ${tileAOuter};

let tileRowA = i32(localId.y) * ${rowPerThreadA};
let tileColA = i32(localId.x) * ${colPerThreadA};
let tileRowB = i32(localId.y) * ${rowPerThreadB};
// Loop over shared dimension.
for (var t = 0; t < num_tiles; t = t + 1) {
  // Load one tile of A into local memory.
  for (var innerRow = 0; innerRow < ${rowPerThreadA}; innerRow = innerRow + 1) {
    for (var innerCol = 0; innerCol < ${colPerThreadA}; innerCol = innerCol + 1) {
      let inputRow = tileRowA + innerRow;
      let inputCol = tileColA + innerCol;
      ${writeDataToSubASnippet(transposeA, batchDims)}
    }
  }

  // Load one tile of B into local memory.
  for (var innerRow = 0; innerRow < ${rowPerThreadB}; innerRow = innerRow + 1) {
    for (var innerCol = 0; innerCol < colPerThread; innerCol = innerCol + 1) {
      let inputRow = tileRowB + innerRow;
      let inputCol = tileCol + innerCol;
      mm_Bsub[inputRow][inputCol] = mm_readB(batch,
        kStart + inputRow,
        globalCol + innerCol${batchDims ? ", batchIndices" : ""});
    }
  }
  kStart = kStart + tileInner;
  workgroupBarrier();

  // Compute acc values for a single thread.
  var BCached : array<${type}, colPerThread>;
  for (var k = 0; k < tileInner; k = k + 1) {
    for (var inner = 0; inner < colPerThread; inner = inner + 1) {
      BCached[inner] = mm_Bsub[k][tileCol + inner];
    }

    for (var innerRow = 0; innerRow < rowPerThread; innerRow = innerRow + 1) {
      ${readDataFromSubASnippet(transposeA)}
      for (var innerCol = 0; innerCol < colPerThread; innerCol = innerCol + 1) {
        acc[innerRow][innerCol] = acc[innerRow][innerCol] + ACached * BCached[innerCol];
      }
    }
  }

  workgroupBarrier();
}

for (var innerRow = 0; innerRow < rowPerThread; innerRow = innerRow + 1) {
  for (var innerCol = 0; innerCol < colPerThread; innerCol = innerCol + 1) {
    mm_write(batch, globalRow + innerRow, globalCol + innerCol,
        acc[innerRow][innerCol]);
  }
}
`;
      return `
  var<workgroup> mm_Asub : array<array<${type}, ${tileAWidth}>, ${tileAHight}>;
  var<workgroup> mm_Bsub : array<array<${type}, ${tileBOuter}>, ${tileInner}>;
  const rowPerThread = ${workPerThread[1]};
  const colPerThread = ${workPerThread[0]};
  const tileInner = ${tileInner};

@compute @workgroup_size(${workgroupSize[0]}, ${workgroupSize[1]}, ${workgroupSize[2]})
fn main(@builtin(local_invocation_id) localId : vec3<u32>,
        @builtin(global_invocation_id) globalId : vec3<u32>,
        @builtin(workgroup_id) workgroupId : vec3<u32>) {
    let batch = ${splitK ? "0" : "i32(globalId.z)"};
    ${batchDims ? `let batchIndices = ${batchDims.offsetToIndices("u32(batch)")};` : ""}
    let num_tiles = ${splitK ? `${Math.ceil(splitedDimInner / tileInner)}` : "(uniforms.dim_inner - 1) / tileInner + 1"};
    var kStart = ${splitK ? `i32(globalId.z) * ${splitedDimInner}` : "0"};

    var acc : array<array<${type}, colPerThread>, rowPerThread>;
    ${matmulSnippet}
  }
`;
    };
    matMulReadWriteFnSource = (component, hasBias, applyActivation, variables, batchShapes, isChannelsLast = false) => {
      const [batchAShape, batchBShape, batchShape] = batchShapes;
      const [batchVariable, aVariable, bVariable, outputVariable2] = variables;
      const broadCastADims = getBroadcastDims(batchAShape, batchShape);
      const broadCastBDims = getBroadcastDims(batchBShape, batchShape);
      const dataType = tensorTypeToWsglStorageType(variables[0].type.tensor);
      const getAIndices = () => {
        const aRank = aVariable.rank;
        const batchRank = batchVariable.rank;
        let resStr = `var aIndices: ${aVariable.type.indices};`;
        for (let i = aRank - 2 - 1, j = batchRank - 1; i >= 0; i--, j--) {
          resStr += `
aIndices[${i}] = ${batchRank > 1 ? `batchIndices[${j}]` : "batchIndices"};`;
        }
        broadCastADims.forEach((i) => {
          resStr += `
aIndices[${i}] = 0;`;
        });
        resStr += `
aIndices[${aRank - 2}] = u32(row);
                   aIndices[${aRank - 1}] = u32(colIn);`;
        return resStr;
      };
      const getBIndices = () => {
        const bRank = bVariable.rank;
        const batchRank = batchVariable.rank;
        let resStr = `var bIndices: ${bVariable.type.indices};`;
        for (let i = bRank - 2 - 1, j = batchRank - 1; i >= 0; i--, j--) {
          resStr += `
bIndices[${i}] = ${batchRank > 1 ? `batchIndices[${j}]` : "batchIndices"};`;
        }
        broadCastBDims.forEach((i) => {
          resStr += `
bIndices[${i}] = 0;`;
        });
        resStr += `
bIndices[${bRank - 2}] = u32(row);
                   bIndices[${bRank - 1}] = u32(colIn);`;
        return resStr;
      };
      const source = `
    fn mm_readA(batch: i32, row: i32, colIn: i32, batchIndices: ${batchVariable.type.indices}) -> ${typeSnippet(
        component,
        dataType
      )} {
      var value = ${typeSnippet(component, dataType)}(0.0);
      let col = colIn * ${component};
      if(row < uniforms.dim_a_outer && col < uniforms.dim_inner)
      {
        ${getAIndices()}
        value = ${aVariable.getByIndices("aIndices")};
      }
      return value;
    }

    fn mm_readB(batch: i32, row: i32, colIn: i32, batchIndices: ${batchVariable.type.indices}) -> ${typeSnippet(
        component,
        dataType
      )} {
      var value = ${typeSnippet(component, dataType)}(0.0);
      let col = colIn * ${component};
      if(row < uniforms.dim_inner && col < uniforms.dim_b_outer)
      {
        ${getBIndices()}
        value = ${bVariable.getByIndices("bIndices")};
      }
      return value;
    }

    fn mm_write(batch: i32, row: i32, colIn: i32, valueIn: ${typeSnippet(component, dataType)}) {
      let col = colIn * ${component};
      if (row < uniforms.dim_a_outer && col < uniforms.dim_b_outer) {
        var value = valueIn;
        let coords = vec3<i32>(batch, row, colIn);
        ${hasBias ? `value = value + ${isChannelsLast ? "bias[colIn]" : `${typeSnippet(component, dataType)}(bias[row])`};` : ""}
        ${applyActivation}
        ${outputVariable2.setByIndices("vec3<u32>(coords)", "value")}
      }
    }
    `;
      return source;
    };
    createMatmulProgramInfo = (inputs, activationAttributes, outputShape, reshapedOutputShape, isChannelsLast = false, squeezeOutputShapeFunction) => {
      const aShape = inputs[0].dims;
      const bShape = inputs[1].dims;
      const outerDimsA = aShape.slice(0, -2);
      const outerDimsB = bShape.slice(0, -2);
      const outerDims = reshapedOutputShape ? reshapedOutputShape.slice(0, -2) : outputShape.slice(0, -2);
      const batchSize = ShapeUtil.size(outerDims);
      const dimAOuter = aShape[aShape.length - 2];
      const dimInner = aShape[aShape.length - 1];
      const dimBOuter = bShape[bShape.length - 1];
      const isVec4 = dimInner % 4 === 0 && dimBOuter % 4 === 0;
      const elementsPerThread = dimAOuter <= 8 ? [4, 1, 1] : [4, 4, 1];
      const workgroupSize = [8, 8, 1];
      const dispatch = [
        Math.ceil(dimBOuter / workgroupSize[0] / elementsPerThread[0]),
        Math.ceil(dimAOuter / workgroupSize[1] / elementsPerThread[1]),
        Math.ceil(batchSize / workgroupSize[2] / elementsPerThread[2])
      ];
      const components = isVec4 ? 4 : 1;
      const aShapeTemp = [...outerDimsA, dimAOuter, dimInner / components];
      const aRank = aShapeTemp.length;
      const bShapeTemp = [...outerDimsB, dimInner, dimBOuter / components];
      const bRank = bShapeTemp.length;
      const outputShapeTemp = [batchSize, dimAOuter, dimBOuter / components];
      const programUniforms = [
        { type: 6 /* int32 */, data: dimAOuter },
        { type: 6 /* int32 */, data: dimBOuter },
        { type: 6 /* int32 */, data: dimInner }
      ];
      appendActivationUniformsData(activationAttributes, programUniforms);
      programUniforms.push(...createTensorShapeVariables(outerDims, aShapeTemp, bShapeTemp));
      const inputDependencies = ["rank", "rank"];
      const hasBias = inputs.length > 2;
      if (hasBias) {
        programUniforms.push(...createTensorShapeVariables(inputs[2].dims));
        inputDependencies.push("rank");
      }
      programUniforms.push(...createTensorShapeVariables(outputShapeTemp));
      const getShaderSource = (shaderHelper) => {
        const batchRank = outerDims.length;
        const batchDims = internalVariable("batchDims", inputs[0].dataType, batchRank, 1);
        const dataType = tensorTypeToWsglStorageType(inputs[0].dataType);
        const A = inputVariable("a", inputs[0].dataType, aRank, components);
        const B = inputVariable("b", inputs[1].dataType, bRank, components);
        const output = outputVariable("result", inputs[0].dataType, outputShapeTemp.length, components);
        const inputVariables = [A, B];
        if (hasBias) {
          const biasComponents = isChannelsLast ? components : 1;
          inputVariables.push(inputVariable("bias", inputs[2].dataType, inputs[2].dims.length, biasComponents));
        }
        const uniforms = [
          { name: "dim_a_outer", type: "i32" },
          { name: "dim_b_outer", type: "i32" },
          { name: "dim_inner", type: "i32" }
        ];
        appendActivationUniforms(activationAttributes, uniforms);
        const baseType = tensorTypeToWsglStorageType(output.type.tensor);
        const applyActivation = getActivationSnippet(activationAttributes, output.type.value, baseType);
        const declareFunctions = matMulReadWriteFnSource(
          components,
          hasBias,
          applyActivation,
          [batchDims, A, B, output],
          [outerDimsA, outerDimsB, outerDims],
          isChannelsLast
        );
        return `
  ${shaderHelper.registerUniforms(uniforms).registerInternalVariables(batchDims).declareVariables(...inputVariables, output)}
  ${declareFunctions}
  ${isVec4 ? makeMatMulPackedVec4Source(elementsPerThread, workgroupSize, dataType, batchDims) : makeMatMulPackedSource(elementsPerThread, workgroupSize, dataType, batchDims)}
                   `;
      };
      return {
        name: "MatMul",
        shaderCache: {
          hint: `${elementsPerThread};${activationAttributes.activation};${isVec4};${isChannelsLast}`,
          inputDependencies
        },
        getRunData: () => ({
          outputs: [
            {
              dims: squeezeOutputShapeFunction ? squeezeOutputShapeFunction(outputShape) : outputShape,
              dataType: inputs[0].dataType
            }
          ],
          dispatchGroup: { x: dispatch[0], y: dispatch[1], z: dispatch[2] },
          programUniforms
        }),
        getShaderSource
      };
    };
  }
});

// web/lib/wasm/jsep/webgpu/ops/3rd-party/conv2d_mm_webgpu.ts
var conv2dCommonSnippet, createConv2DMatMulProgramInfo;
var init_conv2d_mm_webgpu = __esm({
  "web/lib/wasm/jsep/webgpu/ops/3rd-party/conv2d_mm_webgpu.ts"() {
    "use strict";
    init_wasm_common();
    init_log();
    init_common();
    init_fuse_utils();
    init_activation_util();
    init_conv_util();
    init_matmul_packed_webgpu();
    conv2dCommonSnippet = (isChannelsLast, fitAOuter, fitBOuter, fitInner, addBias = false, attributes, innerElementSizeX = 4, innerElementSizeW = 4, innerElementSize = 4, dataType = "f32") => {
      const getXSnippet = (innerElementSize2) => {
        switch (innerElementSize2) {
          case 1:
            return "resData = x[xIndex];";
          case 3:
            return `resData = vec3<${dataType}>(x[xIndex], x[xIndex + 1], x[xIndex + 2]);`;
          case 4:
            return "resData = x[xIndex / 4];";
          default:
            throw new Error(`innerElementSize ${innerElementSize2} is not supported.`);
        }
      };
      const getWSnippet = (innerElementSize2) => {
        switch (innerElementSize2) {
          case 1:
            return "return w[row * i32(uniforms.w_shape[3]) + colIn];";
          case 4:
            return "return w[row * i32(uniforms.w_shape[3]) / 4 + colIn];";
          default:
            throw new Error(`innerElementSize ${innerElementSize2} is not supported.`);
        }
      };
      const coordASnippet = isChannelsLast ? `
    let coord = vec4<i32>(batch, xRow, xCol, xCh);
    ` : `
    let coord = vec4<i32>(batch, xCh, xRow, xCol);
    `;
      const coordResSnippet = isChannelsLast ? `
    let coords = vec4<i32>(
      batch,
      row / outWidth,
      row % outWidth,
      col);
    ` : `
    let coords = vec4<i32>(
      batch,
      row,
      col / outWidth,
      col % outWidth);
    `;
      const xHeight = isChannelsLast ? "i32(uniforms.x_shape[1])" : "i32(uniforms.x_shape[2])";
      const xWidth = isChannelsLast ? "i32(uniforms.x_shape[2])" : "i32(uniforms.x_shape[3])";
      const row = isChannelsLast ? "row" : "col";
      const col = isChannelsLast ? "col" : "row";
      const readXSnippet = `
    let inChannels = i32(uniforms.w_shape[2]);
    let outWidth = ${isChannelsLast ? "i32(uniforms.result_shape[2])" : "i32(uniforms.result_shape[3])"};
    let outRow = ${row} / outWidth;
    let outCol = ${row} % outWidth;

    let WRow = ${col} / (i32(uniforms.w_shape[1]) * inChannels);
    let WCol = ${col} / inChannels % i32(uniforms.w_shape[1]);
    let xRow = outRow * uniforms.stride[0] + uniforms.dilation[0] * WRow - uniforms.pad[0];
    let xCol = outCol * uniforms.stride[1] + uniforms.dilation[1] * WCol - uniforms.pad[1];
    let xCh = ${col} % inChannels;
    var resData = ${typeSnippet(innerElementSizeX, dataType)}(0.0);
    // The bounds checking is always needed since we use it to pad zero for
    // the 'same' padding type.
    if (xRow >= 0 && xRow < ${xHeight} && xCol >= 0 && xCol < ${xWidth}) {
      ${coordASnippet}
      let xIndex = getIndexFromCoords4D(coord, vec4<i32>(uniforms.x_shape));
      ${getXSnippet(innerElementSizeX)}
    }
    return resData;`;
      const sampleX = isChannelsLast ? fitAOuter && fitInner ? `
    let col = colIn * ${innerElementSizeX};
    ${readXSnippet}` : `
    let col = colIn * ${innerElementSizeX};
    if (row < uniforms.dim_a_outer && col < uniforms.dim_inner) {
      ${readXSnippet}
    }
    return ${typeSnippet(innerElementSizeX, dataType)}(0.0);` : fitInner && fitBOuter ? `
    let col = colIn * ${innerElementSizeX};
    ${readXSnippet}` : `
    let col = colIn * ${innerElementSizeX};
    if (row < uniforms.dim_inner && col < uniforms.dim_b_outer) {
      ${readXSnippet}
    }
    return ${typeSnippet(innerElementSizeX, dataType)}(0.0);`;
      const sampleW = `${getWSnippet(innerElementSizeW)}`;
      const resType = typeSnippet(innerElementSize, dataType);
      const aType = isChannelsLast ? typeSnippet(innerElementSizeX, dataType) : typeSnippet(innerElementSizeW, dataType);
      const bType = isChannelsLast ? typeSnippet(innerElementSizeW, dataType) : typeSnippet(innerElementSizeX, dataType);
      const applyActivation = getActivationSnippet(attributes, resType, dataType);
      const userCode = `
    fn mm_readA(batch: i32, row : i32, colIn : i32) -> ${aType} {
      ${isChannelsLast ? sampleX : sampleW}
    }

    fn mm_readB(batch: i32, row : i32, colIn : i32) -> ${bType} {
      ${isChannelsLast ? sampleW : sampleX}
    }

    fn mm_write(batch: i32, row : i32, colIn : i32, valueIn : ${resType}) {
      let col = colIn * ${innerElementSize};
      if (row < uniforms.dim_a_outer && col < uniforms.dim_b_outer)
      {
      var value = valueIn;
      let outWidth = ${isChannelsLast ? "i32(uniforms.result_shape[2])" : "i32(uniforms.result_shape[3])"};
      ${coordResSnippet}
      ${biasSnippet(addBias)}
      ${applyActivation}
      setOutputAtCoords(coords[0], coords[1], coords[2], coords[3], value);
      }
    }`;
      return userCode;
    };
    createConv2DMatMulProgramInfo = (inputs, attributes, outputShape, dimAOuter, dimBOuter, dimInner, hasBias, sequentialAccessByThreads, squeezeOutputShapeFunction) => {
      const isChannelsLast = attributes.format === "NHWC";
      const inChannels = isChannelsLast ? inputs[0].dims[3] : inputs[0].dims[1];
      const batchSize = outputShape[0];
      const outWidth = isChannelsLast ? outputShape[2] : outputShape[3];
      const outHeight = isChannelsLast ? outputShape[1] : outputShape[2];
      const outChannels = isChannelsLast ? outputShape[3] : outputShape[1];
      const isVec4 = isChannelsLast && (inChannels % 4 === 0 || inChannels % 3 === 0) && outChannels % 4 === 0;
      const dispatchX = isChannelsLast ? outChannels : outWidth * outHeight;
      const dispatchY = isChannelsLast ? outWidth * outHeight : outChannels;
      const workGroupSize = [8, 8, 1];
      const elementsPerThread = dimAOuter <= 8 ? [4, 1, 1] : [4, 4, 1];
      const dispatch = [
        Math.ceil(dispatchX / workGroupSize[0] / elementsPerThread[0]),
        Math.ceil(dispatchY / workGroupSize[1] / elementsPerThread[1]),
        Math.ceil(batchSize / workGroupSize[2] / elementsPerThread[2])
      ];
      LOG_DEBUG("verbose", () => `[conv2d_mm_webgpu] dispatch = ${dispatch}`);
      const innerElementSize = isVec4 ? isChannelsLast && inChannels % 4 !== 0 ? 3 : 4 : 1;
      const tileAOuter = workGroupSize[1] * elementsPerThread[1];
      const tileBOuter = workGroupSize[0] * elementsPerThread[0];
      const tileInner = Math.max(workGroupSize[0] * innerElementSize, workGroupSize[1]);
      const fitAOuter = dimAOuter % tileAOuter === 0;
      const fitBOuter = dimBOuter % tileBOuter === 0;
      const fitInner = dimInner % tileInner === 0;
      const elementsSize = isVec4 ? [innerElementSize, 4, 4] : [1, 1, 1];
      const programUniforms = [
        { type: 6 /* int32 */, data: dimAOuter },
        { type: 6 /* int32 */, data: dimBOuter },
        { type: 6 /* int32 */, data: dimInner },
        { type: 6 /* int32 */, data: [attributes.pads[0], attributes.pads[1]] },
        { type: 6 /* int32 */, data: attributes.strides },
        { type: 6 /* int32 */, data: attributes.dilations }
      ];
      appendActivationUniformsData(attributes, programUniforms);
      programUniforms.push(...createTensorShapeVariables(inputs[0].dims, inputs[1].dims));
      const inputDependencies = ["rank", "rank"];
      if (hasBias) {
        programUniforms.push(...createTensorShapeVariables(inputs[2].dims));
        inputDependencies.push("rank");
      }
      programUniforms.push(...createTensorShapeVariables(outputShape));
      const getShaderSource = (shaderHelper) => {
        const uniforms = [
          { name: "dim_a_outer", type: "i32" },
          { name: "dim_b_outer", type: "i32" },
          { name: "dim_inner", type: "i32" },
          { name: "pad", type: "i32", length: 2 },
          { name: "stride", type: "i32", length: 2 },
          { name: "dilation", type: "i32", length: 2 }
        ];
        appendActivationUniforms(attributes, uniforms);
        const components = isVec4 ? 4 : 1;
        const t = tensorTypeToWsglStorageType(inputs[0].dataType);
        let declareFunctions = `
      fn setOutputAtIndex(flatIndex : i32, value : ${isVec4 ? `vec4<${t}>` : t}) {
        result[flatIndex] = ${isVec4 ? `vec4<${t}>` : t}(value);
      }
      fn setOutputAtCoords(d0 : i32, d1 : i32, d2 : i32, d3 : i32, value : ${isVec4 ? `vec4<${t}>` : t}) {
        let flatIndex = getOutputIndexFromCoords(vec4<i32>(d0, d1, d2, d3));
        setOutputAtIndex(flatIndex ${isVec4 ? "/ 4" : ""}, value);
      }`;
        const x = inputVariable(
          "x",
          inputs[0].dataType,
          inputs[0].dims.length,
          innerElementSize === 3 ? 1 : innerElementSize
        );
        const w = inputVariable("w", inputs[1].dataType, inputs[1].dims.length, components);
        const inputVariables = [x, w];
        const output = outputVariable("result", inputs[0].dataType, outputShape.length, components);
        if (hasBias) {
          const bias = inputVariable("bias", inputs[2].dataType, inputs[2].dims.length, components);
          inputVariables.push(bias);
          declareFunctions += `
        fn getBiasByOutputCoords(coords : vec4<i32>) -> ${isVec4 ? `vec4<${t}>` : t} {
          return bias[coords.${isChannelsLast ? "w" : "y"}${isVec4 ? "/ 4" : ""}];
        }`;
        }
        return `
        ${utilFunctions("uniforms.result_strides")}
        //struct Uniforms { xShape : vec4<i32>, wShape : vec4<i32>, outShape : vec4<i32>,
        //  outShapeStrides: vec3<i32>, filterDims : vec2<i32>, pad : vec2<i32>, stride : vec2<i32>,
        //  dilation : vec2<i32>, dimAOuter : i32, dimBOuter : i32, dimInner : i32 };
        ${shaderHelper.registerUniforms(uniforms).declareVariables(...inputVariables, output)}
        ${declareFunctions}
        ${conv2dCommonSnippet(
          isChannelsLast,
          fitAOuter,
          fitBOuter,
          fitInner,
          hasBias,
          attributes,
          elementsSize[0],
          elementsSize[1],
          elementsSize[2],
          t
        )}
        ${isVec4 ? makeMatMulPackedVec4Source(elementsPerThread, workGroupSize, t, void 0, !isChannelsLast, tileInner) : makeMatMulPackedSource(
          elementsPerThread,
          workGroupSize,
          t,
          void 0,
          !isChannelsLast,
          tileInner,
          false,
          void 0,
          sequentialAccessByThreads
        )}`;
      };
      return {
        name: "Conv2DMatMul",
        shaderCache: {
          hint: `${attributes.cacheKey};${innerElementSize};${isVec4};${fitAOuter};${fitBOuter};${fitInner};${tileAOuter};${tileBOuter};${tileInner}`,
          inputDependencies
        },
        getRunData: () => ({
          outputs: [
            {
              dims: squeezeOutputShapeFunction ? squeezeOutputShapeFunction(outputShape) : outputShape,
              dataType: inputs[0].dataType
            }
          ],
          dispatchGroup: { x: dispatch[0], y: dispatch[1], z: dispatch[2] },
          programUniforms
        }),
        getShaderSource
      };
    };
  }
});

// web/lib/wasm/jsep/webgpu/ops/3rd-party/conv3d_naive_webgpu.ts
var arrayProduct, parse3TupleParam, getEffectiveFilterSize, computeDefaultPad, computeOutputShape4D, get3DPadAndOutInfo, computeConv3DInfo, createConv3DNaiveProgramInfo;
var init_conv3d_naive_webgpu = __esm({
  "web/lib/wasm/jsep/webgpu/ops/3rd-party/conv3d_naive_webgpu.ts"() {
    "use strict";
    init_wasm_common();
    init_log();
    init_util();
    init_common();
    init_fuse_utils();
    init_activation_util();
    arrayProduct = (arr) => {
      let product = 1;
      for (let i = 0; i < arr.length; i++) {
        product *= arr[i];
      }
      return product;
    };
    parse3TupleParam = (param) => typeof param === "number" ? [param, param, param] : param;
    getEffectiveFilterSize = (filterSize, dilation) => {
      if (dilation <= 1) {
        return filterSize;
      }
      return filterSize + (filterSize - 1) * (dilation - 1);
    };
    computeDefaultPad = (inputShape, fieldSize, stride, dilation = 1) => {
      const effectiveFieldSize = getEffectiveFilterSize(fieldSize, dilation);
      return Math.floor((inputShape[0] * (stride - 1) - stride + effectiveFieldSize) / 2);
    };
    computeOutputShape4D = (inShape, filterShape, outChannels, strides, zeroPad) => {
      if (zeroPad == null) {
        zeroPad = computeDefaultPad(inShape, filterShape[0], strides[0]);
      }
      const outShape = [0, 0, 0, outChannels];
      for (let index = 0; index < 3; index++) {
        if (inShape[index] + 2 * zeroPad >= filterShape[index]) {
          outShape[index] = Math.trunc((inShape[index] - filterShape[index] + 2 * zeroPad) / strides[index] + 1);
        }
      }
      return outShape;
    };
    get3DPadAndOutInfo = (pad2, inDepth, inHeight, inWidth, strideDepth, strideHeight, strideWidth, filterDepth, filterHeight, filterWidth) => {
      let padInfo;
      let outDepth;
      let outHeight;
      let outWidth;
      if (pad2 === "VALID") {
        pad2 = 0;
      }
      if (typeof pad2 === "number") {
        padInfo = { top: pad2, bottom: pad2, left: pad2, right: pad2, front: pad2, back: pad2 };
        const outShape = computeOutputShape4D(
          [inDepth, inHeight, inWidth, 1],
          [filterDepth, filterHeight, filterWidth],
          1,
          [strideDepth, strideHeight, strideWidth],
          pad2
        );
        outDepth = outShape[0];
        outHeight = outShape[1];
        outWidth = outShape[2];
      } else if (Array.isArray(pad2)) {
        if (!pad2.every((val, _, arr) => val === arr[0])) {
          throw Error(`Unsupported padding parameter: ${pad2}`);
        }
        padInfo = { top: pad2[0], bottom: pad2[1], left: pad2[2], right: pad2[3], front: pad2[4], back: pad2[5] };
        const outShape = computeOutputShape4D(
          [inDepth, inHeight, inWidth, 1],
          [filterDepth, filterHeight, filterWidth],
          1,
          [strideDepth, strideHeight, strideWidth],
          pad2[0]
        );
        outDepth = outShape[0];
        outHeight = outShape[1];
        outWidth = outShape[2];
      } else if (pad2 === "SAME_UPPER") {
        outDepth = Math.ceil(inDepth / strideDepth);
        outHeight = Math.ceil(inHeight / strideHeight);
        outWidth = Math.ceil(inWidth / strideWidth);
        const padAlongDepth = (outDepth - 1) * strideDepth + filterDepth - inDepth;
        const padAlongHeight = (outHeight - 1) * strideHeight + filterHeight - inHeight;
        const padAlongWidth = (outWidth - 1) * strideWidth + filterWidth - inWidth;
        const front = Math.floor(padAlongDepth / 2);
        const back = padAlongDepth - front;
        const top = Math.floor(padAlongHeight / 2);
        const bottom = padAlongHeight - top;
        const left = Math.floor(padAlongWidth / 2);
        const right = padAlongWidth - left;
        padInfo = { top, bottom, left, right, front, back };
      } else {
        throw Error(`Unknown padding parameter: ${pad2}`);
      }
      return { padInfo, outDepth, outHeight, outWidth };
    };
    computeConv3DInfo = (inShape, filterShape, strides, dilations, pad2, depthwise = false, dataFormat = "channelsLast") => {
      let batchSize, inDepth, inHeight, inWidth, inChannels;
      if (dataFormat === "channelsLast") {
        [batchSize, inDepth, inHeight, inWidth, inChannels] = inShape;
      } else if (dataFormat === "channelsFirst") {
        [batchSize, inChannels, inDepth, inHeight, inWidth] = inShape;
      } else {
        throw new Error(`Unknown dataFormat ${dataFormat}`);
      }
      const [filterChannels, , filterDepth, filterHeight, filterWidth] = filterShape;
      const [strideDepth, strideHeight, strideWidth] = parse3TupleParam(strides);
      const [dilationDepth, dilationHeight, dilationWidth] = parse3TupleParam(dilations);
      const effectiveFilterDepth = getEffectiveFilterSize(filterDepth, dilationDepth);
      const effectiveFilterHeight = getEffectiveFilterSize(filterHeight, dilationHeight);
      const effectiveFilterWidth = getEffectiveFilterSize(filterWidth, dilationWidth);
      const { padInfo, outDepth, outHeight, outWidth } = get3DPadAndOutInfo(
        pad2,
        inDepth,
        inHeight,
        inWidth,
        strideDepth,
        strideHeight,
        strideWidth,
        effectiveFilterDepth,
        effectiveFilterHeight,
        effectiveFilterWidth
      );
      const outChannels = depthwise ? filterChannels * inChannels : filterChannels;
      let outShape = [0, 0, 0, 0, 0];
      if (dataFormat === "channelsFirst") {
        outShape = [batchSize, outChannels, outDepth, outHeight, outWidth];
      } else if (dataFormat === "channelsLast") {
        outShape = [batchSize, outDepth, outHeight, outWidth, outChannels];
      }
      return {
        batchSize,
        dataFormat,
        inDepth,
        inHeight,
        inWidth,
        inChannels,
        outDepth,
        outHeight,
        outWidth,
        outChannels,
        padInfo,
        strideDepth,
        strideHeight,
        strideWidth,
        filterDepth,
        filterHeight,
        filterWidth,
        effectiveFilterDepth,
        effectiveFilterHeight,
        effectiveFilterWidth,
        dilationDepth,
        dilationHeight,
        dilationWidth,
        inShape,
        outShape,
        filterShape
      };
    };
    createConv3DNaiveProgramInfo = (inputs, attributes, outputShape, filterDims, pads, dataFormat) => {
      const isChannelLast = dataFormat === "channelsLast";
      const inChannels = isChannelLast ? inputs[0].dims[3] : inputs[0].dims[1];
      const isVec4 = false;
      const workGroupSize = [64, 1, 1];
      const dispatchLayout = { x: outputShape.map((_, i) => i) };
      const dispatch = [Math.ceil(arrayProduct(dispatchLayout.x.map((d) => outputShape[d])) / workGroupSize[0]), 1, 1];
      LOG_DEBUG("verbose", () => `[conv3d_naive_webgpu] dispatch = ${dispatch}`);
      const innerElementSize = isVec4 ? isChannelLast && inChannels % 4 !== 0 ? 3 : 4 : 1;
      const outputSize = ShapeUtil.size(outputShape);
      const programUniforms = [
        { type: 12 /* uint32 */, data: outputSize },
        { type: 12 /* uint32 */, data: filterDims },
        { type: 12 /* uint32 */, data: pads },
        { type: 12 /* uint32 */, data: attributes.strides },
        { type: 12 /* uint32 */, data: attributes.dilations }
      ];
      appendActivationUniformsData(attributes, programUniforms);
      programUniforms.push(...createTensorShapeVariables(inputs[0].dims, inputs[1].dims));
      const inputDependencies = ["rank", "rank"];
      const hasBias = inputs.length === 3;
      if (hasBias) {
        programUniforms.push(...createTensorShapeVariables(inputs[2].dims));
        inputDependencies.push("rank");
      }
      programUniforms.push(...createTensorShapeVariables(outputShape));
      const getShaderSource = (shaderHelper) => {
        const uniforms = [
          { name: "output_size", type: "u32" },
          { name: "filter_dims", type: "u32", length: filterDims.length },
          { name: "pads", type: "u32", length: pads.length },
          { name: "strides", type: "u32", length: attributes.strides.length },
          { name: "dilations", type: "u32", length: attributes.dilations.length }
        ];
        appendActivationUniforms(attributes, uniforms);
        const components = isVec4 ? 4 : 1;
        const t = tensorTypeToWsglStorageType(inputs[0].dataType);
        const x = inputVariable(
          "x",
          inputs[0].dataType,
          inputs[0].dims.length,
          innerElementSize === 3 ? 1 : innerElementSize
        );
        const w = inputVariable("W", inputs[1].dataType, inputs[1].dims.length, components);
        const inputVariables = [x, w];
        const output = outputVariable("result", inputs[0].dataType, outputShape.length, components);
        let declareFunctions = "";
        if (hasBias) {
          const bias = inputVariable("bias", inputs[2].dataType, inputs[2].dims.length, components);
          inputVariables.push(bias);
          declareFunctions += `
        fn getBiasByOutputCoords(coords : array<u32, 5>) -> ${isVec4 ? `vec4<${t}>` : t} {
          return bias[${isChannelLast ? getElementAt("coords", 4, 5) : getElementAt("coords", 1, 5)}${isVec4 ? "/ 4" : ""}];
        }`;
        }
        const resType = typeSnippet(innerElementSize, t);
        const applyActivation = getActivationSnippet(attributes, resType, t);
        return `
            ${declareFunctions}
            fn getX(d0 : u32, d1 : u32, d2 : u32, d3 : u32, d4 : u32) -> f32 {
              let aIndices = array<u32, 5>(d0, d1, d2, d3, d4);
              return ${x.getByIndices("aIndices")};
            }
            fn getW(d0 : u32, d1 : u32, d2 : u32, d3 : u32, d4 : u32) -> f32 {
              let aIndices = array<u32, 5>(d0, d1, d2, d3, d4);
              return ${w.getByIndices("aIndices")};
            }
          ${shaderHelper.registerUniforms(uniforms).declareVariables(...inputVariables, output)}
          ${shaderHelper.mainStart()}
          ${shaderHelper.guardAgainstOutOfBoundsWorkgroupSizes("uniforms.output_size")}
              let coords = ${output.offsetToIndices("global_idx")};
              let batch = ${getElementAt("coords", 0, x.rank)};
              let d2 = ${isChannelLast ? getElementAt("coords", x.rank - 1, x.rank) : getElementAt("coords", 1, x.rank)};
              let xFRCCorner = vec3<u32>(${isChannelLast ? getElementAt("coords", 1, x.rank) : getElementAt("coords", 2, x.rank)},
              ${isChannelLast ? getElementAt("coords", 2, x.rank) : getElementAt("coords", 3, x.rank)},
              ${isChannelLast ? getElementAt("coords", 3, x.rank) : getElementAt("coords", 4, x.rank)}) * uniforms.strides - uniforms.pads;
              let xFCorner = xFRCCorner.x;
              let xRCorner = xFRCCorner.y;
              let xCCorner = xFRCCorner.z;
              let xShapeY = ${isChannelLast ? getElementAt("uniforms.x_shape", 1, x.rank) : getElementAt("uniforms.x_shape", 2, x.rank)};
              let xShapeZ = ${isChannelLast ? getElementAt("uniforms.x_shape", 2, x.rank) : getElementAt("uniforms.x_shape", 3, x.rank)};
              let xShapeW = ${isChannelLast ? getElementAt("uniforms.x_shape", 3, x.rank) : getElementAt("uniforms.x_shape", 4, x.rank)};
              let xShapeU = ${isChannelLast ? getElementAt("uniforms.x_shape", 4, x.rank) : getElementAt("uniforms.x_shape", 1, x.rank)};
              let inputDepthNearestVec4 = (xShapeU / 4) * 4;
              let inputDepthVec4Remainder = xShapeU % 4;

              var value = 0.0;
              for (var wF = 0u; wF < uniforms.filter_dims[0]; wF++) {
                let xF = xFCorner + wF * uniforms.dilations[0];
                if (xF < 0 || xF >= xShapeY) {
                  continue;
                }

                for (var wR = 0u; wR < uniforms.filter_dims[1]; wR++) {
                  let xR = xRCorner + wR * uniforms.dilations[1];
                  if (xR < 0 || xR >= xShapeZ) {
                    continue;
                  }

                  for (var wC = 0u; wC < uniforms.filter_dims[2]; wC++) {
                    let xC = xCCorner + wC * uniforms.dilations[2];
                    if (xC < 0 || xC >= xShapeW) {
                      continue;
                    }

                    for (var d1 = 0u; d1 < inputDepthNearestVec4; d1 += 4) {
                      ${isChannelLast ? `let xValues = vec4<f32>(
                               getX(batch, xF, xR, xC, d1),
                               getX(batch, xF, xR, xC, d1 + 1),
                               getX(batch, xF, xR, xC, d1 + 2),
                               getX(batch, xF, xR, xC, d1 + 3));
                            ` : `let xValues = vec4<f32>(
                               getX(batch, d1, xF, xR, xC),
                               getX(batch, d1 + 1, xF, xR, xC),
                               getX(batch, d1 + 2, xF, xR, xC),
                               getX(batch, d1 + 3, xF, xR, xC));
                            `}
                            let wValues = vec4<f32>(
                              getW(d2, d1, wF, wR, wC),
                              getW(d2, d1 + 1, wF, wR, wC),
                              getW(d2, d1 + 2, wF, wR, wC),
                              getW(d2, d1 + 3, wF, wR, wC));
                      value += dot(xValues, wValues);
                    }
                    if (inputDepthVec4Remainder == 1) {
                        ${isChannelLast ? `value += getX(batch, xF, xR, xC, inputDepthNearestVec4)
                          * getW(d2, inputDepthNearestVec4, wF, wR, wC);` : `value += getX(batch, inputDepthNearestVec4, xF, xR, xC)
                          * getW(d2, inputDepthNearestVec4, wF, wR, wC);`}
                    } else if (inputDepthVec4Remainder == 2) {
                      ${isChannelLast ? `let xValues = vec2<f32>(
                        getX(batch, xF, xR, xC, inputDepthNearestVec4),
                        getX(batch, xF, xR, xC, inputDepthNearestVec4 + 1));
                      ` : `let xValues = vec2<f32>(
                        getX(batch, inputDepthNearestVec4, xF, xR, xC),
                        getX(batch, inputDepthNearestVec4 + 1, xF, xR, xC));
                    `}
                    let wValues = vec2<f32>(
                      getW(d2, inputDepthNearestVec4, wF, wR, wC),
                      getW(d2, inputDepthNearestVec4 + 1, wF, wR, wC));
                      value += dot(xValues, wValues);
                    } else if (inputDepthVec4Remainder == 3) {
                      ${isChannelLast ? `let xValues = vec3<f32>(
                        getX(batch, xF, xR, xC, inputDepthNearestVec4),
                        getX(batch, xF, xR, xC, inputDepthNearestVec4 + 1),
                        getX(batch, xF, xR, xC, inputDepthNearestVec4 + 2));
                      ` : `let xValues = vec3<f32>(
                        getX(batch, inputDepthNearestVec4, xF, xR, xC),
                        getX(batch, inputDepthNearestVec4 + 1, xF, xR, xC),
                        getX(batch, inputDepthNearestVec4 + 2, xF, xR, xC));
                    `}
                    let wValues = vec3<f32>(
                      getW(d2, inputDepthNearestVec4, wF, wR, wC),
                      getW(d2, inputDepthNearestVec4 + 1, wF, wR, wC),
                      getW(d2, inputDepthNearestVec4 + 2, wF, wR, wC));
                      value += dot(xValues, wValues);
                    }
                  }
                }
              }
              ${hasBias ? "value = value + getBiasByOutputCoords(coords)" : ""};
              ${applyActivation}
              result[global_idx] = f32(value);
          }`;
      };
      return {
        name: "Conv3DNaive",
        shaderCache: { hint: `${attributes.cacheKey};${isChannelLast};${innerElementSize};${hasBias}`, inputDependencies },
        getRunData: () => ({
          outputs: [{ dims: outputShape, dataType: inputs[0].dataType }],
          dispatchGroup: { x: dispatch[0], y: dispatch[1], z: dispatch[2] },
          programUniforms
        }),
        getShaderSource
      };
    };
  }
});

// web/lib/wasm/jsep/webgpu/ops/conv-grouped.ts
var createGroupedConvProgramInfo, createGroupedConvVectorizeProgramInfo;
var init_conv_grouped = __esm({
  "web/lib/wasm/jsep/webgpu/ops/conv-grouped.ts"() {
    "use strict";
    init_wasm_common();
    init_util();
    init_common();
    init_conv();
    init_fuse_utils();
    createGroupedConvProgramInfo = (inputs, attributes, squeezeOutputShapeFunction) => {
      const hasBias = inputs.length > 2;
      const processBias = hasBias ? "value += b[output_channel];" : "";
      const xShape = inputs[0].dims;
      const wShape = inputs[1].dims;
      const outputChannelsPerGroup = wShape[0] / attributes.group;
      const isChannelLast = attributes.format === "NHWC";
      const outputShape = calculateOutputShape(
        xShape,
        wShape,
        attributes.dilations,
        attributes.pads,
        attributes.strides,
        isChannelLast
      );
      const outputSize = ShapeUtil.size(outputShape);
      const programUniforms = [
        { type: 12 /* uint32 */, data: outputSize },
        { type: 12 /* uint32 */, data: attributes.dilations },
        { type: 12 /* uint32 */, data: [attributes.strides[0], attributes.strides[1]] },
        { type: 12 /* uint32 */, data: [attributes.pads[0], attributes.pads[1]] },
        { type: 12 /* uint32 */, data: outputChannelsPerGroup }
      ];
      appendActivationUniformsData(attributes, programUniforms);
      programUniforms.push(...createTensorShapeVariables(xShape, wShape));
      const inputDependencies = ["rank", "rank"];
      if (hasBias) {
        programUniforms.push(...createTensorShapeVariables(inputs[2].dims));
        inputDependencies.push("rank");
      }
      programUniforms.push(...createTensorShapeVariables(outputShape));
      const getShaderSource = (shaderHelper) => {
        const output = outputVariable("output", inputs[0].dataType, outputShape.length);
        const baseType = tensorTypeToWsglStorageType(output.type.tensor);
        const applyActivation = getActivationSnippet(attributes, output.type.value, baseType);
        const x = inputVariable("x", inputs[0].dataType, xShape.length);
        const w = inputVariable("w", inputs[1].dataType, wShape.length);
        const inputVars = [x, w];
        if (hasBias) {
          inputVars.push(inputVariable("b", inputs[2].dataType, inputs[2].dims.length));
        }
        const uniforms = [
          { name: "output_size", type: "u32" },
          { name: "dilations", type: "u32", length: attributes.dilations.length },
          { name: "strides", type: "u32", length: 2 },
          { name: "pads", type: "u32", length: 2 },
          { name: "output_channels_per_group", type: "u32" }
        ];
        appendActivationUniforms(attributes, uniforms);
        return `
  ${shaderHelper.registerUniforms(uniforms).declareVariables(...inputVars, output)}

  ${shaderHelper.mainStart()}
    ${shaderHelper.guardAgainstOutOfBoundsWorkgroupSizes("uniforms.output_size")}

    let outputIndices = ${output.offsetToIndices("global_idx")};
    let batch: u32 = outputIndices[0];
    let output_channel: u32 = outputIndices[${isChannelLast ? 3 : 1}];
    let xRCCorner: vec2<u32> = vec2<u32>(outputIndices[${isChannelLast ? 1 : 2}], outputIndices[${isChannelLast ? 2 : 3}]) * uniforms.strides - uniforms.pads;
    let group_id: u32 = output_channel / uniforms.output_channels_per_group;

    var value: ${output.type.value} = ${output.type.value}(0);
    for (var wInChannel: u32 = 0u; wInChannel < uniforms.w_shape[1]; wInChannel++) {
      let input_channel = group_id * uniforms.w_shape[1] + wInChannel;
      for (var wHeight: u32 = 0u; wHeight < uniforms.w_shape[2]; wHeight++) {
        let xHeight = xRCCorner.x + wHeight * uniforms.dilations[0];

        if (xHeight < 0u || xHeight >= uniforms.x_shape[${isChannelLast ? 1 : 2}]) {
          continue;
        }

        for (var wWidth: u32 = 0u; wWidth < uniforms.w_shape[3]; wWidth++) {
          let xWidth = xRCCorner.y + wWidth * uniforms.dilations[1];
          if (xWidth < 0u || xWidth >= uniforms.x_shape[${isChannelLast ? 2 : 3}]) {
            continue;
          }

          let xVal = ${isChannelLast ? x.get("batch", "xHeight", "xWidth", "input_channel") : x.get("batch", "input_channel", "xHeight", "xWidth")};
          let wVal = ${w.get("output_channel", "wInChannel", "wHeight", "wWidth")};
          value += xVal*wVal;
        }
      }
    }
    ${processBias}
    ${applyActivation}
    ${output.setByOffset("global_idx", "value")}
  }`;
      };
      return {
        name: "GroupedConv",
        shaderCache: { hint: attributes.cacheKey, inputDependencies },
        getRunData: () => ({
          outputs: [
            {
              dims: squeezeOutputShapeFunction ? squeezeOutputShapeFunction(outputShape) : outputShape,
              dataType: inputs[0].dataType
            }
          ],
          dispatchGroup: { x: Math.ceil(
            outputSize / 64
            /* workgroup size */
          ) },
          programUniforms
        }),
        getShaderSource
      };
    };
    createGroupedConvVectorizeProgramInfo = (inputs, attributes, outputShape, squeezeOutputShapeFunction) => {
      const hasBias = inputs.length > 2;
      const components = getMaxComponents(outputShape[3]);
      const outputNumber = getMaxComponents(outputShape[2]);
      const outputSize = ShapeUtil.size(outputShape) / components / outputNumber;
      const xShape = [inputs[0].dims[0], inputs[0].dims[1], inputs[0].dims[2], inputs[0].dims[3] / components];
      const wShape = [inputs[1].dims[0], inputs[1].dims[1], inputs[1].dims[2], inputs[1].dims[3] / components];
      const outputShapeInShader = [outputShape[0], outputShape[1], outputShape[2], outputShape[3] / components];
      const programUniforms = [
        { type: 12 /* uint32 */, data: outputSize },
        { type: 6 /* int32 */, data: [attributes.strides[0], attributes.strides[1]] },
        { type: 6 /* int32 */, data: [attributes.pads[0], attributes.pads[1]] }
      ];
      appendActivationUniformsData(attributes, programUniforms);
      programUniforms.push(...createTensorShapeVariables(xShape, wShape, outputShapeInShader));
      const xNumber = (outputNumber - 1) * attributes.strides[1] + wShape[1];
      const getShaderSource = (shaderHelper) => {
        const output = outputVariable("output", inputs[0].dataType, outputShapeInShader.length, components);
        const baseType = tensorTypeToWsglStorageType(output.type.tensor);
        const applyActivation = getActivationSnippet(attributes, output.type.value, baseType);
        const x = inputVariable("x", inputs[0].dataType, xShape.length, components);
        const w = inputVariable("w", inputs[1].dataType, wShape.length, components);
        const inputVars = [x, w];
        if (hasBias) {
          inputVars.push(inputVariable("b", inputs[2].dataType, inputs[2].dims, components));
        }
        const processBias = hasBias ? "value += b[output_channel];" : "";
        const uniforms = [
          { name: "output_size", type: "u32" },
          { name: "strides", type: "i32", length: 2 },
          { name: "pads", type: "i32", length: 2 }
        ];
        appendActivationUniforms(attributes, uniforms);
        return `
  ${shaderHelper.registerUniforms(uniforms).declareVariables(...inputVars, output)}
  ${shaderHelper.mainStart()}
    ${shaderHelper.guardAgainstOutOfBoundsWorkgroupSizes("uniforms.output_size")}
    let width0 = uniforms.output_shape[3];
    let output_channel = global_idx % width0;
    var index1 = global_idx / width0;
    let width1 = uniforms.output_shape[2] / ${outputNumber}u;
    let col = (index1 % width1) * ${outputNumber}u;
    index1 = index1 / width1;
    let row = index1 % uniforms.output_shape[1];
    let batch = index1 / uniforms.output_shape[1];

    let x_corner = vec2<i32>(i32(row), i32(col)) * uniforms.strides - uniforms.pads;

    var x_vals: array<${x.type.value}, ${xNumber}>;
    var values: array<${output.type.value}, ${outputNumber}>;
    let input_channel = output_channel;
    // Use constant instead of uniform can give better performance for w's height/width.
    for (var w_height: u32 = 0u; w_height < ${wShape[0]}; w_height++) {
      let x_height = x_corner.x + i32(w_height);
      if (x_height >= 0 && u32(x_height) < uniforms.x_shape[1]) {
        for (var i = 0; i < ${xNumber}; i++) {
          let x_width = x_corner.y + i;
          if (x_width >= 0 && u32(x_width) < uniforms.x_shape[2]) {
            x_vals[i] = ${x.get("batch", "u32(x_height)", "u32(x_width)", "input_channel")};
          } else {
            x_vals[i] = ${x.type.value}(0);
          }
        }
        for (var w_width: u32 = 0u; w_width < ${wShape[1]}; w_width++) {
          let w_val = ${w.get("w_height", "w_width", "0", "output_channel")};
          for (var i = 0u; i < ${outputNumber}u; i++) {
            values[i] = fma(x_vals[i * u32(uniforms.strides[1]) + w_width], w_val, values[i]);
          }
        }
      }
    }

    for (var i = 0u; i < ${outputNumber}u; i++) {
      var value = values[i];
      ${processBias}
      ${applyActivation}
      ${output.set("batch", "row", "col + i", "output_channel", "value")};
    }
  }`;
      };
      return {
        name: "GroupedConv-Vectorize",
        shaderCache: {
          hint: `${attributes.cacheKey};${components};${outputNumber};${xNumber};${wShape[0]};${wShape[1]}`,
          inputDependencies: hasBias ? ["rank", "rank", "type"] : ["rank", "rank"]
        },
        getRunData: () => ({
          outputs: [
            {
              dims: squeezeOutputShapeFunction ? squeezeOutputShapeFunction(outputShape) : outputShape,
              dataType: inputs[0].dataType
            }
          ],
          dispatchGroup: { x: Math.ceil(
            outputSize / 64
            /* workgroup size */
          ) },
          programUniforms
        }),
        getShaderSource
      };
    };
  }
});

// web/lib/wasm/jsep/webgpu/ops/matmul.ts
var createNaiveMatmulProgramInfo, validateInputs8, matMul;
var init_matmul = __esm({
  "web/lib/wasm/jsep/webgpu/ops/matmul.ts"() {
    "use strict";
    init_wasm_common();
    init_util();
    init_matmul_packed_webgpu();
    init_common();
    init_fuse_utils();
    createNaiveMatmulProgramInfo = (inputs, activationAttributes, outputShape, reshapedOutputShape, isChannelsLast = false, squeezeOutputShapeFunction) => {
      const aShape = inputs[0].dims;
      const bShape = inputs[1].dims;
      const M = aShape[aShape.length - 2];
      const N = bShape[bShape.length - 1];
      const K = aShape[aShape.length - 1];
      const components = getMaxComponents(N);
      const aComponents = getMaxComponents(K);
      const outputNumber = getMaxComponents(M);
      const outputSize = ShapeUtil.size(outputShape) / components / outputNumber;
      const hasBias = inputs.length > 2;
      const outerDims = reshapedOutputShape ? reshapedOutputShape.slice(0, -2) : outputShape.slice(0, -2);
      const batchSize = ShapeUtil.size(outerDims);
      const outputShapeInShader = [batchSize, M, N];
      const programUniforms = [
        { type: 12 /* uint32 */, data: outputSize },
        { type: 12 /* uint32 */, data: M },
        { type: 12 /* uint32 */, data: N },
        { type: 12 /* uint32 */, data: K }
      ];
      appendActivationUniformsData(activationAttributes, programUniforms);
      programUniforms.push(...createTensorShapeVariables(outerDims, aShape, bShape));
      if (hasBias) {
        programUniforms.push(...createTensorShapeVariables(inputs[2].dims));
      }
      programUniforms.push(...createTensorShapeVariables(outputShapeInShader));
      const getShaderSource = (shaderHelper) => {
        const batchDims = internalVariable("batch_dims", inputs[0].dataType, outerDims.length);
        const a = inputVariable("a", inputs[0].dataType, aShape.length, aComponents);
        const b = inputVariable("b", inputs[1].dataType, bShape.length, components);
        const output = outputVariable("output", inputs[0].dataType, outputShapeInShader.length, components);
        const baseType = tensorTypeToWsglStorageType(output.type.tensor);
        const applyActivation = getActivationSnippet(activationAttributes, output.type.value, baseType);
        const inputVariables = [a, b];
        let processBias = "";
        if (hasBias) {
          const biasComponents = isChannelsLast ? components : 1;
          inputVariables.push(inputVariable("bias", inputs[2].dataType, inputs[2].dims.length, biasComponents));
          processBias = `${isChannelsLast ? `value += bias[col / ${biasComponents}];` : `value += ${output.type.value}(bias[row + i]);`}`;
        }
        const outerDimsA = aShape.slice(0, -2);
        const outerDimsB = bShape.slice(0, -2);
        const broadCastADims = getBroadcastDims(outerDimsA, outerDims);
        const broadCastBDims = getBroadcastDims(outerDimsB, outerDims);
        const uniforms = [
          { name: "output_size", type: "u32" },
          { name: "M", type: "u32" },
          { name: "N", type: "u32" },
          { name: "K", type: "u32" }
        ];
        appendActivationUniforms(activationAttributes, uniforms);
        const getIndices = (variable, broadCastDims) => {
          const rank = variable.rank;
          const name = variable.name;
          if (rank === 2) {
            return `var ${name}_indices = ${variable.type.indices}(0u, 0u);`;
          }
          const batchRank = batchDims.rank;
          let resStr = `var ${name}_indices: ${variable.type.indices};`;
          for (let i = rank - 2 - 1, j = batchRank - 1; i >= 0; i--, j--) {
            resStr += `
${name}_indices[${i}] = ${batchRank > 1 ? `batch_indices[${j}]` : "batch_indices"};`;
          }
          broadCastDims.forEach((i) => {
            resStr += `
${name}_indices[${i}] = 0;`;
          });
          resStr += `${name}_indices[${rank - 2}] = 0u;
                     ${name}_indices[${rank - 1}] = 0u;`;
          return resStr;
        };
        const calcResult = () => {
          let calcStr = `var a_data: ${a.type.value};`;
          for (let i = 0; i < aComponents; i++) {
            calcStr += `
              let b_data${i} = b[(b_offset + (k + ${i}) * uniforms.N + col) / ${components}];`;
          }
          for (let i = 0; i < outputNumber; i++) {
            calcStr += `a_data = a[(a_offset + (row + ${i}) * uniforms.K + k) / ${aComponents}];`;
            for (let j = 0; j < aComponents; j++) {
              calcStr += `
            values[${i}] = fma(${b.type.value}(a_data${aComponents === 1 ? "" : `[${j}]`}), b_data${j}, values[${i}]);
`;
            }
          }
          return calcStr;
        };
        return `
  ${shaderHelper.registerUniforms(uniforms).registerInternalVariables(batchDims).declareVariables(...inputVariables, output)}
  ${shaderHelper.mainStart()}
    ${shaderHelper.guardAgainstOutOfBoundsWorkgroupSizes("uniforms.output_size")}
    let col = (global_idx % (uniforms.N / ${components})) * ${components};
    var index1 = global_idx / (uniforms.N / ${components});
    let stride1 = uniforms.M / ${outputNumber};
    let row = (index1 % stride1) * ${outputNumber};
    let batch = index1 / stride1;

    ${outputShape.length === 2 ? "" : `let batch_indices = ${batchDims.offsetToIndices("batch")};`}
    ${getIndices(a, broadCastADims)}
    let a_offset = ${a.indicesToOffset("a_indices")};
    ${getIndices(b, broadCastBDims)}
    let b_offset = ${b.indicesToOffset("b_indices")};
    var values: array<${output.type.value}, ${outputNumber}>;
    for (var k: u32 = 0u; k < uniforms.K; k = k + ${aComponents}) {
      ${calcResult()}
    }
    for (var i = 0u; i < ${outputNumber}u; i++) {
      var value = values[i];
      ${processBias}
      ${applyActivation}
      let cur_indices = ${output.type.indices}(batch, row + i, col);
      let offset = ${output.indicesToOffset("cur_indices")};
      ${output.setByOffset(`offset / ${components}`, "value")};
    }
  }
  `;
      };
      return {
        name: "MatMulNaive",
        shaderCache: {
          hint: `${activationAttributes.activation};${components};${aComponents};${outputNumber};${isChannelsLast}`,
          inputDependencies: hasBias ? ["rank", "rank", "rank"] : ["rank", "rank"]
        },
        getRunData: () => ({
          outputs: [
            {
              dims: squeezeOutputShapeFunction ? squeezeOutputShapeFunction(outputShape) : outputShape,
              dataType: inputs[0].dataType
            }
          ],
          dispatchGroup: { x: Math.ceil(
            outputSize / 64
            /* workgroup size */
          ) },
          programUniforms
        }),
        getShaderSource
      };
    };
    validateInputs8 = (inputs) => {
      if (!inputs || inputs.length !== 2) {
        throw new Error("MatMul requires 2 inputs.");
      }
      if (inputs[0].dims[inputs[0].dims.length - 1] !== inputs[1].dims[inputs[1].dims.length - 2]) {
        throw new Error("shared dimension does not match.");
      }
    };
    matMul = (context) => {
      validateInputs8(context.inputs);
      const outputShape = BroadcastUtil.calcShape(context.inputs[0].dims, context.inputs[1].dims, true);
      if (!outputShape) {
        throw new Error("Can't use matmul on the given tensors");
      }
      const N = outputShape[outputShape.length - 1];
      const K = context.inputs[0].dims[context.inputs[0].dims.length - 1];
      if (N < 8 && K < 8) {
        context.compute(createNaiveMatmulProgramInfo(context.inputs, { activation: "" }, outputShape));
      } else {
        context.compute(createMatmulProgramInfo(context.inputs, { activation: "" }, outputShape));
      }
    };
  }
});

// web/lib/wasm/jsep/webgpu/ops/conv.ts
var calculateOutputShape, weightTransposeAttribute, validateInputs9, getAdjustedConvAttributes, parseConvAttributes, conv2d, conv1d, conv3d, conv;
var init_conv = __esm({
  "web/lib/wasm/jsep/webgpu/ops/conv.ts"() {
    "use strict";
    init_util();
    init_conv2d_mm_webgpu();
    init_conv3d_naive_webgpu();
    init_matmul_packed_webgpu();
    init_conv_grouped();
    init_fuse_utils();
    init_matmul();
    init_transpose();
    calculateOutputShape = (inputShape, kernelShape, dilations, adjustPads, strides, isChannelLast) => {
      const batchSize = inputShape[0];
      const inputSpatialShape = inputShape.slice(isChannelLast ? 1 : 2, isChannelLast ? 3 : 4);
      const spatialRank = inputSpatialShape.length;
      const outChannels = kernelShape[0];
      const kernelSpatialShape = kernelShape.slice(2);
      const dilatedKernelShape = kernelSpatialShape.map((v, i) => v + (v - 1) * (dilations[i] - 1));
      const inputSpatialShapeWithPad = inputSpatialShape.map((v, i) => v + adjustPads[i] + adjustPads[i + spatialRank]);
      const outputShape = inputSpatialShapeWithPad.map(
        (v, i) => Math.floor((v - dilatedKernelShape[i] + strides[i]) / strides[i])
      );
      outputShape.splice(0, 0, batchSize);
      outputShape.splice(isChannelLast ? 3 : 1, 0, outChannels);
      return outputShape;
    };
    weightTransposeAttribute = [2, 3, 1, 0];
    validateInputs9 = (inputs, attributes) => {
      if (!inputs || inputs.length !== 2 && inputs.length !== 3) {
        throw new Error("Conv requires 2 or 3 inputs");
      }
      if (inputs[0].dims.length > 5) {
        throw new Error("greater than 5D is not supported");
      }
      if (inputs[0].dims.length !== inputs[1].dims.length) {
        throw new Error("filter does not have same dimension as input");
      }
      const dataChannel = inputs[0].dims[attributes.format === "NHWC" ? inputs[0].dims.length - 1 : 1];
      const filterInChannel = inputs[1].dims[1] * attributes.group;
      if (dataChannel !== filterInChannel) {
        throw new Error("FILTER_IN_CHANNEL should be equal to DATA_CHANNEL");
      }
      if (inputs.length === 3 && (inputs[2].dims.length !== 1 || inputs[1].dims[0] !== inputs[2].dims[0])) {
        throw new Error("invalid bias");
      }
      const spatialRank = inputs[0].dims.length - 2;
      if (attributes.dilations.length !== spatialRank) {
        throw new Error(`dilations should be ${spatialRank}D`);
      }
      if (attributes.strides.length !== spatialRank) {
        throw new Error(`strides should be ${spatialRank}D`);
      }
      if (attributes.pads.length !== spatialRank * 2) {
        throw new Error(`pads should be ${spatialRank * 2}D`);
      }
      if (attributes.kernelShape.length !== 0 && attributes.kernelShape.length !== inputs[1].dims.length - 2) {
        throw new Error("invalid kernel shape");
      }
    };
    getAdjustedConvAttributes = (attributes, inputs) => {
      const kernelShape = attributes.kernelShape.slice();
      for (let i = 2; i < inputs[1].dims.length; ++i) {
        if (kernelShape[i - 2] === 0) {
          kernelShape[i - 2] = inputs[1].dims[i];
        }
      }
      const pads = attributes.pads.slice();
      PoolConvUtil.adjustPadsBasedOnAutoPad(
        inputs[0].dims,
        attributes.strides,
        attributes.dilations,
        kernelShape,
        pads,
        attributes.format === "NHWC",
        attributes.autoPad
      );
      const newAttributes = Object.assign({}, attributes);
      Object.assign(newAttributes, { kernelShape, pads });
      return newAttributes;
    };
    parseConvAttributes = (attributes) => {
      const activationAttributes = parseInternalActivationAttributes(attributes);
      const format = attributes.format;
      const autoPad = ["NOTSET", "VALID", "SAME_UPPER", "SAME_LOWER"][attributes.auto_pad];
      const dilations = attributes.dilations;
      const group = attributes.group;
      const kernelShape = attributes.kernel_shape;
      const pads = attributes.pads;
      const strides = attributes.strides;
      const wIsConst = attributes.w_is_const();
      return {
        autoPad,
        format,
        dilations,
        group,
        kernelShape,
        pads,
        strides,
        wIsConst,
        ...activationAttributes,
        cacheKey: `${attributes.format};${activationAttributes.activation};`
      };
    };
    conv2d = (context, inputs, attributes, squeezeOutputShapeFunction) => {
      const isChannelsLast = attributes.format === "NHWC";
      if (attributes.group !== 1) {
        const enableGroupedConvVectorize = !context.adapterInfo.isArchitecture("ampere");
        if (enableGroupedConvVectorize && isChannelsLast && inputs[1].dims[0] === attributes.group && inputs[1].dims[1] === 1 && attributes.dilations[0] === 1 && attributes.dilations[1] === 1) {
          const outputShape2 = calculateOutputShape(
            inputs[0].dims,
            inputs[1].dims,
            attributes.dilations,
            attributes.pads,
            attributes.strides,
            isChannelsLast
          );
          const transposedWeight2 = context.kernelCustomData.wT ?? context.compute(createTransposeProgramInfo(inputs[1], weightTransposeAttribute), {
            inputs: [1],
            outputs: [attributes.wIsConst ? -2 : -1]
          })[0];
          if (attributes.wIsConst && !context.kernelCustomData.wT) {
            context.kernelCustomData.wT = transposedWeight2;
          }
          const convInputs2 = [inputs[0], transposedWeight2];
          if (inputs.length === 3) {
            convInputs2.push(inputs[2]);
          }
          context.compute(
            createGroupedConvVectorizeProgramInfo(convInputs2, attributes, outputShape2, squeezeOutputShapeFunction),
            { inputs: convInputs2 }
          );
        } else {
          context.compute(createGroupedConvProgramInfo(inputs, attributes, squeezeOutputShapeFunction));
        }
        return;
      }
      const hasBias = inputs.length === 3;
      const inputHeight = inputs[0].dims[isChannelsLast ? 1 : 2];
      const inputWidth = inputs[0].dims[isChannelsLast ? 2 : 3];
      const inputChannels = inputs[0].dims[isChannelsLast ? 3 : 1];
      const weightHeight = inputs[1].dims[2];
      const weightWidth = inputs[1].dims[3];
      const outputShape = calculateOutputShape(
        inputs[0].dims,
        inputs[1].dims,
        attributes.dilations,
        attributes.pads,
        attributes.strides,
        isChannelsLast
      );
      const outHeight = outputShape[isChannelsLast ? 1 : 2];
      const outWidth = outputShape[isChannelsLast ? 2 : 3];
      const outChannels = outputShape[isChannelsLast ? 3 : 1];
      const sameSize = isChannelsLast && weightHeight === inputHeight && weightWidth === inputWidth && attributes.pads[0] === 0 && attributes.pads[1] === 0;
      if (sameSize || weightHeight === 1 && weightWidth === 1 && attributes.dilations[0] === 1 && attributes.dilations[1] === 1 && attributes.strides[0] === 1 && attributes.strides[1] === 1 && attributes.pads[0] === 0 && attributes.pads[1] === 0) {
        const batch = outputShape[0];
        let xReshaped, wReshaped, matmulOutputShape;
        const matmulInputs = [];
        if (isChannelsLast) {
          const transposedWeight2 = context.kernelCustomData.wT ?? context.compute(createTransposeProgramInfo(inputs[1], weightTransposeAttribute), {
            inputs: [1],
            outputs: [attributes.wIsConst ? -2 : -1]
          })[0];
          if (attributes.wIsConst && !context.kernelCustomData.wT) {
            context.kernelCustomData.wT = transposedWeight2;
          }
          if (sameSize) {
            const sharedDim = inputHeight * inputWidth * inputChannels;
            xReshaped = inputs[0].reshape([1, batch, sharedDim]);
            wReshaped = transposedWeight2.reshape([1, sharedDim, outChannels]);
            matmulOutputShape = [1, batch, outChannels];
          } else {
            xReshaped = inputs[0].reshape([batch, inputHeight * inputWidth, inputChannels]);
            wReshaped = transposedWeight2.reshape([1, inputChannels, outChannels]);
            matmulOutputShape = [batch, outHeight * outWidth, outChannels];
          }
          matmulInputs.push(xReshaped);
          matmulInputs.push(wReshaped);
        } else {
          xReshaped = inputs[0].reshape([batch, inputChannels, inputHeight * inputWidth]);
          wReshaped = inputs[1].reshape([1, outChannels, inputChannels]);
          matmulOutputShape = [batch, outChannels, outHeight * outWidth];
          matmulInputs.push(wReshaped);
          matmulInputs.push(xReshaped);
        }
        if (hasBias) {
          matmulInputs.push(inputs[2]);
        }
        const N = matmulOutputShape[2];
        const K = matmulInputs[0].dims[matmulInputs[0].dims.length - 1];
        if (N < 8 && K < 8) {
          context.compute(
            createNaiveMatmulProgramInfo(
              matmulInputs,
              attributes,
              outputShape,
              matmulOutputShape,
              isChannelsLast,
              squeezeOutputShapeFunction
            ),
            { inputs: matmulInputs }
          );
        } else {
          context.compute(
            createMatmulProgramInfo(
              matmulInputs,
              attributes,
              outputShape,
              matmulOutputShape,
              isChannelsLast,
              squeezeOutputShapeFunction
            ),
            { inputs: matmulInputs }
          );
        }
        return;
      }
      const sequentialAccessByThreads = (
        /* backend.adapterInfo.isIntel() */
        true
      );
      const transposedWeight = context.kernelCustomData.wT ?? context.compute(createTransposeProgramInfo(inputs[1], weightTransposeAttribute), {
        inputs: [1],
        outputs: [attributes.wIsConst ? -2 : -1]
      })[0];
      if (attributes.wIsConst && !context.kernelCustomData.wT) {
        context.kernelCustomData.wT = transposedWeight;
      }
      const convInputs = [inputs[0], transposedWeight];
      if (hasBias) {
        convInputs.push(inputs[2]);
      }
      const dimAOuter = isChannelsLast ? outHeight * outWidth : outChannels;
      const dimBOuter = isChannelsLast ? outChannels : outHeight * outWidth;
      const dimInner = weightHeight * weightWidth * inputChannels;
      context.compute(
        createConv2DMatMulProgramInfo(
          convInputs,
          attributes,
          outputShape,
          dimAOuter,
          dimBOuter,
          dimInner,
          hasBias,
          sequentialAccessByThreads,
          squeezeOutputShapeFunction
        ),
        { inputs: convInputs }
      );
    };
    conv1d = (context, attributes) => {
      const isChannelLast = attributes.format === "NHWC";
      const inputs = [
        context.inputs[0].reshape(
          isChannelLast ? (
            // [N, W, C] -> [N, H=1, W, C]
            [context.inputs[0].dims[0], 1, context.inputs[0].dims[1], context.inputs[0].dims[2]]
          ) : (
            // [N, C, W] -> [N, C, H=1, W]
            [context.inputs[0].dims[0], context.inputs[0].dims[1], 1, context.inputs[0].dims[2]]
          )
        ),
        //[FILTER_OUT_CHANNEL, FILTER_IN_CHANNEL, kW] -> [FILTER_OUT_CHANNEL, FILTER_IN_CHANNEL, kH=1, kW]
        context.inputs[1].reshape([context.inputs[1].dims[0], context.inputs[1].dims[1], 1, context.inputs[1].dims[2]])
      ];
      if (context.inputs.length === 3) {
        inputs.push(context.inputs[2]);
      }
      const pads = [0, attributes.pads[0], 0, attributes.pads[1]];
      const strides = [1].concat(attributes.strides);
      const dilations = [1].concat(attributes.dilations);
      const kernelShape = [1].concat(attributes.kernelShape);
      const adjustedAttributes = getAdjustedConvAttributes(
        { ...attributes, pads, strides, dilations, kernelShape },
        inputs
      );
      conv2d(
        context,
        inputs,
        adjustedAttributes,
        (outputShape) => isChannelLast ? [outputShape[0], outputShape[2], outputShape[3]] : [outputShape[0], outputShape[1], outputShape[3]]
      );
    };
    conv3d = (context, inputs, attributes) => {
      const format = attributes.format === "NHWC" ? "channelsLast" : "channelsFirst";
      const adjustedAttributes = getAdjustedConvAttributes(attributes, inputs);
      const pads = attributes.autoPad === "NOTSET" ? attributes.pads : attributes.autoPad;
      const convInfo = computeConv3DInfo(
        inputs[0].dims,
        inputs[1].dims,
        attributes.strides,
        attributes.dilations,
        pads,
        false,
        format
      );
      context.compute(
        createConv3DNaiveProgramInfo(
          inputs,
          adjustedAttributes,
          convInfo.outShape,
          [convInfo.filterDepth, convInfo.filterHeight, convInfo.filterWidth],
          [convInfo.padInfo.front, convInfo.padInfo.top, convInfo.padInfo.left],
          format
        )
      );
    };
    conv = (context, attributes) => {
      validateInputs9(context.inputs, attributes);
      if (context.inputs[0].dims.length === 3) {
        conv1d(context, attributes);
      } else if (context.inputs[0].dims.length === 5) {
        conv3d(context, context.inputs, attributes);
      } else {
        const adjustedAttributes = getAdjustedConvAttributes(attributes, context.inputs);
        conv2d(context, context.inputs, adjustedAttributes);
      }
    };
  }
});

// web/lib/wasm/jsep/webgpu/ops/3rd-party/conv_backprop_mm_webgpu.ts
var conv2dTransposeCommonSnippet, createConv2DTransposeMatMulProgramInfo;
var init_conv_backprop_mm_webgpu = __esm({
  "web/lib/wasm/jsep/webgpu/ops/3rd-party/conv_backprop_mm_webgpu.ts"() {
    "use strict";
    init_wasm_common();
    init_log();
    init_common();
    init_fuse_utils();
    init_activation_util();
    init_conv_util();
    init_matmul_packed_webgpu();
    conv2dTransposeCommonSnippet = (isChannelsLast, addBias = false, attributes, type, innerElementSize = 4) => {
      const getWSnippet = (innerElementSize2) => {
        switch (innerElementSize2) {
          case 1:
            return "return w[getIndexFromCoords4D(coord, vec4<i32>(uniforms.w_shape))];";
          case 4:
            return `
            let coord1 = vec4<i32>(coordX, coordY, col + 1, rowInner);
            let coord2 = vec4<i32>(coordX, coordY, col + 2, rowInner);
            let coord3 = vec4<i32>(coordX, coordY, col + 3, rowInner);
            let v0 = w[getIndexFromCoords4D(coord, vec4<i32>(uniforms.w_shape))];
            let v1 = w[getIndexFromCoords4D(coord1, vec4<i32>(uniforms.w_shape))];
            let v2 = w[getIndexFromCoords4D(coord2, vec4<i32>(uniforms.w_shape))];
            let v3 = w[getIndexFromCoords4D(coord3, vec4<i32>(uniforms.w_shape))];
            return ${type}(v0, v1, v2, v3);
            `;
          default:
            throw new Error(`innerElementSize ${innerElementSize2} is not supported.`);
        }
      };
      const coordASnippet = isChannelsLast ? `
      let coord = vec4<i32>(batch, iXR, iXC, xCh);
      ` : `
      let coord = vec4<i32>(batch, xCh, iXR, iXC);
      `;
      const coordResSnippet = isChannelsLast ? `
    let coords = vec4<i32>(
      batch,
      row / outWidth,
      row % outWidth,
      col);
    ` : `
    let coords = vec4<i32>(
      batch,
      row,
      col / outWidth,
      col % outWidth);
    `;
      const xHeight = isChannelsLast ? "i32(uniforms.x_shape[1])" : "i32(uniforms.x_shape[2])";
      const xWidth = isChannelsLast ? "i32(uniforms.x_shape[2])" : "i32(uniforms.x_shape[3])";
      const row = isChannelsLast ? "row" : "col";
      const col = isChannelsLast ? "col" : "row";
      const readASnippet = `
      let inChannels = ${isChannelsLast ? "i32(uniforms.x_shape[3])" : "i32(uniforms.x_shape[1])"};
      let outWidth = ${isChannelsLast ? "i32(uniforms.result_shape[2])" : "i32(uniforms.result_shape[3])"};
      let outRow = ${row} / outWidth;
      let outCol = ${row} % outWidth;

      let WRow = ${col} / (uniforms.filter_dims[1] * inChannels);
      let WCol = ${col} / inChannels % uniforms.filter_dims[1];
      let xR = f32(outRow - uniforms.pads[0] + uniforms.dilations[0] * WRow) / f32(uniforms.strides[0]);
      let xC = f32(outCol - uniforms.pads[1] + uniforms.dilations[1] * WCol) / f32(uniforms.strides[1]);
      if (xR < 0.0 || xR >= f32(${xHeight}) || fract(xR) > 0.0) {
        return ${type}(0.0);
      }
      if (xC < 0.0 || xC >= f32(${xWidth}) || fract(xC) > 0.0) {
        return ${type}(0.0);
      }
      let iXR = i32(xR);
      let iXC = i32(xC);
      let xCh = ${col} % inChannels;
      ${coordASnippet}
      return x[getIndexFromCoords4D(coord, vec4<i32>(uniforms.x_shape))/${innerElementSize}];`;
      const sampleA = isChannelsLast ? `
      let col = colIn * ${innerElementSize};
      if (row < uniforms.dim_a_outer && col < uniforms.dim_inner) {
        ${readASnippet}
      }
      return ${type}(0.0);` : `
      let col = colIn * ${innerElementSize};
      if (row < uniforms.dim_inner && col < uniforms.dim_b_outer) {
        ${readASnippet}
      }
      return ${type}(0.0);`;
      const sampleW = `
      let col = colIn * ${innerElementSize};
      let inChannels = ${isChannelsLast ? "i32(uniforms.x_shape[3])" : "i32(uniforms.x_shape[1])"};
      let coordX = uniforms.filter_dims[0] - 1 - row / (uniforms.filter_dims[1] * inChannels);
      let coordY = uniforms.filter_dims[1] - 1 - (row / inChannels) % uniforms.filter_dims[1];
      if (${isChannelsLast ? "row < uniforms.dim_inner && col < uniforms.dim_b_outer" : "row < uniforms.dim_inner && col < uniforms.dim_a_outer"}  && coordX >= 0 && coordY >= 0) {
        let rowInner = row % inChannels;
        let coord = vec4<i32>(coordX, coordY, col, rowInner);
        ${getWSnippet(innerElementSize)}
      }
      return ${type}(0.0);
      `;
      const applyActivation = getActivationSnippet(attributes, type);
      const userCode = `
  fn mm_readA(batch: i32, row : i32, colIn : i32) -> ${type} {
    ${isChannelsLast ? sampleA : sampleW}
  }

  fn mm_readB(batch: i32, row : i32, colIn : i32) -> ${type} {
    ${isChannelsLast ? sampleW : sampleA}
  }

  fn mm_write(batch: i32, row : i32, colIn : i32, valueInput : ${type}) {
    let col = colIn * ${innerElementSize};
    if (row < uniforms.dim_a_outer && col < uniforms.dim_b_outer) {
      var value = valueInput;
      let outWidth = ${isChannelsLast ? "i32(uniforms.result_shape[2])" : "i32(uniforms.result_shape[3])"};
      ${coordResSnippet}
      ${biasSnippet(addBias)}
      ${applyActivation}
      result[getIndexFromCoords4D(coords, vec4<i32>(uniforms.result_shape))/${innerElementSize}] = value;
    }
  }`;
      return userCode;
    };
    createConv2DTransposeMatMulProgramInfo = (inputs, attributes, outputShape, dimAOuter, dimBOuter, dimInner, hasBias, sequentialAccessByThreads) => {
      const isChannelsLast = attributes.format === "NHWC";
      const inChannels = isChannelsLast ? inputs[0].dims[3] : inputs[0].dims[1];
      const batchSize = outputShape[0];
      const outWidth = isChannelsLast ? outputShape[2] : outputShape[3];
      const outHeight = isChannelsLast ? outputShape[1] : outputShape[2];
      const outChannels = isChannelsLast ? outputShape[3] : outputShape[1];
      const isVec4 = isChannelsLast && inChannels % 4 === 0 && inChannels % 3 && outChannels % 4 === 0;
      const dispatchX = isChannelsLast ? outChannels : outWidth * outHeight;
      const dispatchY = isChannelsLast ? outWidth * outHeight : outChannels;
      const workGroupSize = [8, 8, 1];
      const elementsPerThread = dimAOuter <= 8 ? [4, 1, 1] : [4, 4, 1];
      const dispatch = [
        Math.ceil(dispatchX / workGroupSize[0] / elementsPerThread[0]),
        Math.ceil(dispatchY / workGroupSize[1] / elementsPerThread[1]),
        Math.ceil(batchSize / workGroupSize[2] / elementsPerThread[2])
      ];
      LOG_DEBUG("verbose", () => `[conv_backprop_mm_webgpu] dispatch = ${dispatch}`);
      const innerElementSize = isVec4 ? 4 : 1;
      const tileInner = Math.max(workGroupSize[0] * innerElementSize, workGroupSize[1]);
      const components = isVec4 ? 4 : 1;
      const filterDims = [attributes.kernelShape[isChannelsLast ? 1 : 2], attributes.kernelShape[isChannelsLast ? 2 : 3]];
      const effectiveFilterDims = [
        filterDims[0] + (attributes.dilations[0] <= 1 ? 0 : (filterDims[0] - 1) * (attributes.dilations[0] - 1)),
        filterDims[1] + (attributes.dilations[1] <= 1 ? 0 : (filterDims[1] - 1) * (attributes.dilations[1] - 1))
      ];
      const pads = [
        effectiveFilterDims[0] - 1 - Math.floor((attributes.pads[0] + attributes.pads[2]) / 2),
        effectiveFilterDims[1] - 1 - Math.floor((attributes.pads[1] + attributes.pads[3]) / 2)
      ];
      const programUniforms = [
        { type: 6 /* int32 */, data: dimAOuter },
        { type: 6 /* int32 */, data: dimBOuter },
        { type: 6 /* int32 */, data: dimInner },
        { type: 6 /* int32 */, data: attributes.strides },
        { type: 6 /* int32 */, data: attributes.dilations },
        { type: 6 /* int32 */, data: filterDims },
        { type: 6 /* int32 */, data: pads }
      ];
      appendActivationUniformsData(attributes, programUniforms);
      programUniforms.push(...createTensorShapeVariables(inputs[0].dims, inputs[1].dims));
      const inputDependencies = ["rank", "rank"];
      if (hasBias) {
        programUniforms.push(...createTensorShapeVariables(inputs[2].dims));
        inputDependencies.push("rank");
      }
      programUniforms.push(...createTensorShapeVariables(outputShape));
      const getShaderSource = (shaderHelper) => {
        const x = inputVariable("x", inputs[0].dataType, inputs[0].dims.length, components);
        const w = inputVariable("w", inputs[1].dataType, inputs[1].dims.length, 1);
        const output = outputVariable("result", inputs[0].dataType, outputShape.length, components);
        const inputVariables = [x, w];
        let declareFunctions = "";
        if (hasBias) {
          const bias = inputVariable("bias", inputs[2].dataType, inputs[2].dims.length, components);
          inputVariables.push(bias);
          declareFunctions += `
          fn getBiasByOutputCoords(coords : vec4<i32>) -> ${bias.type.value} {
            return bias[coords.${isChannelsLast ? "w" : "y"}${isVec4 ? "/ 4" : ""}];
          }`;
        }
        const uniforms = [
          { name: "dim_a_outer", type: "i32" },
          { name: "dim_b_outer", type: "i32" },
          { name: "dim_inner", type: "i32" },
          { name: "strides", type: "i32", length: 2 },
          { name: "dilations", type: "i32", length: 2 },
          { name: "filter_dims", type: "i32", length: filterDims.length },
          { name: "pads", type: "i32", length: pads.length }
        ];
        appendActivationUniforms(attributes, uniforms);
        const elemType = tensorTypeToWsglStorageType(inputs[0].dataType, 1);
        if (elemType !== "f16" && elemType !== "f32") {
          throw new Error(`elemType ${elemType} is not supported.`);
        }
        return `
        ${utilFunctions("uniforms.result_strides")}
        ${shaderHelper.registerUniforms(uniforms).declareVariables(...inputVariables, output)};
        ${declareFunctions}
        ${conv2dTransposeCommonSnippet(isChannelsLast, hasBias, attributes, x.type.value, innerElementSize)}
        ${isVec4 ? makeMatMulPackedVec4Source(
          elementsPerThread,
          workGroupSize,
          elemType,
          void 0,
          !isChannelsLast,
          tileInner
        ) : makeMatMulPackedSource(
          elementsPerThread,
          workGroupSize,
          elemType,
          void 0,
          !isChannelsLast,
          tileInner,
          false,
          void 0,
          sequentialAccessByThreads
        )}`;
      };
      return {
        name: "Conv2DTransposeMatMul",
        shaderCache: { hint: `${attributes.cacheKey};${elementsPerThread};${workGroupSize};${isVec4}`, inputDependencies },
        getRunData: () => ({
          outputs: [{ dims: outputShape, dataType: inputs[0].dataType }],
          dispatchGroup: { x: dispatch[0], y: dispatch[1], z: dispatch[2] },
          programUniforms
        }),
        getShaderSource
      };
    };
  }
});

// web/lib/wasm/jsep/webgpu/ops/3rd-party/conv_backprop_webgpu.ts
var createConvTranspose2DOpProgramShaderSource, createConvTranspose2DProgramInfo;
var init_conv_backprop_webgpu = __esm({
  "web/lib/wasm/jsep/webgpu/ops/3rd-party/conv_backprop_webgpu.ts"() {
    "use strict";
    init_wasm_common();
    init_log();
    init_util();
    init_common();
    createConvTranspose2DOpProgramShaderSource = (shaderHelper, inputs, outputShape, hasBias, is1DimensionDispatch, isVec4 = false, dataType, uniforms, isChannelsLast = false) => {
      const rowDim = isChannelsLast ? 1 : 2;
      const colDim = isChannelsLast ? 2 : 3;
      const channelDim = isChannelsLast ? 3 : 1;
      const workPerThread = isVec4 ? 2 : 1;
      let declareFunctions = `
  fn setOutputAtIndex(flatIndex : u32, value : ${isVec4 ? `vec4<${dataType}>` : dataType}) {
    result[flatIndex] = ${isVec4 ? `vec4<${dataType}>` : dataType}(value);
  }`;
      if (hasBias) {
        declareFunctions += `
    fn getBiasByOutputCoords(coords : vec4<u32>) -> ${isVec4 ? `vec4<${dataType}>` : dataType} {
      return bias[coords.${isChannelsLast ? "w" : "y"}${isVec4 ? "/ 4" : ""}];
    }`;
      }
      const components = isVec4 ? 4 : 1;
      const w = inputVariable("W", inputs[1].dataType, inputs[1].dims.length, components);
      const dy = inputVariable("Dy", inputs[0].dataType, inputs[0].dims.length, components);
      const inputVariables = [dy, w];
      if (hasBias) {
        inputVariables.push(inputVariable("bias", inputs[2].dataType, [outputShape[channelDim]].length, components));
      }
      const output = outputVariable("result", inputs[0].dataType, outputShape.length, components);
      const codeSnippet4 = `{
        let batch: u32 = ${is1DimensionDispatch ? "global_id.z" : "workgroup_id.z"} / uniforms.result_shape[1];
        let r = ${is1DimensionDispatch ? "global_id.z" : "workgroup_id.z"} % uniforms.result_shape[1];
        let c = ${is1DimensionDispatch ? "global_id.y" : "workgroup_id.y"} * ${workPerThread};
        let d1: u32 = ${is1DimensionDispatch ? "global_id.x" : "workgroup_id.x"} * 4;

        let dyCorner = vec2<i32>(i32(r), i32(c)) - vec2<i32>(uniforms.pads);

        // Convolve dy(?, ?, d2) with w(:, :, d1, d2) to compute dx(xR, xC, d1).
        // ? = to be determined. : = across all values in that axis.
        var dotProd: array<vec4<${dataType}>, ${workPerThread}>;
        for (var i = 0; i < ${workPerThread}; i++) {
          dotProd[i] = vec4<${dataType}>(0.0);
        }
        for (var wR: u32 = 0; wR < uniforms.filter_dims[0]; wR = wR + 1) {
          var dyR = (${dataType}(dyCorner.x) + ${dataType}(wR)) / ${dataType}(uniforms.strides.x);
          let wRPerm = uniforms.filter_dims[0] - 1 - wR;
          if (dyR < 0.0 || dyR >= ${dataType}(uniforms.Dy_shape[1]) ||
              fract(dyR) > 0.0 || wRPerm < 0) {
            continue;
          }
          let idyR: u32 = u32(dyR);

          for (var wC: u32 = 0; wC < uniforms.filter_dims[1]; wC = wC + 1) {
            let dyC = (${dataType}(dyCorner.y) + ${dataType}(wC)) / ${dataType}(uniforms.strides.y);
            let dyC2 = (${dataType}(dyCorner.y) + 1.0 + ${dataType}(wC)) / ${dataType}(uniforms.strides.y);
            let wCPerm = uniforms.filter_dims[1] - 1 - wC;
            if (wCPerm < 0) {
              continue;
            }
            var bDyCVal = true;
            var bDyCVal2 = true;
            if (dyC < 0.0 || dyC >= ${dataType}(uniforms.Dy_shape[2]) ||
                fract(dyC) > 0.0) {
              bDyCVal = false;
            }
            if (dyC2 < 0.0 || dyC2 >= ${dataType}(uniforms.Dy_shape[2]) ||
                fract(dyC2) > 0.0) {
              bDyCVal2 = false;
            }

            let idyC: u32 = u32(dyC);
            let idyC2: u32 = u32(dyC2);
            if (bDyCVal && bDyCVal2) {
              let d2Length = uniforms.Dy_shape[3];
              for (var d2 :u32 = 0; d2 < d2Length; d2 = d2 + 4) {
                let wValue0 = ${w.get("u32(wRPerm)", "u32(wCPerm)", "d1", "d2")};
                let wValue1 = ${w.get("u32(wRPerm)", "u32(wCPerm)", "d1 + 1", "d2")};
                let wValue2 = ${w.get("u32(wRPerm)", "u32(wCPerm)", "d1 + 2", "d2")};
                let wValue3 = ${w.get("u32(wRPerm)", "u32(wCPerm)", "d1 + 3", "d2")};

                var xValue = ${dy.get("batch", "idyR", "idyC", "d2")};
                let tmpval = vec4<${dataType}>(dot(xValue, wValue0),
                                      dot(xValue, wValue1),
                                      dot(xValue, wValue2),
                                      dot(xValue, wValue3));
                dotProd[0] = dotProd[0] + tmpval;

                xValue =  ${dy.get("batch", "idyR", "idyC2", "d2")};

                dotProd[1] = dotProd[1] + vec4<${dataType}>(dot(xValue, wValue0),
                                                    dot(xValue, wValue1),
                                                    dot(xValue, wValue2),
                                                    dot(xValue, wValue3));
              }
            } else if (bDyCVal) {
              let d2Length = uniforms.Dy_shape[${channelDim}];
              for (var d2: u32 = 0; d2 < d2Length; d2 = d2 + 4) {
                let wValue0 = ${w.get("u32(wRPerm)", "u32(wCPerm)", "d1", "d2")};
                let wValue1 = ${w.get("u32(wRPerm)", "u32(wCPerm)", "d1 + 1", "d2")};
                let wValue2 = ${w.get("u32(wRPerm)", "u32(wCPerm)", "d1 + 2", "d2")};
                let wValue3 = ${w.get("u32(wRPerm)", "u32(wCPerm)", "d1 + 3", "d2")};

                var xValue = ${dy.get("batch", "idyR", "idyC", "d2")};
                let tmpval = vec4<${dataType}>(dot(xValue, wValue0),
                                      dot(xValue, wValue1),
                                      dot(xValue, wValue2),
                                      dot(xValue, wValue3));
                dotProd[0] = dotProd[0] + tmpval;
              }
            } else if (bDyCVal2) {
              let d2Length = uniforms.Dy_shape[3];
              for (var d2: u32 = 0; d2 < d2Length; d2 = d2 + 4) {
                let wValue0 = ${w.get("u32(wRPerm)", "u32(wCPerm)", "d1", "d2")};
                let wValue1 = ${w.get("u32(wRPerm)", "u32(wCPerm)", "d1 + 1", "d2")};
                let wValue2 = ${w.get("u32(wRPerm)", "u32(wCPerm)", "d1 + 2", "d2")};
                let wValue3 = ${w.get("u32(wRPerm)", "u32(wCPerm)", "d1 + 3", "d2")};

                var xValue = ${dy.get("batch", "idyR", "idyC2", "d2")};
                let tmpval = vec4<${dataType}>(dot(xValue, wValue0),
                                      dot(xValue, wValue1),
                                      dot(xValue, wValue2),
                                      dot(xValue, wValue3));
                dotProd[1] = dotProd[1] + tmpval;
              }
            }
          }
        }

        for (var i: u32 = 0; i < ${workPerThread}; i = i + 1) {
          let value = dotProd[i] + ${hasBias ? "bias[c+i]" : `vec4<${dataType}>(0.0)`};
          ${output.set("batch", "r", "c + i", "d1", "value")};
        }
      }`;
      const codeSnippet = `
          let outputIndices = ${output.offsetToIndices("global_idx")};
          let batch = ${output.indicesGet("outputIndices", 0)};
          let d1 = ${output.indicesGet("outputIndices", channelDim)};
          let r = ${output.indicesGet("outputIndices", rowDim)};
          let c = ${output.indicesGet("outputIndices", colDim)};
          let dyCorner = vec2<i32>(i32(r), i32(c)) - uniforms.pads;
          let dyRCorner = dyCorner.x;
          let dyCCorner = dyCorner.y;
          let groupId = d1 / uniforms.output_channels_per_group;
          let wOutChannel = d1 - groupId * uniforms.output_channels_per_group;
          // Convolve dy(?, ?, d2) with w(:, :, d1, d2) to compute dx(xR, xC, d1).
          // ? = to be determined. : = across all values in that axis.
          var dotProd = ${dataType}(0.0);
          for (var wR: u32 = 0; wR < uniforms.effective_filter_dims.x; wR = wR + 1) {
            if (wR % uniforms.dilations.x != 0) {
              continue;
            }
            let dyR = (${dataType}(dyRCorner) + ${dataType}(wR)) / ${dataType}(uniforms.strides[0]);
            let wRPerm = uniforms.filter_dims.x - 1 - wR / uniforms.dilations.x;
            if (dyR < 0.0 || dyR >= ${dataType}(uniforms.Dy_shape[${rowDim}]) || fract(dyR) > 0.0 ||
                wRPerm < 0) {
              continue;
            }
            let idyR: u32 = u32(dyR);

            for (var wC: u32 = 0; wC < uniforms.effective_filter_dims.y; wC = wC + 1) {
              if (wC % uniforms.dilations.y != 0) {
                continue;
              }
              let dyC = (${dataType}(dyCCorner) + ${dataType}(wC)) / ${dataType}(uniforms.strides.y);
              let wCPerm = uniforms.filter_dims.y - 1 - wC / uniforms.dilations.y;
              if (dyC < 0.0 || dyC >= ${dataType}(uniforms.Dy_shape[${colDim}]) ||
                  fract(dyC) > 0.0 || wCPerm < 0) {
                continue;
              }
              let idyC: u32 = u32(dyC);
              var inputChannel = groupId * uniforms.input_channels_per_group;
              for (var d2: u32 = 0; d2 < uniforms.input_channels_per_group; d2 = d2 + 1) {
                let xValue = ${isChannelsLast ? dy.get("batch", "idyR", "idyC", "inputChannel") : dy.get("batch", "inputChannel", "idyR", "idyC")};
                let wValue = ${w.get("inputChannel", "wOutChannel", "u32(wRPerm)", "u32(wCPerm)")};
                dotProd = dotProd + xValue * wValue;
                inputChannel = inputChannel + 1;
              }
            }
          }
          let value = dotProd + ${hasBias ? "bias[d1]" : `${dataType}(0.0)`};
          ${output.setByOffset("global_idx", "value")};
        `;
      return `
  ${shaderHelper.registerUniforms(uniforms).declareVariables(...inputVariables, output)}
  ${declareFunctions}

    ${shaderHelper.mainStart()}
    ${shaderHelper.guardAgainstOutOfBoundsWorkgroupSizes("uniforms.output_size")};
  ${isVec4 ? codeSnippet4 : codeSnippet}}`;
    };
    createConvTranspose2DProgramInfo = (inputs, attributes, squeezeOutputShapeFunction) => {
      const hasBias = inputs.length > 2;
      const outputShape = attributes.outputShape;
      const outputSize = ShapeUtil.size(outputShape);
      const dispatch = [Math.ceil(outputSize / 64), 1, 1];
      LOG_DEBUG("verbose", () => `[conv2d_backprop_webgpu] dispatch = ${dispatch}`);
      const isChannelsLast = attributes.format === "NHWC";
      const inputDependencies = ["rank", "rank"];
      const strides = [attributes.strides[0], attributes.strides[1]];
      const filterDims = [attributes.kernelShape[isChannelsLast ? 1 : 2], attributes.kernelShape[isChannelsLast ? 2 : 3]];
      const dilations = [attributes.dilations[0], attributes.dilations[1]];
      const effectiveFilterDims = [
        filterDims[0] + (attributes.dilations[0] <= 1 ? 0 : (attributes.kernelShape[isChannelsLast ? 1 : 2] - 1) * (attributes.dilations[0] - 1)),
        filterDims[1] + (attributes.dilations[1] <= 1 ? 0 : (attributes.kernelShape[isChannelsLast ? 2 : 3] - 1) * (attributes.dilations[1] - 1))
      ];
      const pads = [
        effectiveFilterDims[0] - 1 - Math.floor((attributes.pads[0] + attributes.pads[2]) / 2),
        effectiveFilterDims[1] - 1 - Math.floor(attributes.pads[1] + attributes.pads[3]) / 2
      ];
      const isVec4 = false;
      const group = attributes.group;
      const wShape = inputs[1].dims;
      const inputChannelsPerGroup = wShape[0] / group;
      const outputChannelsPerGroup = wShape[1];
      const programUniforms = [
        { type: 12 /* uint32 */, data: outputSize },
        { type: 12 /* uint32 */, data: strides },
        { type: 12 /* uint32 */, data: filterDims },
        { type: 12 /* uint32 */, data: dilations },
        { type: 12 /* uint32 */, data: effectiveFilterDims },
        { type: 6 /* int32 */, data: pads },
        { type: 12 /* uint32 */, data: inputChannelsPerGroup },
        { type: 12 /* uint32 */, data: outputChannelsPerGroup },
        ...createTensorShapeVariables(inputs[0].dims, inputs[1].dims)
      ];
      if (hasBias) {
        programUniforms.push(...createTensorShapeVariables(inputs[2].dims));
        inputDependencies.push("rank");
      }
      programUniforms.push(...createTensorShapeVariables(outputShape));
      const is1DimensionDispatch = dispatch[1] === 1 && dispatch[2] === 1;
      const getShaderSource = (shaderHelper) => {
        const uniforms = [
          { name: "output_size", type: "u32" },
          { name: "strides", type: "u32", length: strides.length },
          { name: "filter_dims", type: "u32", length: filterDims.length },
          { name: "dilations", type: "u32", length: filterDims.length },
          { name: "effective_filter_dims", type: "u32", length: effectiveFilterDims.length },
          { name: "pads", type: "i32", length: pads.length },
          { name: "input_channels_per_group", type: "u32" },
          { name: "output_channels_per_group", type: "u32" }
        ];
        const dataType = tensorTypeToWsglStorageType(inputs[0].dataType);
        return `${createConvTranspose2DOpProgramShaderSource(
          shaderHelper,
          inputs,
          outputShape,
          hasBias,
          is1DimensionDispatch,
          isVec4,
          dataType,
          uniforms,
          isChannelsLast
        )}`;
      };
      return {
        name: "ConvTranspose2D",
        shaderCache: { hint: `${attributes.cacheKey};`, inputDependencies },
        getRunData: () => ({
          dispatchGroup: { x: dispatch[0], y: dispatch[1], z: dispatch[2] },
          outputs: [
            {
              dims: squeezeOutputShapeFunction ? squeezeOutputShapeFunction(outputShape) : outputShape,
              dataType: inputs[0].dataType
            }
          ],
          programUniforms
        }),
        getShaderSource
      };
    };
  }
});

// web/lib/wasm/jsep/webgpu/ops/conv-transpose.ts
var computeTotalPad, distributePadding, calculateOutputShapeAndPads, getAdjustedConvTransposeAttributes, parseConvTransposeAttributes, validateInputs10, weightTransposePerm, convTranspose2d, convTranspose1d, convTranspose;
var init_conv_transpose = __esm({
  "web/lib/wasm/jsep/webgpu/ops/conv-transpose.ts"() {
    "use strict";
    init_conv_backprop_mm_webgpu();
    init_conv_backprop_webgpu();
    init_fuse_utils();
    init_transpose();
    computeTotalPad = (inDim, stride, adj, kernel, dilation, outSize) => (inDim - 1) * stride + adj + (kernel - 1) * dilation + 1 - outSize;
    distributePadding = (totalPad, autoPad, pads, head, tail) => {
      const smallPad = Math.floor(totalPad / 2);
      if (autoPad === "SAME_UPPER") {
        pads[head] = smallPad;
        pads[tail] = totalPad - smallPad;
      } else if (autoPad === "SAME_LOWER") {
        pads[head] = totalPad - smallPad;
        pads[tail] = smallPad;
      }
    };
    calculateOutputShapeAndPads = (inputShape, kernelShape, dilations, autoPad, group, pads, strides, isChannelLast, outputPadding, outputShape) => {
      const spatialRank = inputShape.length - 2;
      const updateOutputShape = outputShape.length === 0;
      if (outputPadding.length === 0) {
        for (let i = 0; i < spatialRank; ++i) {
          outputPadding.push(0);
        }
      }
      const batchSize = inputShape[0];
      const outChannels = kernelShape[isChannelLast ? 3 : 1] * group;
      for (let i = 0, j = inputShape.length - spatialRank - (isChannelLast ? 1 : 0); i < spatialRank; ++i, ++j) {
        const inSize = inputShape[j];
        const outSize = updateOutputShape ? inSize * strides[i] : outputShape[i];
        const totalPad = computeTotalPad(inSize, strides[i], pads[i], kernelShape[j], dilations[i], outSize);
        distributePadding(totalPad, autoPad, pads, i, i + spatialRank);
        if (updateOutputShape) {
          outputShape.push(
            strides[i] * (inSize - 1) + outputPadding[i] + (kernelShape[j] - 1) * dilations[i] + 1 - pads[i] - pads[i + spatialRank]
          );
        }
      }
      outputShape.splice(0, 0, batchSize);
      outputShape.splice(isChannelLast ? 3 : 1, 0, outChannels);
    };
    getAdjustedConvTransposeAttributes = (attributes, inputs) => {
      const kernelShape = attributes.kernelShape.slice();
      if (attributes.kernelShape.length === 0 || attributes.kernelShape.reduce((a, b) => a * b, 1) === 0) {
        kernelShape.length = 0;
        for (let i = 2; i < inputs[1].dims.length; ++i) {
          kernelShape.push(inputs[1].dims[i]);
        }
      }
      const isChannelsLast = attributes.format === "NHWC";
      kernelShape.splice(0, 0, inputs[1].dims[0]);
      kernelShape.splice(isChannelsLast ? 3 : 1, 0, inputs[1].dims[1]);
      const pads = attributes.pads.slice();
      const outputShape = attributes.outputShape.slice();
      const outputPadding = attributes.outputPadding.slice();
      const inputShape = inputs[0].dims;
      let dilations = attributes.dilations.slice();
      if (dilations.reduce((a, b) => a + b, 0) === 0) {
        const spatialRank = inputs[0].dims.length - 2;
        dilations = new Array(spatialRank).fill(1);
      }
      let strides = attributes.strides.slice();
      if (strides.reduce((a, b) => a + b, 0) === 0) {
        const spatialRank = inputs[0].dims.length - 2;
        strides = new Array(spatialRank).fill(1);
      }
      calculateOutputShapeAndPads(
        inputShape,
        kernelShape,
        dilations,
        attributes.autoPad,
        attributes.group,
        pads,
        strides,
        isChannelsLast,
        outputPadding,
        outputShape
      );
      const newAttributes = Object.assign({}, attributes);
      Object.assign(newAttributes, { kernelShape, pads, outputPadding, outputShape, dilations, strides });
      return newAttributes;
    };
    parseConvTransposeAttributes = (attributes) => {
      const activationAttributes = parseInternalActivationAttributes(attributes);
      const format = attributes.format;
      const autoPad = ["NOTSET", "VALID", "SAME_UPPER", "SAME_LOWER"][typeof attributes.autoPad == "undefined" ? 0 : attributes.autoPad];
      const dilations = attributes.dilations;
      const group = attributes.group;
      const kernelShape = attributes.kernelShape;
      const pads = attributes.pads;
      const strides = attributes.strides;
      const wIsConst = attributes.wIsConst();
      const outputPadding = attributes.outputPadding;
      const outputShape = attributes.outputShape;
      return {
        autoPad,
        format,
        dilations,
        group,
        kernelShape,
        outputPadding,
        outputShape,
        pads,
        strides,
        wIsConst,
        ...activationAttributes,
        cacheKey: `${attributes.format};${activationAttributes.activation};`
      };
    };
    validateInputs10 = (inputs, attributes) => {
      if (!inputs || inputs.length !== 2 && inputs.length !== 3) {
        throw new Error("Conv requires 2 or 3 inputs");
      }
      if (inputs[0].dims.length !== 4 && inputs[0].dims.length !== 3) {
        throw new Error("currently only support 2-dimensional conv");
      }
      if (inputs[0].dims.length !== inputs[1].dims.length) {
        throw new Error("filter does not have same dimension as input");
      }
      const dataChannel = inputs[0].dims[attributes.format === "NHWC" ? inputs[0].dims.length - 1 : 1];
      const filterInChannel = inputs[1].dims[0];
      if (dataChannel !== filterInChannel) {
        throw new Error("FILTER_IN_CHANNEL should be equal to DATA_CHANNEL");
      }
      const featureMaps = inputs[1].dims[1] * attributes.group;
      if (inputs.length === 3 && (inputs[2].dims.length !== 1 || inputs[2].dims[0] !== featureMaps)) {
        throw new Error("invalid bias");
      }
      const spatialRank = inputs[0].dims.length - 2;
      const dilationsSet = attributes.dilations.reduce((a, b) => a + b, 0) > 0;
      if (dilationsSet && attributes.dilations.length !== spatialRank) {
        throw new Error(`dilations should be ${spatialRank}D`);
      }
      const stridesSet = attributes.strides.reduce((a, b) => a + b, 0) > 0;
      if (stridesSet && attributes.strides.length !== spatialRank) {
        throw new Error(`strides should be ${spatialRank}D`);
      }
      const padsSet = attributes.pads.reduce((a, b) => a + b, 0) > 0;
      if (padsSet && attributes.pads.length !== spatialRank * 2) {
        throw new Error(`pads should be ${spatialRank * 2}D`);
      }
      if (attributes.outputPadding.length !== spatialRank && attributes.outputPadding.length !== 0) {
        throw new Error(`output_padding should be ${spatialRank}D`);
      }
      const kernelShapeSet = attributes.kernelShape.reduce((a, b) => a + b, 0) > 0;
      if (kernelShapeSet && attributes.kernelShape.length !== 0 && attributes.kernelShape.length !== inputs[1].dims.length - 2) {
        throw new Error("invalid kernel shape");
      }
      if (attributes.outputShape.length !== 0 && attributes.outputShape.length !== inputs[0].dims.length - 2) {
        throw new Error("invalid output shape");
      }
    };
    weightTransposePerm = [2, 3, 1, 0];
    convTranspose2d = (context, inputs, attributes) => {
      const adjustedAttributes = getAdjustedConvTransposeAttributes(attributes, inputs);
      const isChannelsLast = attributes.format === "NHWC";
      const outputShape = adjustedAttributes.outputShape;
      const outChannels = outputShape[isChannelsLast ? 3 : 1];
      const inputChannels = inputs[0].dims[isChannelsLast ? 3 : 1];
      if (adjustedAttributes.group !== 1 || outChannels === 1 && inputChannels === 1) {
        context.compute(createConvTranspose2DProgramInfo(inputs, adjustedAttributes));
        return;
      }
      const outHeight = outputShape[isChannelsLast ? 1 : 2];
      const outWidth = outputShape[isChannelsLast ? 2 : 3];
      const weightHeight = inputs[1].dims[2];
      const weightWidth = inputs[1].dims[3];
      const dimAOuter = isChannelsLast ? outHeight * outWidth : outChannels;
      const dimBOuter = isChannelsLast ? outChannels : outHeight * outWidth;
      const dimInner = weightHeight * weightWidth * inputChannels;
      const sequentialAccessByThreads = (
        /* backend.adapterInfo.isIntel() */
        true
      );
      const transposedWeight = context.kernelCustomData.wT ?? context.compute(createTransposeProgramInfo(inputs[1], weightTransposePerm), {
        inputs: [1],
        outputs: [attributes.wIsConst ? -2 : -1]
      })[0];
      if (attributes.wIsConst && !context.kernelCustomData.wT) {
        context.kernelCustomData.wT = transposedWeight;
      }
      const convTransposeInputs = [inputs[0], transposedWeight];
      const hasBias = inputs.length === 3;
      if (hasBias) {
        if (!isChannelsLast && inputs[2].dims.length === 1) {
          convTransposeInputs.push(inputs[2].reshape([inputs[2].dims[0], 1, 1]));
        } else {
          convTransposeInputs.push(inputs[2]);
        }
      }
      context.compute(
        createConv2DTransposeMatMulProgramInfo(
          convTransposeInputs,
          adjustedAttributes,
          outputShape,
          dimAOuter,
          dimBOuter,
          dimInner,
          hasBias,
          sequentialAccessByThreads
        ),
        { inputs: convTransposeInputs }
      );
    };
    convTranspose1d = (context, attributes) => {
      const isChannelLast = attributes.format === "NHWC";
      const inputs = [
        context.inputs[0].reshape(
          isChannelLast ? (
            // [N, W, C] -> [N, H=1, W, C]
            [context.inputs[0].dims[0], 1, context.inputs[0].dims[1], context.inputs[0].dims[2]]
          ) : (
            // [N, C, W] -> [N, C, H=1, W]
            [context.inputs[0].dims[0], context.inputs[0].dims[1], 1, context.inputs[0].dims[2]]
          )
        ),
        //[FILTER_OUT_CHANNEL, FILTER_IN_CHANNEL, kW] -> [FILTER_OUT_CHANNEL, FILTER_IN_CHANNEL, kH=1, kW]
        context.inputs[1].reshape([context.inputs[1].dims[0], context.inputs[1].dims[1], 1, context.inputs[1].dims[2]])
      ];
      if (context.inputs.length === 3) {
        inputs.push(context.inputs[2]);
      }
      let kernelShape = attributes.kernelShape;
      if (kernelShape.length === 0 || kernelShape[0] === 0) {
        kernelShape = [context.inputs[1].dims[2]];
      }
      let dilations = attributes.dilations;
      if (dilations.length === 0 || dilations[0] === 0) {
        dilations = [1];
      }
      let strides = attributes.strides;
      if (strides.length === 0 || strides[0] === 0) {
        strides = [1];
      }
      let pads = attributes.pads;
      if (pads.length === 0) {
        pads = [0, 0];
      }
      pads = [0, pads[0], 0, pads[1]];
      strides = [1].concat(strides);
      dilations = [1].concat(dilations);
      kernelShape = [1].concat(kernelShape);
      const adjustedAttributes = getAdjustedConvTransposeAttributes(
        { ...attributes, pads, strides, dilations, kernelShape },
        inputs
      );
      context.compute(
        createConvTranspose2DProgramInfo(
          inputs,
          adjustedAttributes,
          (outputShape) => isChannelLast ? [outputShape[0], outputShape[2], outputShape[3]] : [outputShape[0], outputShape[1], outputShape[3]]
        )
      );
    };
    convTranspose = (context, attributes) => {
      validateInputs10(context.inputs, attributes);
      if (context.inputs[0].dims.length === 3) {
        convTranspose1d(context, attributes);
      } else {
        convTranspose2d(context, context.inputs, attributes);
      }
    };
  }
});

// web/lib/wasm/jsep/webgpu/ops/cumsum.ts
var createCumsumProgramInfo, cumsum, parseCumSumAttributes;
var init_cumsum = __esm({
  "web/lib/wasm/jsep/webgpu/ops/cumsum.ts"() {
    "use strict";
    init_wasm_common();
    init_util();
    init_attribute_with_cache_key();
    init_common();
    createCumsumProgramInfo = (inputType, inputShape, axisInput, attributes) => {
      const outputSize = ShapeUtil.size(inputShape);
      const rank = inputShape.length;
      const input = inputVariable("input", inputType, rank);
      const output = outputVariable("output", inputType, rank);
      const axisValue = axisInput.dataType === 6 /* int32 */ ? axisInput.getInt32Array()[0] : Number(axisInput.getBigInt64Array()[0]);
      const axis = ShapeUtil.normalizeAxis(axisValue, rank);
      const getShaderSource = (shaderHelper) => {
        const index = ` i32(${input.indicesGet("inputIndices", "uniforms.axis")}) `;
        const max = getElementAt("uniforms.input_shape", "uniforms.axis", rank);
        const lowerLimit = attributes.reverse ? index + (attributes.exclusive ? " + 1" : "") : "0";
        const upperLimit = attributes.reverse ? max : index + (attributes.exclusive ? "" : " + 1");
        return `
                ${shaderHelper.registerUniform("outputSize", "u32").registerUniform("axis", "u32").declareVariables(input, output)}
                ${shaderHelper.mainStart()}
                  ${shaderHelper.guardAgainstOutOfBoundsWorkgroupSizes("uniforms.outputSize")}
                  var inputIndices = ${output.offsetToIndices("global_idx")};
                  var sum = ${output.type.value}(0);
                  let first : i32 = ${lowerLimit};
                  let last : i32 = ${upperLimit};
                  for (var i : i32 = first; i < last; i++) {
                    ${input.indicesSet("inputIndices", "uniforms.axis", "u32(i)")};
                    sum = sum + ${input.getByIndices("inputIndices")};
                  }
                  ${output.setByOffset("global_idx", "sum")};
                }`;
      };
      return {
        name: "CumSum",
        shaderCache: { hint: attributes.cacheKey, inputDependencies: ["rank"] },
        getRunData: () => ({
          outputs: [{ dims: inputShape, dataType: inputType }],
          dispatchGroup: { x: Math.ceil(
            outputSize / 64
            /* workgroup size */
          ) },
          programUniforms: [
            { type: 12 /* uint32 */, data: outputSize },
            { type: 12 /* uint32 */, data: axis },
            ...createTensorShapeVariables(inputShape, inputShape)
          ]
        }),
        getShaderSource
      };
    };
    cumsum = (context, attributes) => {
      const inputShape = context.inputs[0].dims;
      const inputType = context.inputs[0].dataType;
      const axis = context.inputs[1];
      context.compute(createCumsumProgramInfo(inputType, inputShape, axis, attributes), { inputs: [0] });
    };
    parseCumSumAttributes = (attributes) => {
      const exclusive = attributes.exclusive === 1;
      const reverse = attributes.reverse === 1;
      return createAttributeWithCacheKey({ exclusive, reverse });
    };
  }
});

// web/lib/wasm/jsep/webgpu/ops/depth-to-space.ts
var validateInputs11, permFunctionBody2, createDepthToSpaceProgramInfo, depthToSpace, parseDepthToSpaceAttributes;
var init_depth_to_space = __esm({
  "web/lib/wasm/jsep/webgpu/ops/depth-to-space.ts"() {
    "use strict";
    init_wasm_common();
    init_util();
    init_attribute_with_cache_key();
    init_common();
    validateInputs11 = (inputs) => {
      if (!inputs || inputs.length !== 1) {
        throw new Error("DepthToSpace requires 1 input.");
      }
      if (inputs[0].dims.length !== 4) {
        throw new Error("DepthToSpace requires 4D input.");
      }
    };
    permFunctionBody2 = (perm, rank, input, output) => {
      const reverseFunc = [];
      reverseFunc.push(`fn perm(i: ${output.type.indices}) -> ${input.type.indices} {
    var a: ${input.type.indices};`);
      for (let i = 0; i < rank; ++i) {
        reverseFunc.push(input.indicesSet("a", perm[i], `i[${i}]`));
      }
      reverseFunc.push("return a;}");
      return reverseFunc.join("\n");
    };
    createDepthToSpaceProgramInfo = (inputTensor, attributes) => {
      let n, h, w, c;
      let shape;
      let perm;
      const isChannelLast = attributes.format === "NHWC";
      const blocksize = attributes.blocksize;
      const isDCRmode = attributes.mode === "DCR";
      if (isChannelLast) {
        [n, h, w, c] = inputTensor.dims;
        shape = isDCRmode ? [n, h, w, blocksize, blocksize, c / blocksize ** 2] : [n, h, w, c / blocksize ** 2, blocksize, blocksize];
        perm = isDCRmode ? [0, 1, 3, 2, 4, 5] : [0, 1, 4, 2, 5, 3];
      } else {
        [n, h, w, c] = [inputTensor.dims[0], inputTensor.dims[2], inputTensor.dims[3], inputTensor.dims[1]];
        shape = isDCRmode ? [n, blocksize, blocksize, c / blocksize ** 2, h, w] : [n, c / blocksize ** 2, blocksize, blocksize, h, w];
        perm = isDCRmode ? [0, 3, 4, 1, 5, 2] : [0, 1, 4, 2, 5, 3];
      }
      const reshapedInputTensor = inputTensor.reshape(shape);
      const reshapedInputRank = reshapedInputTensor.dims.length;
      const inputDataType = inputTensor.dataType;
      const reshapedInput = inputVariable("a", inputDataType, reshapedInputRank);
      const permedOutput = outputVariable("output", inputDataType, reshapedInputRank);
      const getShaderSource = (shaderHelper) => `
  ${shaderHelper.registerUniform("output_size", "u32").declareVariables(reshapedInput, permedOutput)}

  ${permFunctionBody2(perm, reshapedInputRank, reshapedInput, permedOutput)}

  ${shaderHelper.mainStart()}
    ${shaderHelper.guardAgainstOutOfBoundsWorkgroupSizes("uniforms.output_size")}

    let indices = ${permedOutput.offsetToIndices("global_idx")};
    let aIndices = perm(indices);

    ${permedOutput.setByOffset("global_idx", reshapedInput.getByIndices("aIndices"))}
  }`;
      return {
        name: "DepthToSpace",
        shaderCache: {
          hint: `${inputTensor.dims};${attributes.blocksize};${attributes.mode}`,
          inputDependencies: ["rank"]
        },
        getRunData: (inputs) => {
          const outputShape = isChannelLast ? [n, h * blocksize, w * blocksize, c / blocksize ** 2] : [n, c / blocksize ** 2, h * blocksize, w * blocksize];
          const outputSize = ShapeUtil.size(outputShape);
          const shapeBeforePerm = reshapedInputTensor.dims;
          const shapeAfterPerm = ShapeUtil.sortBasedOnPerm(shapeBeforePerm, perm);
          return {
            outputs: [{ dims: outputShape, dataType: inputs[0].dataType }],
            dispatchGroup: { x: Math.ceil(
              outputSize / 64
              /* workgroup size */
            ) },
            programUniforms: [
              { type: 12 /* uint32 */, data: outputSize },
              ...createTensorShapeVariables(shapeBeforePerm, shapeAfterPerm)
            ]
          };
        },
        getShaderSource
      };
    };
    depthToSpace = (context, attributes) => {
      validateInputs11(context.inputs);
      context.compute(createDepthToSpaceProgramInfo(context.inputs[0], attributes));
    };
    parseDepthToSpaceAttributes = (attributes) => createAttributeWithCacheKey({
      blocksize: attributes.blocksize,
      mode: attributes.mode,
      format: attributes.format
    });
  }
});

// web/lib/wasm/jsep/webgpu/ops/einsum.ts
var symbolPattern, termPattern, termPatternOnly, lhsPattern, lhsPatternOnly, EinsumTerm, EinsumEquation, appendMax, createEinsumProgramInfo, einsum, parseEinsumAttributes;
var init_einsum = __esm({
  "web/lib/wasm/jsep/webgpu/ops/einsum.ts"() {
    "use strict";
    init_wasm_common();
    init_util();
    init_attribute_with_cache_key();
    init_common();
    symbolPattern = "[a-zA-Z]|\\.\\.\\.";
    termPattern = "(" + symbolPattern + ")+";
    termPatternOnly = "^" + termPattern + "$";
    lhsPattern = "(" + termPattern + ",)*" + termPattern;
    lhsPatternOnly = "^" + lhsPattern + "$";
    EinsumTerm = class {
      constructor(inputIndex = -1) {
        this.symbolToIndices = /* @__PURE__ */ new Map();
        this.inputIndex = inputIndex;
      }
      // Add a symbol to the term
      addSymbol(symbol, index) {
        let value = this.symbolToIndices.get(symbol);
        if (value === void 0) {
          value = [index];
        } else {
          value.push(index);
        }
        this.symbolToIndices.set(symbol, value);
      }
      // -1 for output and 0, 1, 2, ... for inputs
    };
    EinsumEquation = class {
      constructor(inputs, equation) {
        this.equation = equation;
        this.hasEllipsis = false;
        this.symbolToInfo = /* @__PURE__ */ new Map();
        this.lhs = new Array();
        this.outputDims = [];
        let [lhs, rhs] = equation.includes("->") ? equation.split("->", 2) : [equation, ""];
        if (!lhs.match(RegExp(lhsPatternOnly))) {
          throw new Error("Invalid LHS term");
        }
        const inputTerms = lhs.split(",");
        inputTerms.forEach((inputTerm, index) => {
          const dims = inputs[index].dims.slice();
          if (!inputTerm.match(RegExp(termPatternOnly))) {
            throw new Error("Invalid LHS term");
          }
          const einsumTerm = this.processTerm(inputTerm, true, dims, index);
          this.lhs.push(einsumTerm);
        });
        if (rhs === "") {
          rhs += [...this.symbolToInfo.entries()].filter(([sym, info]) => info.count === 1 || sym === "...").map(([sym]) => sym).join("");
        } else {
          if (!rhs.match(RegExp(termPattern))) {
            throw new Error("Invalid RHS");
          }
        }
        const rhsSymbols = rhs.match(RegExp(symbolPattern, "g"));
        rhsSymbols?.forEach((symbol) => {
          if (symbol === "...") {
            this.outputDims = this.outputDims.concat(this.ellipsisDims);
          } else {
            const info = this.symbolToInfo.get(symbol);
            if (info === void 0) {
              throw new Error("Invalid RHS symbol");
            }
            this.outputDims.push(info.dimValue);
          }
        });
        this.rhs = this.processTerm(rhs, false, this.outputDims);
      }
      // End of EinsumEqation constructor
      // Add a symbol to the equation
      addSymbol(symbol, dimValue, inputIndex) {
        let info = this.symbolToInfo.get(symbol);
        if (info !== void 0) {
          if (info.dimValue !== dimValue && info.count !== 1) {
            throw new Error("Dimension mismatch");
          } else {
            info.count++;
            info.inputIndices.push(inputIndex);
          }
        } else {
          info = { count: 1, dimValue, inputIndices: [inputIndex] };
        }
        this.symbolToInfo.set(symbol, info);
      }
      // Process one input/output term
      processTerm(term, isInput, dims, index = -1) {
        const rank = dims.length;
        let ellipsis = false;
        let ellipsisDims = [];
        let nextDim = 0;
        if (!term.match(RegExp(termPatternOnly)) && !isInput && term !== "") {
          throw new Error("Invalid LHS term");
        }
        const indexSymbols = term.match(RegExp(symbolPattern, "g"));
        const einsumTerm = new EinsumTerm(index);
        indexSymbols?.forEach((symbol, i) => {
          if (symbol === "...") {
            if (ellipsis) {
              throw new Error("Only one ellipsis is allowed per input term");
            }
            ellipsis = true;
            const ellipsisDimLength = rank - indexSymbols.length + 1;
            if (ellipsisDimLength < 0) {
              throw new Error("Ellipsis out of bounds");
            }
            ellipsisDims = dims.slice(nextDim, nextDim + ellipsisDimLength);
            if (this.hasEllipsis) {
              if (this.ellipsisDims.length !== ellipsisDims.length || this.ellipsisDims.toString() !== ellipsisDims.toString()) {
                throw new Error("Ellipsis dimensions mismatch");
              }
            } else if (isInput) {
              this.hasEllipsis = true;
              this.ellipsisDims = ellipsisDims;
            } else {
              throw new Error("Ellipsis must be specified in the LHS");
            }
            for (let j = 0; j < ellipsisDims.length; j++) {
              const symbol2 = String.fromCharCode("0".charCodeAt(0) + j);
              einsumTerm.addSymbol(symbol2, i + j);
              this.addSymbol(symbol2, dims[nextDim++], index);
            }
          } else {
            einsumTerm.addSymbol(symbol, i + (this.hasEllipsis ? this.ellipsisDims.length - 1 : 0));
            this.addSymbol(symbol, dims[nextDim++], index);
          }
        });
        return einsumTerm;
      }
      // Output dimensions of the equation
    };
    appendMax = (name) => name + "_max";
    createEinsumProgramInfo = (inputShapes, dataType, einsumEquation, outputShape) => {
      const ranks = inputShapes.map((dims) => dims.length);
      const inputVars = ranks.map((rank, index) => inputVariable(`input${index}`, dataType, rank));
      const outputSize = ShapeUtil.size(outputShape);
      const output = outputVariable("output", dataType, outputShape.length);
      const uniformsSymbols = [...einsumEquation.symbolToInfo.keys()].filter(
        (symbol) => !einsumEquation.rhs.symbolToIndices.has(symbol)
      );
      const getShaderSource = (shaderHelper) => {
        const idxCopy = [];
        const initProd = "var prod = 1.0;";
        const initSum = "var sum = 0.0;";
        const updateSum = "sum += prod;";
        const reduceOpsSetIndices = [];
        const reduceOpsLoopHeaders = [];
        const reduceOpsLoopFooters = [];
        const reduceOpCompute = [];
        const isReduceOpsWithoutLoop = einsumEquation.symbolToInfo.size === einsumEquation.rhs.symbolToIndices.size;
        einsumEquation.symbolToInfo.forEach((info, symbol) => {
          if (einsumEquation.rhs.symbolToIndices.has(symbol)) {
            const outputIndex = einsumEquation.rhs.symbolToIndices.get(symbol)?.[0];
            if (outputIndex !== void 0) {
              einsumEquation.lhs.forEach((term, i) => {
                if (info.inputIndices.includes(i)) {
                  const indices = term.symbolToIndices.get(symbol);
                  if (indices === void 0) {
                    throw new Error("Invalid symbol error");
                  }
                  indices.forEach((index) => {
                    idxCopy.push(
                      `${inputVars[i].indicesSet(
                        `input${i}Indices`,
                        index,
                        output.indicesGet("outputIndices", outputIndex)
                      )}`
                    );
                  });
                }
              });
            }
          } else {
            einsumEquation.lhs.forEach((term, i) => {
              if (info.inputIndices.includes(i)) {
                const indices = term.symbolToIndices.get(symbol);
                if (indices === void 0) {
                  throw new Error("Invalid symbol error");
                }
                indices.forEach((index) => {
                  reduceOpsSetIndices.push(`${inputVars[i].indicesSet(`input${i}Indices`, index, `${symbol}`)}`);
                });
                reduceOpCompute.push(`prod *= ${inputVars[i].getByIndices(`input${i}Indices`)};`);
              }
            });
            reduceOpsLoopHeaders.push(
              `for(var ${symbol}: u32 = 0; ${symbol} < uniforms.${appendMax(symbol)}; ${symbol}++) {`
            );
            reduceOpsLoopFooters.push("}");
          }
        });
        const reduceOps2 = isReduceOpsWithoutLoop ? [
          ...idxCopy,
          `let sum = ${inputVars.map((inputVar, i) => inputVar.getByIndices(`input${i}Indices`)).join(" * ")};`
        ] : [
          ...idxCopy,
          initSum,
          ...reduceOpsLoopHeaders,
          ...reduceOpsSetIndices,
          initProd,
          ...reduceOpCompute,
          updateSum,
          ...reduceOpsLoopFooters
        ];
        return `
            ${shaderHelper.registerUniforms(uniformsSymbols.map((symbol) => ({ name: `${appendMax(symbol)}`, type: "u32" }))).registerUniform("outputSize", "u32").declareVariables(...inputVars, output)}

            ${shaderHelper.mainStart()}
            ${shaderHelper.guardAgainstOutOfBoundsWorkgroupSizes("uniforms.outputSize")}
            var outputIndices = ${output.offsetToIndices("global_idx")};
            ${inputVars.map((_var, i) => `var input${i}Indices: ${inputVars[i].type.indices};`).join("\n")}
            ${reduceOps2.join("\n")};
            ${output.setByOffset("global_idx", "sum")};
          }`;
      };
      return {
        name: "Einsum",
        shaderCache: { hint: einsumEquation.equation, inputDependencies: inputShapes.map(() => "rank") },
        getRunData: () => {
          const programUniformsInit = uniformsSymbols.filter((symbol) => einsumEquation.symbolToInfo.has(symbol)).map((symbol) => ({ type: 12 /* uint32 */, data: einsumEquation.symbolToInfo.get(symbol)?.dimValue || 0 }));
          programUniformsInit.push({ type: 12 /* uint32 */, data: outputSize });
          const programUniforms = inputShapes.map((dims, _) => [...createTensorShapeVariables(dims)]).reduce((acc, inputProgramUniforms) => acc.concat(inputProgramUniforms), programUniformsInit);
          programUniforms.push(...createTensorShapeVariables(outputShape));
          return {
            outputs: [{ dims: outputShape, dataType }],
            dispatchGroup: { x: Math.ceil(
              outputSize / 64
              /* workgroup size */
            ) },
            programUniforms
          };
        },
        getShaderSource
      };
    };
    einsum = (context, attributes) => {
      const einsumEquation = new EinsumEquation(context.inputs, attributes.equation);
      const outputShape = einsumEquation.outputDims;
      const inputShapes = context.inputs.map((input, _) => input.dims);
      context.compute(createEinsumProgramInfo(inputShapes, context.inputs[0].dataType, einsumEquation, outputShape));
    };
    parseEinsumAttributes = (attributes) => {
      const equation = attributes.equation.replace(/\s+/g, "");
      return createAttributeWithCacheKey({ equation });
    };
  }
});

// web/lib/wasm/jsep/webgpu/ops/expand.ts
var validateInputs12, getAdjustedShape, calculateOutputShape2, createExpandProgramInfo, expand;
var init_expand = __esm({
  "web/lib/wasm/jsep/webgpu/ops/expand.ts"() {
    "use strict";
    init_wasm_common();
    init_util();
    init_common();
    validateInputs12 = (inputs) => {
      if (!inputs || inputs.length !== 2) {
        throw new Error("Expand requires 2 input.");
      }
      const inputShape = inputs[0].dims;
      const shape = Array.from(inputs[1].getBigInt64Array(), Number);
      let shapeIndex = shape.length < inputShape.length ? 0 : shape.length - inputShape.length;
      let inputShapeIndex = inputShape.length < shape.length ? 0 : inputShape.length - shape.length;
      for (; shapeIndex < shape.length && inputShapeIndex < inputShape.length; ++shapeIndex, ++inputShapeIndex) {
        if (shape[shapeIndex] !== inputShape[inputShapeIndex] && shape[shapeIndex] !== 1 && inputShape[inputShapeIndex] !== 1) {
          throw new Error("Expand requires shape to be broadcastable to input");
        }
      }
    };
    getAdjustedShape = (shape1, shape2) => {
      const diff = shape1.length - shape2.length;
      const shape = [];
      for (let i = 0; i < diff; ++i) {
        shape.push(shape1[i]);
      }
      for (let i = 0; i < shape2.length; ++i) {
        shape.push(shape2[i] === 1 ? shape1[i + diff] : shape2[i]);
      }
      return shape;
    };
    calculateOutputShape2 = (inputShape, shape) => inputShape.length > shape.length ? getAdjustedShape(inputShape, shape) : getAdjustedShape(shape, inputShape);
    createExpandProgramInfo = (inputs) => {
      const inputShape = inputs[0].dims;
      const shape = Array.from(inputs[1].getBigInt64Array(), Number);
      const outputShape = calculateOutputShape2(inputShape, shape);
      const dataType = inputs[0].dataType;
      const components = dataType === 9 /* bool */ ? 4 : 1;
      const outputSize = Math.ceil(ShapeUtil.size(outputShape) / components);
      const getShaderSource = (shaderHelper) => {
        const input = inputVariable("input", dataType, inputShape.length, components);
        const output = outputVariable("output", dataType, outputShape.length, components);
        let assignment;
        if (dataType === 9 /* bool */) {
          const singleAssignment = (resStr, x, typeCast = "") => `
          let outputIndices${x} = ${output.offsetToIndices(`outputOffset + ${x}u`)};
          let offset${x} = ${input.broadcastedIndicesToOffset(`outputIndices${x}`, output)};
          let index${x} = offset${x} / 4u;
          let component${x} = offset${x} % 4u;
          ${resStr}[${x}] = ${typeCast}(${input.getByOffset(`index${x}`)}[component${x}]);
        `;
          assignment = `
        let outputOffset = global_idx * ${components};
        var data = vec4<u32>(0);
        ${singleAssignment("data", 0, "u32")}
        ${singleAssignment("data", 1, "u32")}
        ${singleAssignment("data", 2, "u32")}
        ${singleAssignment("data", 3, "u32")}
        ${output.setByOffset("global_idx", "data")}
      }`;
        } else {
          assignment = `
        let outputIndices = ${output.offsetToIndices("global_idx")};
        let inputOffset = ${input.broadcastedIndicesToOffset("outputIndices", output)};
        ${output.setByOffset("global_idx", input.getByOffset("inputOffset"))}
      }`;
        }
        return `
    ${shaderHelper.registerUniform("vec_size", "u32").declareVariables(input, output)}
    ${shaderHelper.mainStart()}
    ${shaderHelper.guardAgainstOutOfBoundsWorkgroupSizes("uniforms.vec_size")}
    ${assignment}`;
      };
      const programUniforms = [
        { type: 12 /* uint32 */, data: outputSize },
        ...createTensorShapeVariables(inputShape, outputShape)
      ];
      return {
        name: "Expand",
        shaderCache: { hint: `${outputShape.length}`, inputDependencies: ["rank"] },
        getShaderSource,
        getRunData: () => ({
          outputs: [{ dims: outputShape, dataType: inputs[0].dataType }],
          dispatchGroup: { x: Math.ceil(
            outputSize / 64
            /* workgroup size */
          ) },
          programUniforms
        })
      };
    };
    expand = (context) => {
      validateInputs12(context.inputs);
      context.compute(createExpandProgramInfo(context.inputs), { inputs: [0] });
    };
  }
});

// web/lib/wasm/jsep/webgpu/ops/fast-gelu.ts
var createFastGeluProgramInfo, fastGelu2;
var init_fast_gelu = __esm({
  "web/lib/wasm/jsep/webgpu/ops/fast-gelu.ts"() {
    "use strict";
    init_wasm_common();
    init_util();
    init_common();
    init_unary_op();
    createFastGeluProgramInfo = (inputTensors) => {
      const dataType = inputTensors[0].dataType;
      const outputSize = ShapeUtil.size(inputTensors[0].dims);
      const biasLength = ShapeUtil.size(inputTensors[1].dims);
      const useVec4 = biasLength % 4 === 0;
      const getShaderSource = (shaderHelper) => {
        const x = inputVariable("x", dataType, [1], 4);
        const bias = inputVariable("bias", dataType, [1], 4);
        const y = outputVariable("y", dataType, [1], 4);
        const uniforms = [
          { name: "output_vec_size", type: "u32" },
          { name: "bias_size", type: "u32" }
        ];
        const singleElementBias = (i) => `
      let bias${i}_offset: u32 = (global_idx * 4 + ${i}) % uniforms.bias_size;
      let bias${i} = ${bias.getByOffset(`bias${i}_offset / 4`)}[bias${i}_offset % 4];`;
        const biasGetExpression = useVec4 ? `
      let bias = ${bias.getByOffset("global_idx % (uniforms.bias_size / 4)")};` : `${singleElementBias(0)}${singleElementBias(1)}${singleElementBias(2)}${singleElementBias(3)}
      let bias = ${x.type.value}(bias0, bias1, bias2, bias3);`;
        return `${shaderHelper.registerUniforms(uniforms).declareVariables(x, bias, y)}

    ${fastGeluImpl(tensorTypeToWsglValueType(dataType))}

    ${shaderHelper.mainStart(WORKGROUP_SIZE)}
      ${shaderHelper.guardAgainstOutOfBoundsWorkgroupSizes("uniforms.output_vec_size")}

      let x = ${x.getByOffset("global_idx")};
      ${biasGetExpression}
      let x_in = x + bias;
      ${y.setByOffset("global_idx", fastGeluExpression("x_in"))}
    }`;
      };
      return {
        name: "FastGeluWithBias",
        shaderCache: { hint: `${useVec4}`, inputDependencies: ["type", "type"] },
        getShaderSource,
        getRunData: (inputs) => ({
          outputs: [{ dims: inputs[0].dims, dataType: inputs[0].dataType }],
          programUniforms: [
            { type: 12 /* uint32 */, data: Math.ceil(outputSize / 4) },
            { type: 12 /* uint32 */, data: biasLength }
          ],
          dispatchGroup: { x: Math.ceil(outputSize / WORKGROUP_SIZE / 4) }
        })
      };
    };
    fastGelu2 = (context) => {
      if (context.inputs.length < 2 || ShapeUtil.size(context.inputs[1].dims) === 0) {
        fastGelu(context);
      } else {
        context.compute(createFastGeluProgramInfo(context.inputs));
      }
    };
  }
});

// web/lib/wasm/jsep/webgpu/ops/gather.ts
var validateInputs13, createGatherProgramInfo, parseGatherAttributes, gather;
var init_gather = __esm({
  "web/lib/wasm/jsep/webgpu/ops/gather.ts"() {
    "use strict";
    init_wasm_common();
    init_util();
    init_attribute_with_cache_key();
    init_common();
    validateInputs13 = (inputs) => {
      if (!inputs || inputs.length !== 2) {
        throw new Error("Gather requires 2 inputs.");
      }
    };
    createGatherProgramInfo = (inputs, attributes) => {
      const inputShape = inputs[0].dims;
      const indicesShape = inputs[1].dims;
      const inputRank = inputShape.length;
      const axis = ShapeUtil.normalizeAxis(attributes.axis, inputRank);
      const outputShape = inputShape.slice(0);
      outputShape.splice(axis, 1, ...indicesShape);
      const axisDimLimit = inputShape[axis];
      const components = inputs[0].dataType === 9 /* bool */ ? 4 : 1;
      const outputSize = Math.ceil(ShapeUtil.size(outputShape) / components);
      const programUniforms = [
        { type: 12 /* uint32 */, data: outputSize },
        { type: 6 /* int32 */, data: axisDimLimit },
        { type: 12 /* uint32 */, data: axis },
        ...createTensorShapeVariables(inputs[0].dims, inputs[1].dims, outputShape)
      ];
      const getShaderSource = (shaderHelper) => {
        const data = inputVariable("data", inputs[0].dataType, inputs[0].dims.length, components);
        const indices = inputVariable("inputIndices", inputs[1].dataType, inputs[1].dims.length);
        const output = outputVariable("output", inputs[0].dataType, outputShape.length, components);
        const calcDataIndices = (x) => {
          const indicesRank = indicesShape.length;
          let calcStr = `var indicesIndices${x}  = ${indices.type.indices}(0);`;
          for (let i = 0; i < indicesRank; i++) {
            calcStr += `${indicesRank > 1 ? `indicesIndices${x}[${i}]` : `indicesIndices${x}`} = ${outputShape.length > 1 ? `outputIndices${x}[uniforms.axis + ${i}]` : `outputIndices${x}`};`;
          }
          calcStr += `
          var idx${x} = ${indices.getByIndices(`indicesIndices${x}`)};
          if (idx${x} < 0) {
            idx${x} = idx${x} + uniforms.axisDimLimit;
          }
          var dataIndices${x} : ${data.type.indices};
        `;
          for (let i = 0, j = 0; i < inputRank; i++) {
            if (i === axis) {
              calcStr += `${inputRank > 1 ? `dataIndices${x}[${i}]` : `dataIndices${x}`} = u32(idx${x});`;
              j += indicesRank;
            } else {
              calcStr += `${inputRank > 1 ? `dataIndices${x}[${i}]` : `dataIndices${x}`} = ${outputShape.length > 1 ? `outputIndices${x}[${j}]` : `outputIndices${x}`};`;
              j++;
            }
          }
          return calcStr;
        };
        let assignment;
        if (inputs[0].dataType === 9 /* bool */) {
          const singleAssignment = (resStr, x, typeCast = "") => `
          let outputIndices${x} = ${output.offsetToIndices(`outputOffset + ${x}u`)};
          ${calcDataIndices(x)};
          let offset${x} = ${data.indicesToOffset(`dataIndices${x}`)};
          let index${x} = offset${x} / 4u;
          let component${x} = offset${x} % 4u;
          ${resStr}[${x}] = ${typeCast}(${data.getByOffset(`index${x}`)}[component${x}]);
        `;
          assignment = `
        let outputOffset = global_idx * ${components};
        var value = vec4<u32>(0);
        ${singleAssignment("value", 0, "u32")}
        ${singleAssignment("value", 1, "u32")}
        ${singleAssignment("value", 2, "u32")}
        ${singleAssignment("value", 3, "u32")}
        ${output.setByOffset("global_idx", "value")}
      `;
        } else {
          assignment = `
      let outputIndices = ${output.offsetToIndices("global_idx")};
      ${calcDataIndices("")};
      let value = ${data.getByIndices("dataIndices")};
      ${output.setByOffset("global_idx", "value")};
      `;
        }
        return `
      ${shaderHelper.registerUniform("outputSize", "u32").registerUniform("axisDimLimit", "i32").registerUniform("axis", "u32").declareVariables(data, indices, output)}
      ${shaderHelper.mainStart()}
        ${shaderHelper.guardAgainstOutOfBoundsWorkgroupSizes("uniforms.outputSize")}
        ${assignment}
      }`;
      };
      return {
        name: "Gather",
        shaderCache: { hint: attributes.cacheKey, inputDependencies: ["rank", "rank"] },
        getRunData: () => ({
          outputs: [{ dims: outputShape, dataType: inputs[0].dataType }],
          dispatchGroup: { x: Math.ceil(
            outputSize / 64
            /* workgroup size */
          ) },
          programUniforms
        }),
        getShaderSource
      };
    };
    parseGatherAttributes = (attributes) => createAttributeWithCacheKey({ axis: attributes.axis });
    gather = (context, attributes) => {
      const inputs = context.inputs;
      validateInputs13(inputs);
      context.compute(createGatherProgramInfo(context.inputs, attributes));
    };
  }
});

// web/lib/wasm/jsep/webgpu/ops/gather-block-quantized.ts
var validateInputs14, createGatherBlockQuantizedProgramInfo, gatherBlockQuantized, parseGatherBlockQuantizedAttributes;
var init_gather_block_quantized = __esm({
  "web/lib/wasm/jsep/webgpu/ops/gather-block-quantized.ts"() {
    "use strict";
    init_wasm_common();
    init_util();
    init_attribute_with_cache_key();
    init_common();
    validateInputs14 = (inputs, attributes) => {
      if (inputs.length < 3 || inputs.length > 4) {
        throw new Error("GatherBlockQuantized requires 3 or 4 inputs.");
      }
      const quantizeAxis = ShapeUtil.normalizeAxis(attributes.quantizeAxis, inputs[0].dims.length);
      const blockSize = attributes.blockSize;
      const data = inputs[0];
      const scales = inputs[2];
      const zeroPoint = inputs.length === 4 ? inputs[3] : void 0;
      if (scales.dims.length !== data.dims.length || !data.dims.map((d, i) => i === quantizeAxis ? Math.ceil(d / blockSize) === scales.dims[i] : d === scales.dims[i]).reduce((a, b) => a && b, true)) {
        throw new Error(
          "Scales must have the same rank as the input tensor and the dims should match except on gatherAxis."
        );
      }
      if (zeroPoint) {
        if (zeroPoint.dataType !== data.dataType) {
          throw new Error("Zero point must have the same data type as the input tensor.");
        }
        if (zeroPoint.dims.length !== scales.dims.length || !zeroPoint.dims.map((d, i) => d === scales.dims[i]).reduce((a, b) => a && b, true)) {
          throw new Error(
            "Zero point must have the same rank as the input tensor and the dims should match except on quantizeAxis."
          );
        }
      }
    };
    createGatherBlockQuantizedProgramInfo = (inputs, attributes) => {
      const inputShape = inputs[0].dims;
      const indicesShape = inputs[1].dims;
      const inputRank = inputShape.length;
      const gatherAxis = ShapeUtil.normalizeAxis(attributes.gatherAxis, inputRank);
      const quantizeAxis = ShapeUtil.normalizeAxis(attributes.quantizeAxis, inputRank);
      const outputShape = inputShape.slice(0);
      outputShape.splice(gatherAxis, 1, ...indicesShape);
      const outputSize = ShapeUtil.size(outputShape);
      const outputType = inputs[2].dataType;
      const inputType = inputs[0].dataType;
      const isSigned = inputType === 22 /* int4 */;
      const programUniforms = [
        { type: 12 /* uint32 */, data: outputSize },
        { type: 12 /* uint32 */, data: quantizeAxis },
        { type: 12 /* uint32 */, data: gatherAxis },
        { type: 12 /* uint32 */, data: attributes.blockSize },
        ...createTensorShapeVariables(...inputs.map((input, _) => input.dims), outputShape)
      ];
      const getShaderSource = (shaderHelper) => {
        const data = inputVariable("data", inputs[0].dataType, inputs[0].dims.length);
        const indices = inputVariable("inputIndices", inputs[1].dataType, inputs[1].dims.length);
        const scales = inputVariable("scales", inputs[2].dataType, inputs[2].dims.length);
        const zeroPoint = inputs.length > 3 ? inputVariable("zeroPoint", inputs[3].dataType, inputs[3].dims.length) : void 0;
        const output = outputVariable("output", outputType, outputShape.length);
        const inputVariables = [data, indices, scales];
        if (zeroPoint) {
          inputVariables.push(zeroPoint);
        }
        const uniforms = [
          { name: "output_size", type: "u32" },
          { name: "quantize_axis", type: "u32" },
          { name: "gather_axis", type: "u32" },
          { name: "block_size", type: "u32" }
        ];
        return `
        ${shaderHelper.registerUniforms(uniforms).declareVariables(...inputVariables, output)}
        ${shaderHelper.mainStart()}
        let output_indices = ${output.offsetToIndices("global_idx")};
        var indices_indices = ${indices.type.indices}(0);
        ${(() => {
          if (indicesShape.length > 1) {
            return `
          for (var i: u32 = 0; i < ${indicesShape.length}; i++) {
            let index = ${output.indicesGet("output_indices", "uniforms.gather_axis + i")};
            ${indices.indicesSet("indices_indices", "i", "index")};
          }`;
          } else {
            return `indices_indices = ${output.indicesGet("output_indices", "uniforms.gather_axis")};`;
          }
        })()};
        var data_indices = ${data.type.indices}(0);
        for (var i: u32 = 0; i < uniforms.gather_axis; i++) {
          let index = ${output.indicesGet("output_indices", "i")};
          ${data.indicesSet("data_indices", "i", "index")};
        }
        var index_from_indices = ${indices.getByIndices("indices_indices")};
        if (index_from_indices < 0) {
          index_from_indices += ${inputShape[gatherAxis]};
        }
        ${data.indicesSet("data_indices", "uniforms.gather_axis", "u32(index_from_indices)")};
        for (var i = uniforms.gather_axis + 1; i < ${outputShape.length}; i++) {
          let index = ${output.indicesGet("output_indices", `i + ${indicesShape.length} - 1`)};
          ${data.indicesSet("data_indices", "i", "index")};
        }
        let data_offset = ${data.indicesToOffset("data_indices")};
        let data_index = data_offset % 8;
        // Convert 4-bit packed data to 8-bit packed data.
        let packed_4bit_quantized_data = ${data.getByOffset("data_offset / 8")};
        let packed_8bit_quantized_data = (packed_4bit_quantized_data >> (4 * (data_index % 2))) & 0x0f0f0f0f;
        let quantized_data_vec = ${isSigned ? "unpack4xI8" : "unpack4xU8"}(u32(packed_8bit_quantized_data));
        let quantized_data = quantized_data_vec[data_index / 2];
        var scale_indices = data_indices;
        let quantize_axis_index = ${scales.indicesGet("data_indices", "uniforms.quantize_axis")} / uniforms.block_size;
        ${scales.indicesSet("scale_indices", "uniforms.quantize_axis", "quantize_axis_index")};
        var scale = ${scales.getByIndices("scale_indices")};
        ${(() => {
          if (!zeroPoint) {
            return "var zero_point = 0";
          } else {
            return `
              let zero_point_indices = scale_indices;
              let zero_point_offset = ${zeroPoint.indicesToOffset("zero_point_indices")};
              let zero_point_index = zero_point_offset % 8;
              let packed_4bit_zero_points = ${zeroPoint.getByOffset("zero_point_offset / 8")};
              let packed_8bit_zero_points = (packed_4bit_zero_points >> (4 * (zero_point_index % 2))) & 0x0f0f0f0f;
              let zero_point_vec = ${isSigned ? "unpack4xI8" : "unpack4xU8"}(u32(packed_8bit_zero_points));
              let zero_point = zero_point_vec[zero_point_index / 2];`;
          }
        })()};
        let dequantized_data = ${tensorTypeToWsglValueType(outputType)}(quantized_data - zero_point) * scale;
        ${output.setByOffset("global_idx", "dequantized_data")};
    }`;
      };
      return {
        name: "GatherBlockQuantized",
        shaderCache: {
          hint: `${attributes.cacheKey};${inputs.filter((_, i) => i !== 1).map((input) => input.dims.join("_")).join(";")}`,
          inputDependencies: Array.from({ length: inputs.length }, (_v, _i) => "rank")
        },
        getRunData: () => ({
          outputs: [{ dims: outputShape, dataType: outputType }],
          dispatchGroup: { x: Math.ceil(
            outputSize / 64
            /* workgroup size */
          ) },
          programUniforms
        }),
        getShaderSource
      };
    };
    gatherBlockQuantized = (context, attributes) => {
      const inputs = context.inputs;
      validateInputs14(inputs, attributes);
      context.compute(createGatherBlockQuantizedProgramInfo(context.inputs, attributes));
    };
    parseGatherBlockQuantizedAttributes = (attributes) => createAttributeWithCacheKey({
      blockSize: attributes.blockSize,
      gatherAxis: attributes.gatherAxis,
      quantizeAxis: attributes.quantizeAxis
    });
  }
});

// web/lib/wasm/jsep/webgpu/ops/gather-elements.ts
var validateInputs15, createGatherElementsProgramInfo, parseGatherElementsAttributes, gatherElements;
var init_gather_elements = __esm({
  "web/lib/wasm/jsep/webgpu/ops/gather-elements.ts"() {
    "use strict";
    init_wasm_common();
    init_util();
    init_attribute_with_cache_key();
    init_common();
    validateInputs15 = (inputs) => {
      if (!inputs || inputs.length !== 2) {
        throw new Error("GatherElements requires 2 inputs.");
      }
      if (inputs[0].dims.length < 1) {
        throw new Error("GatherElements requires that the data input be rank >= 1.");
      }
      if (inputs[0].dims.length !== inputs[1].dims.length) {
        throw new Error(`GatherElements requires that the data input and
                     indices input tensors be of same rank.`);
      }
    };
    createGatherElementsProgramInfo = (inputs, attributes) => {
      const inputShape = inputs[0].dims;
      const inputOutputDataType = inputs[0].dataType;
      const inputRank = inputShape.length;
      const indicesShape = inputs[1].dims;
      const indicesDataType = inputs[1].dataType;
      const axis = ShapeUtil.normalizeAxis(attributes.axis, inputRank);
      const axisDimLimit = inputShape[axis];
      const outputShape = indicesShape.slice(0);
      const outputSize = ShapeUtil.size(outputShape);
      const input = inputVariable("input", inputOutputDataType, inputRank);
      const indices = inputVariable("indicesInput", indicesDataType, indicesShape.length);
      const output = outputVariable("output", inputOutputDataType, outputShape.length);
      const programUniforms = [
        { type: 12 /* uint32 */, data: outputSize },
        { type: 6 /* int32 */, data: axisDimLimit },
        { type: 12 /* uint32 */, data: axis }
      ];
      programUniforms.push(...createTensorShapeVariables(inputShape, indicesShape, outputShape));
      const inputDependencies = ["rank", "rank"];
      const getShaderSource = (shaderHelper) => `
      ${shaderHelper.registerUniform("outputSize", "u32").registerUniform("axisDimLimit", "i32").registerUniform("axis", "u32").declareVariables(input, indices, output)}
      ${shaderHelper.mainStart()}
      ${shaderHelper.guardAgainstOutOfBoundsWorkgroupSizes("uniforms.outputSize")}

      let outputIndices = ${output.offsetToIndices("global_idx")};

      var idx = ${indices.getByOffset("global_idx")};
      if (idx < 0) {
        idx = idx + uniforms.axisDimLimit;
      }
      var inputIndices = ${input.type.indices}(outputIndices);
      ${input.indicesSet("inputIndices", "uniforms.axis", "u32(idx)")};
      let value = ${input.getByIndices("inputIndices")};

      ${output.setByOffset("global_idx", "value")};
  }`;
      return {
        name: "GatherElements",
        shaderCache: { inputDependencies },
        getRunData: () => ({
          outputs: [{ dims: outputShape, dataType: inputs[0].dataType }],
          dispatchGroup: { x: Math.ceil(
            outputSize / 64
            /* workgroup size */
          ) },
          programUniforms
        }),
        getShaderSource
      };
    };
    parseGatherElementsAttributes = (attributes) => createAttributeWithCacheKey({ axis: attributes.axis });
    gatherElements = (context, attributes) => {
      const inputs = context.inputs;
      validateInputs15(inputs);
      context.compute(createGatherElementsProgramInfo(context.inputs, attributes));
    };
  }
});

// web/lib/wasm/jsep/webgpu/ops/gemm.ts
var validateInputs16, createGemmProgramInfo, parseGemmAttributes, gemm;
var init_gemm = __esm({
  "web/lib/wasm/jsep/webgpu/ops/gemm.ts"() {
    "use strict";
    init_wasm_common();
    init_util();
    init_common();
    validateInputs16 = (inputs) => {
      if (!inputs) {
        throw new Error("Input is missing");
      }
      if (inputs.length < 2 || inputs.length > 3) {
        throw new Error("Invaid input number.");
      }
      if (inputs.length === 3 && inputs[2].dims.length > 2) {
        throw new Error("Invalid input shape of C");
      }
      if (inputs[0].dataType !== inputs[1].dataType || inputs.length === 3 && inputs[0].dataType !== inputs[2].dataType) {
        throw new Error("Input types are mismatched");
      }
    };
    createGemmProgramInfo = (inputs, attributes) => {
      const aShape = inputs[0].dims.slice();
      const bShape = inputs[1].dims.slice();
      const [M, N, K] = GemmUtil.getShapeOfGemmResult(
        aShape,
        attributes.transA,
        bShape,
        attributes.transB,
        inputs.length === 3 ? inputs[2].dims : void 0
      );
      const outputShape = [M, N];
      if (!outputShape) {
        throw new Error("Can't use gemm on the given tensors");
      }
      const outputSize = ShapeUtil.size(outputShape);
      const programUniforms = [
        { type: 12 /* uint32 */, data: outputSize },
        { type: 12 /* uint32 */, data: M },
        { type: 12 /* uint32 */, data: N },
        { type: 12 /* uint32 */, data: K },
        { type: 1 /* float */, data: attributes.alpha },
        { type: 1 /* float */, data: attributes.beta }
      ];
      const inputDependencies = ["type", "type"];
      if (inputs.length === 3) {
        programUniforms.push(...createTensorShapeVariables(inputs[2].dims));
        inputDependencies.push("rank");
      }
      programUniforms.push(...createTensorShapeVariables(outputShape));
      const getShaderSource = (shaderHelper) => {
        let line = "";
        if (attributes.transA && attributes.transB) {
          line = "value += a[k * uniforms.M + m] * b[n * uniforms.K + k];";
        } else if (attributes.transA && !attributes.transB) {
          line = "value += a[k * uniforms.M + m] * b[k * uniforms.N + n];";
        } else if (!attributes.transA && attributes.transB) {
          line = "value += a[m * uniforms.K + k] * b[n * uniforms.K + k];";
        } else if (!attributes.transA && !attributes.transB) {
          line = "value += a[m * uniforms.K + k] * b[k * uniforms.N + n];";
        }
        const calculateAlpha = attributes.alpha === 1 ? "" : "value *= uniforms.alpha;";
        const a = inputVariable("a", inputs[0].dataType, inputs[0].dims);
        const b = inputVariable("b", inputs[1].dataType, inputs[1].dims);
        const dataType = a.type.value;
        let c = null;
        const variables = [a, b];
        if (inputs.length === 3) {
          c = inputVariable("c", inputs[2].dataType, inputs[2].dims.length);
          variables.push(c);
        }
        const output = outputVariable("output", inputs[0].dataType, outputShape.length);
        variables.push(output);
        const uniforms = [
          { name: "output_size", type: "u32" },
          { name: "M", type: "u32" },
          { name: "N", type: "u32" },
          { name: "K", type: "u32" },
          { name: "alpha", type: "f32" },
          { name: "beta", type: "f32" }
        ];
        return `
  ${shaderHelper.registerUniforms(uniforms).declareVariables(...variables)}

  ${shaderHelper.mainStart()}
    ${shaderHelper.guardAgainstOutOfBoundsWorkgroupSizes("uniforms.output_size")}

    let m = global_idx / uniforms.N;
    let n = global_idx % uniforms.N;

    var value = ${dataType}(0);
    for (var k: u32 = 0u; k < uniforms.K; k++) {
      ${line}
    }

    ${calculateAlpha}
    ${(() => {
          if (c != null) {
            return `let cOffset = ${c.broadcastedIndicesToOffset("vec2(m, n)", output)}; value += ${dataType}(uniforms.beta) * ${c.getByOffset("cOffset")};`;
          }
          return "";
        })()}
    output[global_idx] = value;
  }`;
      };
      return {
        name: "Gemm",
        shaderCache: { hint: `${attributes.cacheKey}`, inputDependencies },
        getRunData: () => ({
          outputs: [{ dims: outputShape, dataType: inputs[0].dataType }],
          dispatchGroup: { x: Math.ceil(
            outputSize / 64
            /* workgroup size */
          ) },
          programUniforms
        }),
        getShaderSource
      };
    };
    parseGemmAttributes = (attributes) => {
      const transA = attributes.transA;
      const transB = attributes.transB;
      const alpha = attributes.alpha;
      const beta = attributes.beta;
      return {
        transA,
        transB,
        alpha,
        beta,
        cacheKey: `${attributes.transA};${attributes.transB};${attributes.alpha === 1}`
      };
    };
    gemm = (context, attributes) => {
      validateInputs16(context.inputs);
      context.compute(createGemmProgramInfo(context.inputs, attributes));
    };
  }
});

// web/lib/wasm/jsep/webgpu/ops/multihead-attention.ts
var getInput, validateInputs17, parseMultiHeadAttentionAttributes, weightTransposeAttribute2, addBiasTranspose, maybeTransposeToBNSHAndAddBias, multiHeadAttention;
var init_multihead_attention = __esm({
  "web/lib/wasm/jsep/webgpu/ops/multihead-attention.ts"() {
    "use strict";
    init_wasm_common();
    init_util();
    init_attribute_with_cache_key();
    init_types();
    init_attention();
    init_common();
    init_transpose();
    getInput = (inputs, i) => inputs.length > i && inputs[i].dims.length > 0 ? inputs[i] : void 0;
    validateInputs17 = (inputs, attributes) => {
      const query = inputs[0];
      const key = getInput(inputs, 1);
      const value = getInput(inputs, 2);
      const bias = getInput(inputs, 3);
      const keyPaddingMask = getInput(inputs, 4);
      const attentionBias = getInput(inputs, 5);
      const pastKey = getInput(inputs, 6);
      const pastValue = getInput(inputs, 7);
      if (query.dims.length !== 3 && query.dims.length !== 5) {
        throw new Error("Input query is expected to have 3 or 5 dimensions");
      }
      const batchSize = query.dims[0];
      const sequenceLength = query.dims[1];
      const hiddenSize = query.dims.length === 3 ? query.dims[2] : attributes.numHeads * query.dims[4];
      let kvSequenceLength = sequenceLength;
      let pastSequenceLength = 0;
      let maxSequenceLength = 0;
      const headSize = Math.floor(hiddenSize / attributes.numHeads);
      if (pastKey && pastValue && ShapeUtil.size(pastKey.dims) && ShapeUtil.size(pastValue.dims)) {
        if (pastKey.dims.length !== 4) {
          throw new Error('Input "past_key" is expected to have 4 dimensions');
        }
        if (pastKey.dims[0] !== batchSize || pastKey.dims[1] !== attributes.numHeads || pastKey.dims[3] !== headSize) {
          throw new Error('Input "past_key" shape (batch_size, num_heads, past_sequence_length, head_size)');
        }
        if (pastValue.dims[0] !== batchSize || pastValue.dims[1] !== attributes.numHeads || pastValue.dims[3] !== headSize) {
          throw new Error('Input "past_value" shape (batch_size, num_heads, past_sequence_length, head_size)');
        }
        if (pastKey.dims[2] !== pastValue.dims[2]) {
          throw new Error('Input "past_key" and "past_value" shall have same dim 2 (past_sequence_length)');
        }
        if (pastValue.dims.length !== 4) {
          throw new Error('Input "past_value" is expected to have 4 dimensions');
        }
        pastSequenceLength = pastKey.dims[2];
        maxSequenceLength = pastKey.dims[2];
      } else if (pastKey && ShapeUtil.size(pastKey.dims) || pastValue && ShapeUtil.size(pastValue.dims)) {
        throw new Error('Input "past_key" and "past_value" shall be both present or both absent');
      }
      let qkvFormat;
      if (key && ShapeUtil.size(key.dims) > 0) {
        if (query.dims.length !== 3) {
          throw new Error('Input "query" is expected to have 3 dimensions when key is given');
        }
        if (key.dims.length < 3 || key.dims.length > 5) {
          throw new Error('Input "key" is expected to have 3, 4, or 5 dimensions');
        }
        if (query.dims[0] !== key.dims[0]) {
          throw new Error('Input "query" and "key" shall have same dim 0 (batch size)');
        }
        if (key.dims.length === 3) {
          if (key.dims[2] !== query.dims[2]) {
            throw new Error('Input "query" and "key" shall have same dim 2 (hidden_size)');
          }
          qkvFormat = 2 /* qkvBSNH */;
          kvSequenceLength = key.dims[1];
        } else if (key.dims.length === 5) {
          if (key.dims[2] !== attributes.numHeads || key.dims[3] !== 2 || key.dims[4] !== headSize) {
            throw new Error('Expect "key" shape (batch_size, kv_sequence_length, num_heads, 2, head_size) for packed kv');
          }
          if (value) {
            throw new Error('Expect "value" be none when "key" has packed kv format.');
          }
          qkvFormat = 5 /* qKvBSNHxBSN2H */;
          kvSequenceLength = key.dims[1];
        } else {
          if (key.dims[1] !== attributes.numHeads || key.dims[3] !== headSize) {
            throw new Error('Expect "key" shape (batch_size, num_heads, kv_sequence_length, head_size) for past_key');
          }
          qkvFormat = 0 /* unknown */;
          kvSequenceLength = key.dims[2];
        }
      } else {
        if (query.dims.length !== 5) {
          throw new Error('Input "query" is expected to have 5 dimensions when key is empty');
        }
        if (query.dims[2] !== attributes.numHeads || query.dims[3] !== 3) {
          throw new Error('Expect "query" shape (batch_size, kv_sequence_length, num_heads, 3, head_size) for packed kv');
        }
        qkvFormat = 3 /* qkvBSN3H */;
      }
      if (bias && ShapeUtil.size(bias.dims) > 0) {
        if (bias.dims.length !== 1) {
          throw new Error('Input "bias" is expected to have 1 dimension');
        }
        if (key) {
          if (key.dims.length === 5 && key.dims[3] === 2) {
            throw new Error("bias is not allowed for packed kv.");
          }
        }
      }
      const totalSequenceLength = pastSequenceLength + kvSequenceLength;
      let maskType = 0 /* none */;
      if (keyPaddingMask && ShapeUtil.size(keyPaddingMask.dims) > 0) {
        maskType = 8 /* maskUnknown */;
        const maskDims = keyPaddingMask.dims;
        if (maskDims.length === 1) {
          if (maskDims[0] === batchSize) {
            maskType = 1 /* mask1dKeySeqLen */;
          } else if (maskDims[0] === 3 * batchSize + 2) {
            maskType = 3 /* mask1DKeySeqLenStart */;
          }
        } else if (maskDims.length === 2 && maskDims[0] === batchSize && maskDims[1] === totalSequenceLength) {
          maskType = 5 /* mask2dKeyPadding */;
        }
        if (maskType === 8 /* maskUnknown */) {
          throw new Error('Input "key_padding_mask" shape shall be (batch_size) or (batch_size, total_sequence_length)');
        }
        throw new Error("Mask not supported");
      }
      let passPastInKv = false;
      let vHiddenSize = hiddenSize;
      if (value && ShapeUtil.size(value.dims) > 0) {
        if (value.dims.length !== 3 && value.dims.length !== 4) {
          throw new Error('Input "value" is expected to have 3 or 4 dimensions');
        }
        if (query.dims[0] !== value.dims[0]) {
          throw new Error('Input "query" and "value" shall have same dim 0 (batch_size)');
        }
        if (value.dims.length === 3) {
          if (kvSequenceLength !== value.dims[1]) {
            throw new Error('Input "key" and "value" shall have the same dim 1 (kv_sequence_length)');
          }
          vHiddenSize = value.dims[2];
        } else {
          if (kvSequenceLength !== value.dims[2]) {
            throw new Error('Input "key" and "value" shall have the same dim 2 (kv_sequence_length)');
          }
          vHiddenSize = value.dims[1] * value.dims[3];
          passPastInKv = true;
        }
      }
      const broadcastResPosBias = false;
      if (keyPaddingMask && ShapeUtil.size(keyPaddingMask.dims) > 0) {
        throw new Error("Key padding mask is not supported");
      }
      if (attentionBias && ShapeUtil.size(attentionBias.dims) > 0) {
        if (attentionBias.dims.length !== 4) {
          throw new Error('Input "attention_bias" is expected to have 4 dimensions');
        }
        if (attentionBias.dims[0] !== batchSize || attentionBias.dims[1] !== attributes.numHeads || attentionBias.dims[2] !== sequenceLength || attentionBias.dims[3] !== totalSequenceLength) {
          throw new Error('Expect "attention_bias" shape (batch_size, num_heads, sequence_length, total_sequence_length)');
        }
      }
      return {
        batchSize,
        sequenceLength,
        pastSequenceLength,
        kvSequenceLength,
        totalSequenceLength,
        maxSequenceLength,
        inputHiddenSize: 0,
        hiddenSize,
        vHiddenSize,
        headSize,
        vHeadSize: Math.floor(vHiddenSize / attributes.numHeads),
        numHeads: attributes.numHeads,
        isUnidirectional: false,
        pastPresentShareBuffer: false,
        maskFilterValue: attributes.maskFilterValue,
        maskType,
        scale: attributes.scale,
        broadcastResPosBias,
        passPastInKv,
        qkvFormat
      };
    };
    parseMultiHeadAttentionAttributes = (attributes) => createAttributeWithCacheKey({ ...attributes });
    weightTransposeAttribute2 = createAttributeWithCacheKey({ perm: [0, 2, 1, 3] });
    addBiasTranspose = (context, qkv, bias, batchSize, sequenceLength, hiddenSize, biasOffset) => {
      const outputShape = [batchSize, sequenceLength, hiddenSize];
      const outputSize = ShapeUtil.size(outputShape);
      const programUniforms = [
        { type: 12 /* uint32 */, data: outputSize },
        { type: 12 /* uint32 */, data: biasOffset },
        { type: 12 /* uint32 */, data: hiddenSize }
      ];
      const getShaderSource = (shaderHelper) => {
        const output = outputVariable("qkv_with_bias", qkv.dataType, outputShape);
        const qkvInput = inputVariable("qkv", qkv.dataType, outputShape);
        const biasInput = inputVariable("bias", bias.dataType, outputShape);
        const uniforms = [
          { name: "output_size", type: "u32" },
          { name: "bias_offset", type: "u32" },
          { name: "hidden_size", type: "u32" }
        ];
        return `
  ${shaderHelper.registerUniforms(uniforms).declareVariables(qkvInput, biasInput, output)}
  ${shaderHelper.mainStart()}
    ${shaderHelper.guardAgainstOutOfBoundsWorkgroupSizes("uniforms.output_size")}
    let bias_offset_idx = (global_idx % uniforms.hidden_size) + uniforms.bias_offset;

    qkv_with_bias[global_idx] = qkv[global_idx] + bias[bias_offset_idx];
  }`;
      };
      return context.compute(
        {
          name: "MultiHeadAttentionAddBias",
          shaderCache: { inputDependencies: ["type", "type"] },
          getRunData: () => ({
            outputs: [{ dims: outputShape, dataType: qkv.dataType, gpuDataType: 0 /* default */ }],
            dispatchGroup: { x: Math.ceil(
              outputSize / 64
              /* workgroup size */
            ) },
            programUniforms
          }),
          getShaderSource
        },
        { inputs: [qkv, bias], outputs: [-1] }
      )[0];
    };
    maybeTransposeToBNSHAndAddBias = (context, batchSize, numHeads, sequenceLength, headSize, input, bias, biasOffset) => {
      let reshapedInput = input;
      if (!(bias && ShapeUtil.size(bias.dims) > 0)) {
        if (input.dims.length === 3) {
          reshapedInput = input.reshape([batchSize, sequenceLength, numHeads, headSize]);
        }
        return context.compute(createTransposeProgramInfo(reshapedInput, weightTransposeAttribute2.perm), {
          inputs: [reshapedInput],
          outputs: [-1]
        })[0];
      } else {
        if (sequenceLength === 1) {
          throw new Error("AddBiasReshape is not implemented. Please export your model with packed QKV or KV");
        } else {
          reshapedInput = addBiasTranspose(
            context,
            input,
            bias,
            batchSize,
            sequenceLength,
            numHeads * headSize,
            biasOffset
          );
          reshapedInput = reshapedInput.reshape([batchSize, sequenceLength, numHeads, headSize]);
          return context.compute(createTransposeProgramInfo(reshapedInput, weightTransposeAttribute2.perm), {
            inputs: [reshapedInput],
            outputs: [-1]
          })[0];
        }
      }
    };
    multiHeadAttention = (context, attributes) => {
      const params = validateInputs17(context.inputs, attributes);
      const query = context.inputs[0];
      const key = getInput(context.inputs, 1);
      const value = getInput(context.inputs, 2);
      const bias = getInput(context.inputs, 3);
      const keyPaddingMask = getInput(context.inputs, 4);
      const attentionBias = getInput(context.inputs, 5);
      const pastKey = getInput(context.inputs, 6);
      const pastValue = getInput(context.inputs, 7);
      if (query.dims.length === 5) {
        throw new Error("Packed QKV is not implemented");
      }
      if (key?.dims.length === 5) {
        throw new Error("Packed KV is not implemented");
      }
      const kvBNSH = key && value && key.dims.length === 4 && value.dims.length === 4;
      const Q = maybeTransposeToBNSHAndAddBias(
        context,
        params.batchSize,
        params.numHeads,
        params.sequenceLength,
        params.headSize,
        query,
        bias,
        0
      );
      if (kvBNSH) {
        return applyAttention(
          context,
          Q,
          key,
          value,
          keyPaddingMask,
          void 0,
          pastKey,
          pastValue,
          attentionBias,
          params,
          attributes
        );
      }
      if (!key || !value) {
        throw new Error("key and value must be provided");
      }
      const K = maybeTransposeToBNSHAndAddBias(
        context,
        params.batchSize,
        params.numHeads,
        params.kvSequenceLength,
        params.headSize,
        key,
        bias,
        params.hiddenSize
      );
      const V = maybeTransposeToBNSHAndAddBias(
        context,
        params.batchSize,
        params.numHeads,
        params.kvSequenceLength,
        params.vHeadSize,
        value,
        bias,
        2 * params.hiddenSize
      );
      applyAttention(context, Q, K, V, keyPaddingMask, void 0, pastKey, pastValue, attentionBias, params, attributes);
    };
  }
});

// web/lib/wasm/jsep/webgpu/ops/tile.ts
var getRepeats, validateInputs18, getOutputShape2, createTileProgramInfo, tile;
var init_tile = __esm({
  "web/lib/wasm/jsep/webgpu/ops/tile.ts"() {
    "use strict";
    init_wasm_common();
    init_util();
    init_common();
    getRepeats = (repeatsTensorView) => Array.from(repeatsTensorView.getBigInt64Array(), Number);
    validateInputs18 = (inputs) => {
      if (!inputs || inputs.length !== 2) {
        throw new Error("Tile requires 2 inputs.");
      }
      if (inputs[0].dataType !== 1 /* float */ && inputs[0].dataType !== 10 /* float16 */ && inputs[0].dataType !== 6 /* int32 */ && inputs[0].dataType !== 12 /* uint32 */) {
        throw new Error("Tile only support float, float16, int32, and uint32 data types");
      }
      if (inputs[1].dataType !== 7 /* int64 */) {
        throw new Error("Tile `repeats` input should be of int64 data type");
      }
      if (inputs[1].dims.length !== 1) {
        throw new Error("Tile `repeats` input should be 1-D");
      }
      const repeats = getRepeats(inputs[1]);
      if (repeats.length !== inputs[0].dims.length) {
        throw new Error("Tile `repeats` input should have same number of elements as rank of input data tensor");
      }
    };
    getOutputShape2 = (inputShape, repeats) => {
      const outputShape = [];
      for (let i = 0; i < inputShape.length; ++i) {
        outputShape.push(inputShape[i] * repeats[i]);
      }
      return outputShape;
    };
    createTileProgramInfo = (inputs, shape) => {
      const inputShape = inputs[0].dims;
      const repeats = shape == null ? getRepeats(inputs[1]) : shape;
      const outputShape = getOutputShape2(inputShape, repeats);
      const outputSize = ShapeUtil.size(outputShape);
      const dataType = inputs[0].dataType;
      const input = inputVariable("input", dataType, inputShape.length);
      const output = outputVariable("output", dataType, outputShape.length);
      const getShaderSource = (shaderHelper) => `
      const inputShape = ${input.indices(...inputShape)};
      ${shaderHelper.registerUniform("output_size", "u32").declareVariables(input, output)}
      ${shaderHelper.mainStart()}
      ${shaderHelper.guardAgainstOutOfBoundsWorkgroupSizes("uniforms.output_size")}
      let output_indices = ${output.offsetToIndices("global_idx")};
      var input_indices: ${input.type.indices};
      for (var i = 0; i < ${inputShape.length}; i++) {
        let input_dim_i = ${input.indicesGet("uniforms.input_shape", "i")};
        let input_dim_value = ${output.indicesGet("output_indices", "i")}  % input_dim_i;

        ${input.indicesSet("input_indices", "i", "input_dim_value")}
      }
      ${output.setByOffset("global_idx", input.getByIndices("input_indices"))}
    }`;
      return {
        name: "Tile",
        shaderCache: { hint: `${repeats}`, inputDependencies: ["rank"] },
        getRunData: () => ({
          outputs: [{ dims: outputShape, dataType: inputs[0].dataType }],
          dispatchGroup: { x: Math.ceil(
            outputSize / 64
            /* workgroup size */
          ) },
          programUniforms: [
            { type: 12 /* uint32 */, data: outputSize },
            ...createTensorShapeVariables(inputs[0].dims, outputShape)
          ]
        }),
        getShaderSource
      };
    };
    tile = (context) => {
      validateInputs18(context.inputs);
      context.compute(createTileProgramInfo(context.inputs), { inputs: [0] });
    };
  }
});

// web/lib/wasm/jsep/webgpu/ops/group-query-attention.ts
var validateInputs19, createConcatProgramInfo2, parseGroupQueryAttentionAttributes, weightTransposeAttribute3, maybeExpandAndTransposeToBNSH, groupQueryAttention;
var init_group_query_attention = __esm({
  "web/lib/wasm/jsep/webgpu/ops/group-query-attention.ts"() {
    "use strict";
    init_wasm_common();
    init_util();
    init_attribute_with_cache_key();
    init_attention();
    init_common();
    init_multihead_attention();
    init_tile();
    init_transpose();
    validateInputs19 = (inputs, attributes) => {
      const query = inputs[0];
      const key = inputs[1];
      const value = inputs[2];
      const pastKey = inputs[3];
      const pastValue = inputs[4];
      if (query.dims.length !== 3 && query.dims.length !== 5) {
        throw new Error("Input query is expected to have 3 or 5 dimensions");
      }
      const dmmhaPacking = false;
      const batchSize = query.dims[0];
      const sequenceLength = query.dims[1];
      const hiddenSize = query.dims.length === 3 ? dmmhaPacking ? query.dims[2] / 3 : query.dims[2] : attributes.numHeads * query.dims[4];
      let kvSequenceLength = sequenceLength;
      let pastSequenceLength = 0;
      let maxSequenceLength = 0;
      const headSize = Math.floor(hiddenSize / attributes.numHeads);
      const hasPastKey = pastKey && pastKey.dims.length !== 0;
      const hasPastValue = pastValue && pastValue.dims.length !== 0;
      const isPastkvBSNH = true;
      if (hasPastKey && hasPastValue) {
        if (pastKey.dims.length !== 4) {
          throw new Error('Input "past_key" is expected to have 4 dimensions');
        }
        if (pastValue.dims.length !== 4) {
          throw new Error('Input "past_value" is expected to have 4 dimensions');
        }
        if (isPastkvBSNH) {
          pastSequenceLength = pastKey.dims[1];
          maxSequenceLength = pastKey.dims[1];
        } else {
          pastSequenceLength = pastKey.dims[2];
          maxSequenceLength = pastKey.dims[2];
        }
      } else if (hasPastKey || hasPastValue) {
        throw new Error('Input "past_key" and "past_value" shall be both present or both absent');
      }
      let qkvFormat;
      if (key) {
        if (query.dims.length !== 3) {
          throw new Error('Input "query" is expected to have 3 dimensions when key is given');
        }
        if (key.dims.length < 3 || key.dims.length > 5) {
          throw new Error('Input "key" is expected to have 3, 4, or 5 dimensions');
        }
        if (query.dims[0] !== key.dims[0]) {
          throw new Error('Input "query" and "key" shall have same dim 0 (batch size)');
        }
        if (key.dims.length === 3) {
          if (query.dims[2] % key.dims[2] !== 0) {
            throw new Error('Dimension 2 of "query" should be a multiple of "key"');
          }
          qkvFormat = 2 /* qkvBSNH */;
          kvSequenceLength = key.dims[1];
        } else if (key.dims.length === 5) {
          if (key.dims[2] !== attributes.numHeads || key.dims[3] !== 2 || key.dims[4] !== headSize) {
            throw new Error('Expect "key" shape (batch_size, kv_sequence_length, num_heads, 2, head_size) for packed kv');
          }
          if (value) {
            throw new Error('Expect "value" be none when "key" has packed kv format.');
          }
          qkvFormat = 5 /* qKvBSNHxBSN2H */;
          kvSequenceLength = key.dims[1];
        } else {
          if (key.dims[1] !== attributes.numHeads || key.dims[3] !== headSize) {
            throw new Error('Expect "key" shape (batch_size, num_heads, kv_sequence_length, head_size) for past_key');
          }
          qkvFormat = 0 /* unknown */;
          kvSequenceLength = key.dims[2];
        }
      } else {
        if (query.dims.length !== 3 && query.dims.length !== 5) {
          throw new Error('Input "query" is expected to have 3 or 5 dimensions when key is empty');
        }
        if (query.dims.length === 5 && (query.dims[2] !== attributes.numHeads || query.dims[3] !== 3)) {
          throw new Error('Expect "query" shape (batch_size, kv_sequence_length, num_heads, 3, head_size) for packed kv');
        }
        qkvFormat = 3 /* qkvBSN3H */;
      }
      const maskType = 0 /* none */;
      let passPastInKv = false;
      let vHiddenSize = hiddenSize;
      if (value) {
        if (value.dims.length !== 3 && value.dims.length !== 4) {
          throw new Error('Input "value" is expected to have 3 or 4 dimensions');
        }
        if (query.dims[0] !== value.dims[0]) {
          throw new Error('Input "query" and "value" shall have same dim 0 (batch_size)');
        }
        if (value.dims.length === 3) {
          if (kvSequenceLength !== value.dims[1]) {
            throw new Error('Input "key" and "value" shall have the same dim 1 (kv_sequence_length)');
          }
          vHiddenSize = value.dims[2];
        } else {
          if (kvSequenceLength !== value.dims[2]) {
            throw new Error('Input "past_key" and "past_value" shall have the same dim 2 (kv_sequence_length)');
          }
          vHiddenSize = value.dims[1] * value.dims[3];
          passPastInKv = true;
        }
      }
      const totalSequenceLength = pastSequenceLength + kvSequenceLength;
      const broadcastResPosBias = false;
      return {
        batchSize,
        sequenceLength,
        pastSequenceLength,
        kvSequenceLength,
        totalSequenceLength,
        maxSequenceLength,
        inputHiddenSize: 0,
        hiddenSize,
        vHiddenSize,
        headSize,
        vHeadSize: Math.floor(vHiddenSize / attributes.kvNumHeads),
        numHeads: attributes.numHeads,
        kvNumHeads: attributes.kvNumHeads,
        nReps: attributes.numHeads / attributes.kvNumHeads,
        pastPresentShareBuffer: false,
        maskType,
        scale: attributes.scale,
        broadcastResPosBias,
        passPastInKv,
        qkvFormat,
        isPastkvBSNH
      };
    };
    createConcatProgramInfo2 = (a, b, dataType, params) => {
      const outputShape = [params.batchSize, params.totalSequenceLength, params.kvNumHeads, params.headSize];
      const component = 4;
      const outputSize = ShapeUtil.size(outputShape) / component;
      const presentSequenceLength = params.totalSequenceLength;
      const output = outputVariable("present_kv", dataType, outputShape.length, component);
      const inputA = inputVariable("new_kv", a.dataType, a.dims.length, component);
      const inputB = b ? inputVariable("past_kv", b.dataType, b.dims.length, component) : void 0;
      const H = Math.ceil(params.headSize / component);
      const dispatch = { x: presentSequenceLength, y: a.dims[0], z: 1 };
      const inputDependencies = b ? ["rank", "rank"] : ["rank"];
      const programUniforms = [
        { type: 12 /* uint32 */, data: outputSize },
        { type: 12 /* uint32 */, data: params.pastSequenceLength },
        { type: 12 /* uint32 */, data: params.kvSequenceLength },
        { type: 12 /* uint32 */, data: params.totalSequenceLength }
      ];
      const inputs = [inputA];
      if (inputB) {
        programUniforms.push(
          ...createTensorShapeVariables(a.dims),
          ...createTensorShapeVariables(b.dims),
          ...createTensorShapeVariables(outputShape)
        );
        inputs.push(inputB);
      } else {
        programUniforms.push(...createTensorShapeVariables(a.dims), ...createTensorShapeVariables(outputShape));
      }
      const uniforms = [
        { name: "output_size", type: "u32" },
        { name: "past_seqlen", type: "u32" },
        { name: "new_seqlen", type: "u32" },
        { name: "present_seqlen", type: "u32" }
      ];
      const pastStr = `      let past_batch_stride = uniforms.past_seqlen * num_heads * H;
        var past_head_stride = uniforms.past_seqlen * H;
        if (is_bsnh) {
          past_head_stride = H;
        }
        let in_offset = b * past_batch_stride + s * row_stride + n * past_head_stride + h;
        present_kv[out_offset] = past_kv[in_offset];`;
      const newStr = `      let new_batch_stride = uniforms.new_seqlen * num_heads * H;
        let new_row_stride = num_heads * H;
        let new_head_stride = H;
        let in_offset = b * new_batch_stride + (s - past_seqlen) * new_row_stride + n * new_head_stride + h;
        present_kv[out_offset] = new_kv[in_offset];`;
      const concatStr = b ? `if (s < past_seqlen) {
        ${pastStr}
        } else if (s < past_seqlen + uniforms.new_seqlen) {
        ${newStr}
        }` : `if (s < past_seqlen + uniforms.new_seqlen) {
          ${newStr}
        }`;
      const getShaderSource = (shaderHelper) => `

  ${shaderHelper.registerUniforms(uniforms).declareVariables(...inputs, output)}
  ${shaderHelper.mainStart([H, params.kvNumHeads, 1])}
    ${shaderHelper.guardAgainstOutOfBoundsWorkgroupSizes("uniforms.output_size")}
    var indices = ${output.offsetToIndices("global_idx")};
    let h = local_id.x;
    let n = local_id.y;
    let s = workgroup_id.x;
    let b = workgroup_id.y;
    let num_heads = ${params.kvNumHeads}u;
    let H = ${H}u;

    let present_seqlen = uniforms.present_seqlen;
    let present_batch_stride = present_seqlen * num_heads * H;
    var row_stride = H;
    let is_bsnh = ${params.isPastkvBSNH};

    if (is_bsnh) {
      row_stride = num_heads * H;
    }
    var present_head_stride = present_seqlen * H;
    if (is_bsnh) {
      present_head_stride = H;
    }

    let past_seqlen = uniforms.past_seqlen;

    let out_offset = b * present_batch_stride + s * row_stride + n * present_head_stride + h;
    ${concatStr}
  }`;
      return {
        name: "ConcatPastNew",
        shaderCache: { hint: `${params.kvNumHeads}${H}${!!b}`, inputDependencies },
        getRunData: () => ({
          outputs: [{ dims: outputShape, dataType }],
          dispatchGroup: dispatch,
          programUniforms
        }),
        getShaderSource
      };
    };
    parseGroupQueryAttentionAttributes = (attributes) => createAttributeWithCacheKey({ ...attributes });
    weightTransposeAttribute3 = createAttributeWithCacheKey({ perm: [0, 2, 1, 3] });
    maybeExpandAndTransposeToBNSH = (context, input, pastKV, params, outputIndex) => {
      let reshapedInput = input;
      const numHeads = params.kvNumHeads;
      const nReps = params.nReps;
      if (input.dims.length === 3 && params.kvSequenceLength !== 0) {
        reshapedInput = input.reshape([params.batchSize, params.kvSequenceLength, numHeads, params.headSize]);
      }
      if (pastKV) {
        reshapedInput = context.compute(createConcatProgramInfo2(reshapedInput, pastKV, reshapedInput.dataType, params), {
          inputs: [reshapedInput, pastKV],
          outputs: [params.isPastkvBSNH ? outputIndex : -1]
        })[0];
      } else {
        reshapedInput = context.compute(createConcatProgramInfo2(reshapedInput, void 0, reshapedInput.dataType, params), {
          inputs: [reshapedInput],
          outputs: [params.isPastkvBSNH ? outputIndex : -1]
        })[0];
      }
      if (nReps !== 1) {
        reshapedInput = context.compute(createTileProgramInfo([reshapedInput], [1, 1, 1, nReps]), {
          inputs: [reshapedInput],
          outputs: [-1]
        })[0];
        reshapedInput = reshapedInput.reshape([
          params.batchSize,
          params.totalSequenceLength,
          numHeads * nReps,
          params.headSize
        ]);
      }
      return context.compute(createTransposeProgramInfo(reshapedInput, weightTransposeAttribute3.perm), {
        inputs: [reshapedInput],
        outputs: [-1]
      })[0];
    };
    groupQueryAttention = (context, attributes) => {
      const params = validateInputs19(context.inputs, attributes);
      if (context.inputs[0].dims.length === 5) {
        throw new Error("Packed QKV is not implemented");
      }
      if (context.inputs[1]?.dims.length === 5) {
        throw new Error("Packed KV is not implemented");
      }
      const Q = maybeTransposeToBNSHAndAddBias(
        context,
        params.batchSize,
        params.numHeads,
        params.sequenceLength,
        params.headSize,
        context.inputs[0],
        void 0,
        0
      );
      const pastKey = context.inputs[3] && context.inputs[3].dims.length !== 0 ? context.inputs[3] : void 0;
      const pastValue = context.inputs[4] && context.inputs[4].dims.length !== 0 ? context.inputs[4] : void 0;
      const K = maybeExpandAndTransposeToBNSH(context, context.inputs[1], pastKey, params, 1);
      const V = maybeExpandAndTransposeToBNSH(context, context.inputs[2], pastValue, params, 2);
      applyAttention(context, Q, K, V, void 0, void 0, void 0, void 0, void 0, params, attributes);
    };
  }
});

// web/lib/wasm/jsep/webgpu/ops/instance-norm.ts
var createInstanceNormProgramInfo, computeMean, createInstanceNormNHWCProgramInfo, instanceNorm;
var init_instance_norm = __esm({
  "web/lib/wasm/jsep/webgpu/ops/instance-norm.ts"() {
    "use strict";
    init_wasm_common();
    init_util();
    init_common();
    createInstanceNormProgramInfo = (inputs, attributes) => {
      const xShape = inputs[0].dims;
      const outputShape = xShape;
      const axis = 2;
      const normCount = ShapeUtil.sizeToDimension(xShape, axis);
      const normSize = ShapeUtil.sizeFromDimension(xShape, axis);
      const components = getMaxComponents(normSize);
      const normPackedSize = normSize / components;
      const inputShape = [xShape[0], xShape[1], normPackedSize];
      const inputDependencies = ["rank", "type", "type"];
      const programUniforms = [
        { type: 12 /* uint32 */, data: normSize },
        { type: 12 /* uint32 */, data: normPackedSize }
      ];
      programUniforms.push(...createTensorShapeVariables(inputShape, inputShape));
      const getShaderSource = (shaderHelper) => {
        const x = inputVariable("x", inputs[0].dataType, inputShape.length, components);
        const scale = inputVariable("scale", inputs[1].dataType, inputs[1].dims);
        const bias = inputVariable("bias", inputs[2].dataType, inputs[2].dims);
        const output = outputVariable("output", inputs[0].dataType, inputShape.length, components);
        const variables = [x, scale, bias, output];
        const dataType = x.type.value;
        const f32Type = components === 1 ? "f32" : `vec${components}<f32>`;
        const workgroupSize = 64;
        const uniforms = [
          { name: "normSize", type: "u32" },
          { name: "normPackedSize", type: "u32" }
        ];
        return `
  var<workgroup> meanShared : f32;
  var<workgroup> squaredNormShared : f32;
  var<workgroup> workgroupShared : array<${f32Type}, ${workgroupSize}>;
  const workgroupSize = ${workgroupSize}u;
  ${shaderHelper.registerUniforms(uniforms).declareVariables(...variables)}
  ${shaderHelper.mainStart(workgroupSize)}
    let norm = global_idx / workgroupSize;
    let batch = norm / uniforms.x_shape[1];
    let channel = norm % uniforms.x_shape[1];
    let localIndex = local_id.x;

    // initialize workgroup memory
    var initial = ${f32Type}(0);
    for (var h = localIndex; h < uniforms.normPackedSize; h += workgroupSize) {
      initial = initial + ${f32Type}(${x.get("batch", "channel", "h")});
    }
    workgroupShared[localIndex] = initial;
    workgroupBarrier();

    // Calculate the mean of current channel data.
    for (var currSize = workgroupSize >> 1;  currSize > 0; currSize = currSize >> 1) {
      if (localIndex < currSize) {
        workgroupShared[localIndex] = workgroupShared[localIndex] + workgroupShared[localIndex + currSize];
      }
      workgroupBarrier();
    }
    if (localIndex == 0) {
      meanShared = ${sumVector("workgroupShared[0]", components)} / f32(uniforms.normSize);
    }
    workgroupBarrier();

    // reinitialize workgroup memory.
    initial = ${f32Type}(0);
    for (var h = localIndex; h < uniforms.normPackedSize; h += workgroupSize) {
      let deviation =  ${f32Type}(${x.get("batch", "channel", "h")}) - ${f32Type}(meanShared);
      initial = initial + deviation * deviation;
    }
    workgroupShared[localIndex] = initial;
    workgroupBarrier();

    // Calculate the sum of square of deviation of current channel data.
    for (var currSize = workgroupSize >> 1;  currSize > 0; currSize = currSize >> 1) {
      if (localIndex < currSize) {
        workgroupShared[localIndex] = workgroupShared[localIndex] + workgroupShared[localIndex + currSize];
      }
      workgroupBarrier();
    }
    if (localIndex == 0) {
      squaredNormShared = ${sumVector("workgroupShared[0]", components)};
    }
    workgroupBarrier();

    let invStdDev = inverseSqrt(squaredNormShared / f32(uniforms.normSize) + f32(${attributes.epsilon}));
    let channelScale = invStdDev * f32(${scale.getByOffset("channel")});
    let channelShift = f32(${bias.getByOffset("channel")}) - meanShared * channelScale;
    for (var h = localIndex; h < uniforms.normPackedSize; h += workgroupSize) {
      let value = ${x.get("batch", "channel", "h")} * ${dataType}(${f32Type}(channelScale)) + ${dataType}(${f32Type}(channelShift));
      ${output.set("batch", "channel", "h", "value")};
    }
  }`;
      };
      return {
        ...{ name: "InstanceNormalization" },
        // TODO: use epsilon as uniform. Currently epsilon as uniform fails test_instancenorm_epsilon.
        shaderCache: { hint: `${attributes.epsilon};${components}`, inputDependencies },
        getRunData: () => ({
          outputs: [{ dims: outputShape, dataType: inputs[0].dataType }],
          dispatchGroup: { x: normCount },
          programUniforms
        }),
        getShaderSource
      };
    };
    computeMean = (context, input, scale, bias, n, h, c, epsilon) => {
      const components = getMaxComponents(c);
      const WG = 64;
      const outputType = components === 1 ? "vec2f" : `mat2x${components}f`;
      const sumCastType = components === 1 ? "f32" : `vec${components}f`;
      const setOutputValue = (var1, var2) => `${outputType}(${var1}, ${var2})`;
      const unitsOfWork = n * c / components;
      const wgSize = Math.ceil(h / WG);
      const meanInputDependencies = ["type"];
      const meanProgramUniforms = [
        { type: 12 /* uint32 */, data: wgSize },
        { type: 12 /* uint32 */, data: h },
        { type: 12 /* uint32 */, data: Math.floor(c / components) },
        { type: 12 /* uint32 */, data: Math.floor(h * c / components) }
      ];
      const getMeanShaderSource = (shaderHelper) => {
        const inputHelper = inputVariable("input", input.dataType, input.dims, components);
        return `
  ${shaderHelper.declareVariables(inputHelper)}
  @group(0) @binding(1) var<storage, read_write> output : array<${outputType}>;
  struct Uniforms {wg_size:u32, H:u32, C:u32, image_size:u32};
  @group(0) @binding(2) var<uniform> uniforms: Uniforms;

  ${shaderHelper.mainStart(WG)}
    let currentImageNumber = global_idx / ${WG} / uniforms.C;
    let currentChannelNumber = (global_idx / ${WG}) % uniforms.C;
    let wgOffset = local_id.x * uniforms.wg_size;
    if (wgOffset >= uniforms.H) {
        return;
    }
    let wgMax = min(wgOffset + uniforms.wg_size, uniforms.H);

    let offset = currentImageNumber * uniforms.image_size + currentChannelNumber;
    var sum = ${fillVector("f32", components)};
    var squaredSum = ${fillVector("f32", components)};
    for (var i: u32 = wgOffset; i < wgMax; i++) {
        let value = ${sumCastType}(input[offset + i * uniforms.C]);
        sum += value;
        squaredSum += value * value;
    }
    output[global_idx] = ${setOutputValue("sum", "squaredSum")};
  }`;
      };
      const meanValues = context.compute(
        {
          name: "InstanceNormComputeMean",
          shaderCache: { hint: `${components}`, inputDependencies: meanInputDependencies },
          getRunData: () => ({
            outputs: [{ dims: [n, c, WG, 2], dataType: 1 /* float */ }],
            dispatchGroup: { x: n * c / components },
            programUniforms: meanProgramUniforms
          }),
          getShaderSource: getMeanShaderSource
        },
        { inputs: [input], outputs: [-1] }
      )[0];
      const programUniforms = [
        { type: 12 /* uint32 */, data: unitsOfWork },
        { type: 12 /* uint32 */, data: h },
        { type: 12 /* uint32 */, data: Math.floor(c / components) },
        { type: 12 /* uint32 */, data: Math.floor(WG * c / components) }
      ];
      const inputDependencies = ["type", "type", "type"];
      const getShaderSource = (shaderHelper) => {
        const scaleHelper = inputVariable("scale", scale.dataType, scale.dims, components);
        const biasHelper = inputVariable("bias", bias.dataType, bias.dims, components);
        return `
  @group(0) @binding(0) var<storage, read> input : array<${outputType}>;
  @group(0) @binding(1) var<storage, read> scale : array<${scaleHelper.type.storage}>;
  @group(0) @binding(2) var<storage, read> bias : array<${biasHelper.type.storage}>;
  @group(0) @binding(3) var<storage, read_write> output : array<${outputType}>;
  struct Uniforms {units_of_work : u32, H: u32, C : u32, image_size : u32};
  @group(0) @binding(4) var<uniform> uniforms: Uniforms;

  ${shaderHelper.mainStart()}
    ${shaderHelper.guardAgainstOutOfBoundsWorkgroupSizes("uniforms.units_of_work")}
    let currentImageNumber = global_idx / uniforms.C;
    let currentChannelNumber = global_idx % uniforms.C;

    let offset = currentImageNumber * uniforms.image_size;
    var sum = ${fillVector("f32", components)};
    var squaredSum = ${fillVector("f32", components)};
    for (var i: u32 = 0; i < min(${WG}, uniforms.H); i++) {
        let value = input[offset + i + currentChannelNumber * ${WG}];
        sum += value[0];
        squaredSum += value[1];
    }
    sum = sum / f32(uniforms.H);
    squaredSum = squaredSum / f32(uniforms.H);
    let invStdDev = inverseSqrt(squaredSum - sum * sum + f32(${epsilon}));
    let channelScale = invStdDev * ${sumCastType}(scale[currentChannelNumber]);
    let channelShift = ${sumCastType}(bias[currentChannelNumber]) - sum * channelScale;

    output[global_idx] = ${setOutputValue("channelScale", "channelShift")};
  }`;
      };
      return context.compute(
        {
          name: "InstanceNormComputeChannelScaleShift",
          // TODO: use epsilon as uniform. Currently epsilon as uniform fails test_instancenorm_epsilon.
          shaderCache: { hint: `${components};${epsilon}`, inputDependencies },
          getRunData: () => ({
            outputs: [{ dims: [n, c, 2], dataType: 1 /* float */ }],
            dispatchGroup: { x: Math.ceil(
              unitsOfWork / 64
              /* workgroup size */
            ) },
            programUniforms
          }),
          getShaderSource
        },
        { inputs: [meanValues, scale, bias], outputs: [-1] }
      )[0];
    };
    createInstanceNormNHWCProgramInfo = (context, inputs, attributes) => {
      const xShape = inputs[0].dims;
      const outputShape = xShape;
      const N = xShape[0];
      const C = xShape[xShape.length - 1];
      const H = ShapeUtil.sizeFromDimension(xShape, 1) / C;
      const components = getMaxComponents(C);
      const outputSize = ShapeUtil.size(outputShape) / components;
      const programUniforms = [
        { type: 12 /* uint32 */, data: H },
        { type: 12 /* uint32 */, data: Math.floor(C / components) }
      ];
      const inputDependencies = ["type", "type"];
      const channelScaleShift = computeMean(context, inputs[0], inputs[1], inputs[2], N, H, C, attributes.epsilon);
      const getShaderSource = (shaderHelper) => {
        const dataType = tensorTypeToWsglStorageType(inputs[0].dataType);
        const scaleType = components === 1 ? "vec2f" : `mat2x${components}f`;
        const scaleCastType = components === 1 ? dataType : `vec${components}<${dataType}>`;
        const inputHelper = inputVariable("input", inputs[0].dataType, inputs[0].dims, components);
        const outputHelper = outputVariable("output", inputs[0].dataType, outputShape, components);
        return `
  @group(0) @binding(0) var<storage, read> input : array<${inputHelper.type.storage}>;
  @group(0) @binding(1) var<storage, read> scaleInput : array<${scaleType}>;
  @group(0) @binding(2) var<storage, read_write> output : array<${outputHelper.type.storage}>;
  struct Uniforms {H: u32, C : u32};
  @group(0) @binding(3) var<uniform> uniforms: Uniforms;

  ${shaderHelper.mainStart()}
    let currentImageNumber = global_idx / (uniforms.C * uniforms.H);
    let currentChannelNumber = global_idx % uniforms.C;

    let scaleOffset = currentImageNumber * uniforms.C + currentChannelNumber;
    let scale = scaleInput[scaleOffset];
    output[global_idx] = fma(input[global_idx], ${scaleCastType}(scale[0]), ${scaleCastType}(scale[1]));
  }`;
      };
      context.compute(
        {
          name: "InstanceNormalizationNHWC",
          shaderCache: { hint: `${components}`, inputDependencies },
          getRunData: () => ({
            outputs: [{ dims: outputShape, dataType: inputs[0].dataType }],
            dispatchGroup: { x: Math.ceil(
              outputSize / 64
              /* workgroup size */
            ) },
            programUniforms
          }),
          getShaderSource
        },
        { inputs: [inputs[0], channelScaleShift] }
      );
    };
    instanceNorm = (context, attributes) => {
      if (attributes.format === "NHWC") {
        createInstanceNormNHWCProgramInfo(context, context.inputs, attributes);
      } else {
        context.compute(createInstanceNormProgramInfo(context.inputs, attributes));
      }
    };
  }
});

// web/lib/wasm/jsep/webgpu/ops/layer-norm.ts
var validateInputs20, createLayerNormProgramInfo, layerNorm;
var init_layer_norm = __esm({
  "web/lib/wasm/jsep/webgpu/ops/layer-norm.ts"() {
    "use strict";
    init_wasm_common();
    init_util();
    init_common();
    validateInputs20 = (inputs) => {
      if (!inputs || inputs.length < 2) {
        throw new Error("layerNorm requires at least 2 inputs.");
      }
    };
    createLayerNormProgramInfo = (inputs, attributes, outputCount) => {
      const simplified = attributes.simplified;
      const xShape = inputs[0].dims;
      const scale = inputs[1];
      const bias = !simplified && inputs[2];
      const outputShape = xShape;
      const axis = ShapeUtil.normalizeAxis(attributes.axis, xShape.length);
      const normCount = ShapeUtil.sizeToDimension(xShape, axis);
      const normSize = ShapeUtil.sizeFromDimension(xShape, axis);
      const scaleSize = ShapeUtil.size(scale.dims);
      const biasSize = bias ? ShapeUtil.size(bias.dims) : 0;
      if (scaleSize !== normSize || bias && biasSize !== normSize) {
        throw new Error(`Size of X.shape()[axis:] == ${normSize}.
       Size of scale and bias (if provided) must match this.
       Got scale size of ${scaleSize} and bias size of ${biasSize}`);
      }
      const meanInvStdDevDim = [];
      for (let i = 0; i < xShape.length; ++i) {
        if (i < axis) {
          meanInvStdDevDim.push(xShape[i]);
        } else {
          meanInvStdDevDim.push(1);
        }
      }
      const components = getMaxComponents(normSize);
      const inputDependencies = ["type", "type"];
      const programUniforms = [
        { type: 12 /* uint32 */, data: normCount },
        { type: 1 /* float */, data: normSize },
        { type: 12 /* uint32 */, data: Math.floor(normSize / components) },
        { type: 1 /* float */, data: attributes.epsilon }
      ];
      if (bias) {
        inputDependencies.push("type");
      }
      const hasMeanDataOutput = outputCount > 1;
      const hasInvStdOutput = outputCount > 2;
      const getShaderSource = (shaderHelper) => {
        const dataType = tensorTypeToWsglStorageType(inputs[0].dataType);
        const variables = [
          inputVariable("x", inputs[0].dataType, inputs[0].dims, components),
          inputVariable("scale", scale.dataType, scale.dims, components)
        ];
        if (bias) {
          variables.push(inputVariable("bias", bias.dataType, bias.dims, components));
        }
        variables.push(outputVariable("output", inputs[0].dataType, outputShape, components));
        if (hasMeanDataOutput) {
          variables.push(outputVariable("mean_data_output", 1 /* float */, meanInvStdDevDim));
        }
        if (hasInvStdOutput) {
          variables.push(outputVariable("inv_std_output", 1 /* float */, meanInvStdDevDim));
        }
        const uniforms = [
          { name: "norm_count", type: "u32" },
          { name: "norm_size", type: "f32" },
          { name: "norm_size_vectorized", type: "u32" },
          { name: "epsilon", type: "f32" }
        ];
        return `
  ${shaderHelper.registerUniforms(uniforms).declareVariables(...variables)}
  ${shaderHelper.mainStart()}
    ${shaderHelper.guardAgainstOutOfBoundsWorkgroupSizes("uniforms.norm_count")}
    let offset = global_idx * uniforms.norm_size_vectorized;
    var mean_vector = ${fillVector("f32", components)};
    var mean_square_vector = ${fillVector("f32", components)};

    for (var h: u32 = 0u; h < uniforms.norm_size_vectorized; h++) {
      let value = ${castToF32(dataType, components, "x[h + offset]")};
      mean_vector += value;
      mean_square_vector += value * value;
    }
    let mean = ${sumVector("mean_vector", components)} / uniforms.norm_size;
    let inv_std_dev = inverseSqrt(${sumVector("mean_square_vector", components)} / uniforms.norm_size ${simplified ? "" : "- mean * mean"} + uniforms.epsilon);

    for (var j: u32 = 0; j < uniforms.norm_size_vectorized; j++) {
      let f32input = ${castToF32(dataType, components, "x[j + offset]")};
      let f32scale = ${castToF32(dataType, components, "scale[j]")};
      output[j + offset] = ${variables[0].type.value}((f32input ${simplified ? "" : "- mean"}) * inv_std_dev * f32scale
        ${bias ? `+ ${castToF32(dataType, components, "bias[j]")}` : ""}
      );
    }

    ${hasMeanDataOutput ? "mean_data_output[global_idx] = mean" : ""};
    ${hasInvStdOutput ? "inv_std_output[global_idx] = inv_std_dev" : ""};
  }`;
      };
      const outputs = [{ dims: outputShape, dataType: inputs[0].dataType }];
      if (hasMeanDataOutput) {
        outputs.push({ dims: meanInvStdDevDim, dataType: 1 /* float */ });
      }
      if (hasInvStdOutput) {
        outputs.push({ dims: meanInvStdDevDim, dataType: 1 /* float */ });
      }
      return {
        name: "LayerNormalization",
        shaderCache: { hint: `${components};${outputCount};${simplified}`, inputDependencies },
        getRunData: () => ({
          outputs,
          dispatchGroup: { x: Math.ceil(
            normCount / 64
            /* workgroup size */
          ) },
          programUniforms
        }),
        getShaderSource
      };
    };
    layerNorm = (context, attributes) => {
      validateInputs20(context.inputs);
      context.compute(createLayerNormProgramInfo(context.inputs, attributes, context.outputCount));
    };
  }
});

// web/lib/wasm/jsep/webgpu/ops/matmulnbits.ts
var validateInputs21, createMatMulNBitsProgramInfo, matMulNBits, parseMatMulNBitsAttributes;
var init_matmulnbits = __esm({
  "web/lib/wasm/jsep/webgpu/ops/matmulnbits.ts"() {
    "use strict";
    init_wasm_common();
    init_util();
    init_attribute_with_cache_key();
    init_common();
    validateInputs21 = (inputs, attributes) => {
      if (inputs.length < 3 || inputs.length > 4) {
        throw new Error("MatMulNBits requires 3 or 4 inputs");
      }
      const a = inputs[0];
      const aRank = a.dims.length;
      if (a.dims[aRank - 1] !== attributes.k) {
        throw new Error("The last dim of input shape does not match the k value");
      }
      const nBlocksPerCol = Math.floor((attributes.k + attributes.blockSize - 1) / attributes.blockSize);
      const blobSize = attributes.blockSize / 8 * attributes.bits;
      const b = inputs[1];
      if (!ShapeUtil.areEqual(b.dims, [attributes.n, nBlocksPerCol, blobSize])) {
        throw new Error("The second inputs must be 3D tensor with shape N X nBlocksPerCol X blobSize");
      }
      const scales = inputs[2];
      const scalesShape = scales.dims;
      if (ShapeUtil.size(scalesShape) !== attributes.n * nBlocksPerCol) {
        throw new Error("scales input size error.");
      }
      if (inputs.length === 4) {
        const zeroPoints = inputs[3];
        const zeroPointsShape = zeroPoints.dims;
        const expectedZeroPointsSize = attributes.bits > 4 ? attributes.n * nBlocksPerCol : attributes.n * Math.floor((nBlocksPerCol + 1) / 2);
        if (ShapeUtil.size(zeroPointsShape) !== expectedZeroPointsSize) {
          throw new Error("zeroPoints input size error.");
        }
      }
    };
    createMatMulNBitsProgramInfo = (inputs, attributes) => {
      const inputShape = inputs[0].dims;
      const aRank = inputShape.length;
      const dimAOuter = inputShape[aRank - 2];
      const dimInner = attributes.k;
      const dimBOuter = attributes.n;
      const batchDims = inputShape.slice(0, aRank - 2);
      const batchSize = ShapeUtil.size(batchDims);
      const blobSize = inputs[1].dims[2];
      const blobSizeInWords = blobSize / 4;
      const dataType = inputs[0].dataType;
      const aComponents = getMaxComponents(attributes.k);
      const bComponents = getMaxComponents(blobSizeInWords);
      const components = getMaxComponents(dimBOuter);
      const outputShape = batchDims.concat([dimAOuter, dimBOuter]);
      const outputNumber = dimAOuter > 1 && dimBOuter / components % 2 === 0 ? 2 : 1;
      const dispatchSize = ShapeUtil.size(outputShape) / components / outputNumber;
      const workgroupSize = 64;
      const programUniforms = [];
      const inputShapeTemp = [batchSize, dimAOuter, dimInner / aComponents];
      const bShape = ShapeUtil.convertShape(inputs[1].dims).slice();
      bShape.splice(-1, 1, blobSizeInWords / bComponents);
      programUniforms.push(...createTensorShapeVariables(inputShapeTemp));
      programUniforms.push(...createTensorShapeVariables(bShape));
      programUniforms.push(...createTensorShapeVariables(inputs[2].dims));
      if (inputs.length === 4) {
        programUniforms.push(...createTensorShapeVariables(ShapeUtil.convertShape(inputs[3].dims)));
      }
      const outputShapeTemp = [batchSize, dimAOuter, dimBOuter / components];
      programUniforms.push(...createTensorShapeVariables(outputShapeTemp));
      const getShaderSource = (shaderHelper) => {
        const inputRank = inputShapeTemp.length;
        const a = inputVariable("a", inputs[0].dataType, inputRank, aComponents);
        const b = inputVariable("b", 12 /* uint32 */, bShape.length, bComponents);
        const scales = inputVariable("scales", inputs[2].dataType, inputs[2].dims.length);
        const inputVariables = [a, b, scales];
        const zeroPoints = inputs.length === 4 ? inputVariable("zero_points", 12 /* uint32 */, inputs[3].dims.length) : void 0;
        if (zeroPoints) {
          inputVariables.push(zeroPoints);
        }
        const outputRank = outputShapeTemp.length;
        const output = outputVariable("output", inputs[0].dataType, outputRank, components);
        const dataType2 = tensorTypeToWsglStorageType(inputs[0].dataType);
        const qDqDataType = (() => {
          switch (aComponents) {
            case 1:
              return `array<${dataType2}, 8>`;
            case 2:
              return `mat4x2<${dataType2}>`;
            case 4:
              return `mat2x4<${dataType2}>`;
            default:
              throw new Error(`${aComponents}-component is not supported.`);
          }
        })();
        const processOneWord = () => {
          let calcStr = `
          // reuse a data
            var input_offset = ${a.indicesToOffset(`${a.type.indices}(batch, row, word_offset)`)};
            var a_data: ${qDqDataType};
            for (var j: u32 = 0; j < ${8 / aComponents}; j++) {
              a_data[j] = ${a.getByOffset("input_offset")};
              input_offset++;
            }
          `;
          for (let c = 0; c < components * outputNumber; c++) {
            calcStr += `
            b_value = ${bComponents === 1 ? `b${c}_data` : `b${c}_data[i]`};
            b_value_lower = unpack4xU8(b_value & b_mask);
            b_value_upper = unpack4xU8((b_value >> 4) & b_mask);
            b_quantized_values = ${qDqDataType}(${Array.from(
              { length: 4 },
              (_, i) => `${dataType2}(b_value_lower[${i}]), ${dataType2}(b_value_upper[${i}])`
            ).join(", ")});
            b_dequantized_values = ${(() => {
              if (aComponents === 1) {
                return `${qDqDataType}(${Array.from(
                  { length: 8 },
                  (_, i) => `(b_quantized_values[${i}] - ${zeroPoints ? `zero_point${c}` : "zero_point"}) * scale${c}`
                ).join(", ")});`;
              } else {
                return `(b_quantized_values - ${qDqDataType}(${Array(8).fill(`${zeroPoints ? `zero_point${c}` : "zero_point"}`).join(",")})) * scale${c};`;
              }
            })()};
            workgroup_shared[local_id.x * ${outputNumber} + ${Math.floor(c / components)}]${components > 1 ? `[${c % components}]` : ""} += ${Array.from(
              { length: 8 / aComponents },
              (_, i) => `${aComponents === 1 ? `a_data[${i}] * b_dequantized_values[${i}]` : `dot(a_data[${i}], b_dequantized_values[${i}])`}`
            ).join(" + ")};
          `;
          }
          return calcStr;
        };
        const prepareScaleAndZeroPoint = () => {
          let calcStr = `
            var col_index = col * ${components};
            ${zeroPoints ? `
            let zero_point_bytes_per_col = (nBlocksPerCol + 1) / 2;
            var zero_point_byte_count: u32;
            var zero_point_word_index: u32;
            var zero_point_byte_offset: u32;
            let zero_point_nibble_offset: u32 = block & 0x1u;
            var zero_point_bits_offset: u32;
            var zero_point_word: u32;` : `
            // The default zero point is 8 for unsigned 4-bit quantization.
            let zero_point = ${dataType2}(${8});`}
            `;
          for (let c = 0; c < components * outputNumber; c++) {
            calcStr += `
            let scale${c} = ${scales.getByOffset(`col_index * nBlocksPerCol + block`)};
            ${zeroPoints ? `
            zero_point_byte_count = col_index * zero_point_bytes_per_col + (block >> 0x1u);
            zero_point_word_index = zero_point_byte_count >> 0x2u;
            zero_point_byte_offset = zero_point_byte_count & 0x3u;
            zero_point_bits_offset = (zero_point_byte_offset << 3) + (zero_point_nibble_offset << 2);
            zero_point_word = ${zeroPoints.getByOffset("zero_point_word_index")} >> zero_point_bits_offset;
            let zero_point${c} = ${dataType2}((zero_point_word) & 0xFu);` : ""}
            col_index += 1;`;
          }
          return calcStr;
        };
        const prepareBData = () => {
          let calcStr = `col_index = col * ${components};`;
          for (let c = 0; c < components * outputNumber; c++) {
            calcStr += `
            let b${c}_data = ${b.getByIndices(`${b.type.indices}(col_index, block, word)`)};
            col_index += 1;`;
          }
          calcStr += `
            var b_value: u32;
            let b_mask: u32 = 0x0F0F0F0Fu;
            var b_value_lower: vec4<u32>;
            var b_value_upper: vec4<u32>;
            var b_quantized_values: ${qDqDataType};
            var b_dequantized_values: ${qDqDataType};`;
          return calcStr;
        };
        return `
        var<workgroup> workgroup_shared: array<${output.type.value}, ${outputNumber * workgroupSize}>;
        ${shaderHelper.declareVariables(...inputVariables, output)}
        ${shaderHelper.mainStart([workgroupSize, 1, 1])}
          let output_indices = ${output.offsetToIndices(`(global_idx / ${workgroupSize}) * ${outputNumber}`)};
          let col = output_indices[2];
          let row = output_indices[1];
          let batch = output_indices[0];
          let nBlocksPerCol = uniforms.b_shape[1];

          for (var block = local_id.x; block < nBlocksPerCol; block += ${workgroupSize}) {
            //process one block
            var word_offset: u32 = block * ${attributes.blockSize / aComponents};
            ${prepareScaleAndZeroPoint()}
            for (var word: u32 = 0; word < ${blobSizeInWords}; word += ${bComponents}) {
              ${prepareBData()}
              for (var i: u32 = 0; i < ${bComponents}; i++) {
                ${processOneWord()}
                word_offset += ${8 / aComponents};
              }
            }
          }
          workgroupBarrier();

          if (local_id.x < ${outputNumber}) {
            var output_value: ${output.type.value} = ${output.type.value}(0);
            var workgroup_shared_offset: u32 = local_id.x;
            for (var b: u32 = 0u; b < ${workgroupSize}u; b++) {
              output_value += workgroup_shared[workgroup_shared_offset];
              workgroup_shared_offset += ${outputNumber};
            }
            ${output.setByIndices(`${output.type.indices}(batch, row, col + local_id.x)`, "output_value")};
          }
        }`;
      };
      return {
        name: "MatMulNBits",
        shaderCache: {
          hint: `${attributes.blockSize};${attributes.bits};${aComponents};${bComponents};${components};${outputNumber};${workgroupSize}`,
          inputDependencies: Array(inputs.length).fill("rank")
        },
        getRunData: () => ({
          outputs: [{ dims: outputShape, dataType }],
          dispatchGroup: { x: dispatchSize },
          programUniforms
        }),
        getShaderSource
      };
    };
    matMulNBits = (context, attributes) => {
      validateInputs21(context.inputs, attributes);
      context.compute(createMatMulNBitsProgramInfo(context.inputs, attributes));
    };
    parseMatMulNBitsAttributes = (attributes) => createAttributeWithCacheKey(attributes);
  }
});

// web/lib/wasm/jsep/webgpu/ops/pad.ts
var validateInputs22, getPadConstant, getPadReflect, getPadEdge, getPadWrap, getPadSnippet, createPadProgramInfo, createPadAttributesFromInputs, pad;
var init_pad = __esm({
  "web/lib/wasm/jsep/webgpu/ops/pad.ts"() {
    "use strict";
    init_wasm_common();
    init_util();
    init_common();
    validateInputs22 = (inputs) => {
      if (!inputs || inputs.length < 1) {
        throw new Error("Too few inputs");
      }
      if (inputs[0].dataType !== 1 /* float */ && inputs[0].dataType !== 10 /* float16 */) {
        throw new Error("Input type must be float or float16.");
      }
      if (inputs.length >= 2) {
        let validPads = inputs[0].dims.length * 2 === inputs[1].dims[0];
        if (inputs.length === 4) {
          validPads = inputs[3].dims[0] * 2 === inputs[1].dims[0];
        }
        if (!validPads) {
          throw new Error("The pads should be a 1D tensor of shape [2 * input_rank] or [2 * num_axes].");
        }
      }
    };
    getPadConstant = (output, inputRank, padsLength) => {
      let block = "";
      for (let i = inputRank - 1; i >= 0; --i) {
        block += `
            k = i32(${output.indicesGet("indices", i)}) - ${getElementAt("uniforms.pads", i, padsLength)};
            if (k < 0) {
              break;
            }
            if (k >= i32(${getElementAt("uniforms.x_shape", i, inputRank)})) {
              break;
            }
            offset += k * i32(${getElementAt("uniforms.x_strides", i, inputRank)});
        `;
      }
      return `
          value = ${output.type.value}(uniforms.constant_value);
          for (var i = 0; i < 1; i++) {
            var offset = 0;
            var k = 0;
            ${block}
            value = x[offset];
          }
      `;
    };
    getPadReflect = (output, inputRank, padsLength) => {
      let block = "";
      for (let i = inputRank - 1; i >= 0; --i) {
        block += `
                k = i32(${output.indicesGet("indices", i)}) - ${getElementAt("uniforms.pads", i, padsLength)};
                if (k < 0) {
                  k = -k;
                }
                {
                  let _2n_1 = 2 * (i32(${getElementAt("uniforms.x_shape", i, inputRank)}) - 1);
                  k = k % _2n_1;
                  if(k >= i32(${getElementAt("uniforms.x_shape", i, inputRank)})) {
                    k = _2n_1 - k;
                  }
                }
                offset += k * i32(${getElementAt("uniforms.x_strides", i, inputRank)});
            `;
      }
      return `
              var offset = 0;
              var k = 0;
              ${block}
              value = x[offset];
          `;
    };
    getPadEdge = (output, inputRank, padsLength) => {
      let block = "";
      for (let i = inputRank - 1; i >= 0; --i) {
        block += `
                k = i32(${output.indicesGet("indices", i)}) - ${getElementAt("uniforms.pads", i, padsLength)};
                if (k < 0) {
                  k = 0;
                }
                if (k >= i32(${getElementAt("uniforms.x_shape", i, inputRank)})) {
                  k = i32(${getElementAt("uniforms.x_shape", i, inputRank)}) - 1;
                }
                offset += k * i32(${getElementAt("uniforms.x_strides", i, inputRank)});
            `;
      }
      return `
              var offset = 0;
              var k = 0;
              ${block}
              value = x[offset];
          `;
    };
    getPadWrap = (output, inputRank, padsLength) => {
      let block = "";
      for (let i = inputRank - 1; i >= 0; --i) {
        block += `
                k = i32(${output.indicesGet("indices", i)}) - ${getElementAt("uniforms.pads", i, padsLength)};
                if (k < 0)  {
                  k += i32(${getElementAt("uniforms.x_shape", i, inputRank)}]);
                }
                if (k >= i32(${getElementAt("uniforms.x_shape", i, inputRank)})) {
                  k -= i32(${getElementAt("uniforms.x_shape", i, inputRank)});
                }
                offset += k * i32(${getElementAt("uniforms.x_strides", i, inputRank)});
            `;
      }
      return `
              var offset = 0;
              var k = 0;
              ${block}
              value = x[offset];
          `;
    };
    getPadSnippet = (output, inputRank, attributes) => {
      switch (attributes.mode) {
        case 0:
          return getPadConstant(output, inputRank, attributes.pads.length);
        case 1:
          return getPadReflect(output, inputRank, attributes.pads.length);
        case 2:
          return getPadEdge(output, inputRank, attributes.pads.length);
        case 3:
          return getPadWrap(output, inputRank, attributes.pads.length);
        default:
          throw new Error("Invalid mode");
      }
    };
    createPadProgramInfo = (inputs, attributes) => {
      const outputShape = ShapeUtil.padShape(inputs[0].dims.slice(), attributes.pads);
      const inputDims = inputs[0].dims;
      const outputSize = ShapeUtil.size(outputShape);
      const programUniforms = [
        { type: 12 /* uint32 */, data: outputSize },
        { type: 6 /* int32 */, data: attributes.pads }
      ];
      const isValueFromInput = inputs.length >= 3 && inputs[2].data;
      if (attributes.mode === 0) {
        programUniforms.push({ type: isValueFromInput ? inputs[2].dataType : 1 /* float */, data: attributes.value });
      }
      programUniforms.push(...createTensorShapeVariables(inputs[0].dims, outputShape));
      const inputDependencies = ["rank"];
      const getShaderSource = (shaderHelper) => {
        const output = outputVariable("output", inputs[0].dataType, outputShape.length);
        const input = inputVariable("x", inputs[0].dataType, inputDims.length);
        const dataType = input.type.value;
        const padSnippet = getPadSnippet(output, inputDims.length, attributes);
        const uniforms = [
          { name: "output_size", type: "u32" },
          { name: "pads", type: "i32", length: attributes.pads.length }
        ];
        if (attributes.mode === 0) {
          uniforms.push({ name: "constant_value", type: isValueFromInput ? dataType : "f32" });
        }
        return `
            ${shaderHelper.registerUniforms(uniforms).declareVariables(input, output)}
            ${shaderHelper.mainStart()}
            ${shaderHelper.guardAgainstOutOfBoundsWorkgroupSizes("uniforms.output_size")}

            let indices = ${output.offsetToIndices("global_idx")};

            var value = ${dataType}(0);
            ${padSnippet}
            output[global_idx] = value;
        }`;
      };
      return {
        name: "Pad",
        shaderCache: { hint: `${attributes.mode}${isValueFromInput}`, inputDependencies },
        getRunData: () => ({
          outputs: [{ dims: outputShape, dataType: inputs[0].dataType }],
          dispatchGroup: { x: Math.ceil(
            ShapeUtil.size(outputShape) / 64
            /* workgroup size */
          ) },
          programUniforms
        }),
        getShaderSource
      };
    };
    createPadAttributesFromInputs = (inputs, attributes) => {
      if (inputs.length > 1) {
        const bigInt64Pads = inputs[1].getBigInt64Array();
        const value = inputs.length >= 3 && inputs[2].data ? inputs[2].dataType === 10 /* float16 */ ? inputs[2].getUint16Array()[0] : inputs[2].getFloat32Array()[0] : 0;
        const inputRank = inputs[0].dims.length;
        const updatePads = new Int32Array(2 * inputRank).fill(0);
        if (inputs.length >= 4) {
          const axes = inputs[3].getBigInt64Array();
          for (let i = 0; i < axes.length; i++) {
            updatePads[Number(axes[i])] = Number(bigInt64Pads[i]);
            updatePads[Number(axes[i]) + inputRank] = Number(bigInt64Pads[i + axes.length]);
          }
        } else {
          bigInt64Pads.forEach((v, i) => updatePads[Number(i)] = Number(v));
        }
        const pads = [];
        updatePads.forEach((v) => pads.push(v));
        return { mode: attributes.mode, value, pads };
      } else {
        return attributes;
      }
    };
    pad = (context, attributes) => {
      validateInputs22(context.inputs);
      const updatedAttributes = createPadAttributesFromInputs(context.inputs, attributes);
      context.compute(createPadProgramInfo(context.inputs, updatedAttributes), { inputs: [0] });
    };
  }
});

// web/lib/wasm/jsep/webgpu/ops/pool.ts
var validateInputs23, getAdjustedPoolAttributesAndOutputShape, getUniformAndPadInfo, generatePoolingCode, createShaderKeyFromAttributes, createAveragePoolShaderKeyFromAttributes, createMaxPoolShaderKeyFromAttributes, parsePoolCommonAttributes, createAveragePoolProgramInfo, parseAveragePoolAttributes, averagePool, globalPoolAttributes, parseGlobalAveragePoolAttributes, globalAveragePool, createMaxPoolProgramInfo, maxPool, parseMaxPoolAttributes, parseGlobalMaxPoolAttributes, globalMaxPool;
var init_pool = __esm({
  "web/lib/wasm/jsep/webgpu/ops/pool.ts"() {
    "use strict";
    init_esm();
    init_wasm_common();
    init_util();
    init_common();
    validateInputs23 = (inputs) => {
      if (env2.webgpu.validateInputContent && (!inputs || inputs.length !== 1)) {
        throw new Error("Pool ops requires 1 input.");
      }
    };
    getAdjustedPoolAttributesAndOutputShape = (input, attributes, isGlobalOperator) => {
      const isChannelsLast = attributes.format === "NHWC";
      const inputShapeAsChannelFirst = input.dims.slice();
      if (isChannelsLast) {
        inputShapeAsChannelFirst.splice(1, 0, inputShapeAsChannelFirst.pop());
      }
      const hasDilations = Object.hasOwnProperty.call(attributes, "dilations");
      const kernelShape = attributes.kernelShape.slice();
      const strides = attributes.strides.slice();
      const dilations = hasDilations ? attributes.dilations.slice() : [];
      const pads = attributes.pads.slice();
      PoolConvUtil.adjustPoolAttributes(isGlobalOperator, inputShapeAsChannelFirst, kernelShape, strides, dilations, pads);
      const outputShapeAsChannelFirst = PoolConvUtil.computePoolOutputShape(
        isGlobalOperator,
        inputShapeAsChannelFirst,
        strides,
        dilations,
        kernelShape,
        pads,
        attributes.autoPad
      );
      const newAttributes = Object.assign({}, attributes);
      if (hasDilations) {
        Object.assign(newAttributes, { kernelShape, strides, pads, dilations, cacheKey: attributes.cacheKey });
      } else {
        Object.assign(newAttributes, { kernelShape, strides, pads, cacheKey: attributes.cacheKey });
      }
      const outputShapeAsChannelLast = outputShapeAsChannelFirst.slice();
      outputShapeAsChannelLast.push(outputShapeAsChannelLast.splice(1, 1)[0]);
      return [newAttributes, isChannelsLast ? outputShapeAsChannelLast : outputShapeAsChannelFirst];
    };
    getUniformAndPadInfo = (outputShape, attributes) => {
      const isChannelsLast = attributes.format === "NHWC";
      const outputSize = ShapeUtil.size(outputShape);
      const kernelSize = ShapeUtil.size(attributes.kernelShape);
      const programUniforms = [
        { type: 12 /* uint32 */, data: outputSize },
        { type: 12 /* uint32 */, data: kernelSize }
      ];
      const uniforms = [
        { name: "outputSize", type: "u32" },
        { name: "kernelSize", type: "u32" }
      ];
      if (attributes.kernelShape.length <= 2) {
        const kw = attributes.kernelShape[attributes.kernelShape.length - 1];
        const sw = attributes.strides[attributes.strides.length - 1];
        const pwStart = attributes.pads[attributes.pads.length / 2 - 1];
        const pwEnd = attributes.pads[attributes.pads.length - 1];
        const pwStartEndNotZero = !!(pwStart + pwEnd);
        programUniforms.push(
          { type: 12 /* uint32 */, data: kw },
          { type: 12 /* uint32 */, data: sw },
          { type: 12 /* uint32 */, data: pwStart },
          { type: 12 /* uint32 */, data: pwEnd }
        );
        uniforms.push(
          { name: "kw", type: "u32" },
          { name: "sw", type: "u32" },
          { name: "pwStart", type: "u32" },
          { name: "pwEnd", type: "u32" }
        );
        let phStartEndNotZero = false;
        if (attributes.kernelShape.length === 2) {
          const kh = attributes.kernelShape[attributes.kernelShape.length - 2];
          const sh = attributes.strides[attributes.strides.length - 2];
          const phStart = attributes.pads[attributes.pads.length / 2 - 2];
          const phEnd = attributes.pads[attributes.pads.length - 2];
          phStartEndNotZero = !!(phStart + phEnd);
          programUniforms.push(
            { type: 12 /* uint32 */, data: kh },
            { type: 12 /* uint32 */, data: sh },
            { type: 12 /* uint32 */, data: phStart },
            { type: 12 /* uint32 */, data: phEnd }
          );
          uniforms.push(
            { name: "kh", type: "u32" },
            { name: "sh", type: "u32" },
            { name: "phStart", type: "u32" },
            { name: "phEnd", type: "u32" }
          );
        }
        return [programUniforms, uniforms, true, pwStartEndNotZero, phStartEndNotZero];
      } else {
        if (isChannelsLast) {
          throw new Error("Pooling with kernelShape.length > 2 is not supported for NHWC format.");
        }
        const kernelStrides = ShapeUtil.computeStrides(attributes.kernelShape);
        programUniforms.push(
          { type: 12 /* uint32 */, data: kernelStrides },
          { type: 12 /* uint32 */, data: attributes.pads },
          { type: 12 /* uint32 */, data: attributes.strides }
        );
        uniforms.push(
          { name: "kernelStrides", type: "u32", length: kernelStrides.length },
          { name: "pads", type: "u32", length: attributes.pads.length },
          { name: "strides", type: "u32", length: attributes.strides.length }
        );
        const hasPads = attributes.pads.reduce((sum, cur) => sum + cur);
        return [programUniforms, uniforms, !!hasPads, false, false];
      }
    };
    generatePoolingCode = (shaderHelper, x, rank, outputShapeRank, attributes, op1, op2, start, uniforms, hasPads, pwStartEndNotZero, phStartEndNotZero) => {
      const isChannelsLast = attributes.format === "NHWC";
      const dataType = x.type.value;
      const output = outputVariable("output", x.type.tensor, outputShapeRank);
      if (attributes.kernelShape.length <= 2) {
        let codeW = "";
        let codeH = "";
        let codeHEnd = "";
        const dimIdxW = rank - (isChannelsLast ? 2 : 1);
        if (pwStartEndNotZero) {
          codeW = `
                for (var i: u32 = 0u; i < uniforms.kw; i++) {
                  xIndices[${dimIdxW}] = indices[${dimIdxW}] * uniforms.sw - uniforms.pwStart + i;
                  if (xIndices[${dimIdxW}] < 0 || xIndices[${dimIdxW}]
                      >= uniforms.x_shape[${dimIdxW}]) {
                    pad++;
                    continue;
                  }
                  let x_val = x[${x.indicesToOffset("xIndices")}];
                  ${op1}
                }`;
        } else {
          codeW = `
                for (var i: u32 = 0u; i < uniforms.kw; i++) {
                  xIndices[${dimIdxW}] = indices[${dimIdxW}] * uniforms.sw - uniforms.pwStart + i;
                  let x_val = x[${x.indicesToOffset("xIndices")}];
                  ${op1}
                }`;
        }
        if (attributes.kernelShape.length === 2) {
          const dimIdxH = rank - (isChannelsLast ? 3 : 2);
          if (phStartEndNotZero) {
            codeH = `
                for (var j: u32 = 0u; j < uniforms.kh; j++) {
                  xIndices[${dimIdxH}] = indices[${dimIdxH}] * uniforms.sh - uniforms.phStart + j;
                  if (xIndices[${dimIdxH}] < 0 || xIndices[${dimIdxH}] >= uniforms.x_shape[${dimIdxH}]) {
                    pad += i32(uniforms.kw);
                    continue;
                  }
              `;
          } else {
            codeH = `
                for (var j: u32 = 0u; j < uniforms.kh; j++) {
                  xIndices[${dimIdxH}] = indices[${dimIdxH}] * uniforms.sh - uniforms.phStart + j;
                `;
          }
          codeHEnd = `
              }
            `;
        }
        const poolingCode = `
            ${shaderHelper.registerUniforms(uniforms).declareVariables(x, output)}

            ${shaderHelper.mainStart()}
              ${shaderHelper.guardAgainstOutOfBoundsWorkgroupSizes("uniforms.outputSize")}

              let indices = ${output.offsetToIndices("global_idx")};
              var xIndices = ${output.offsetToIndices("global_idx")};

              var value = ${dataType}(${start});
              var pad = 0;
              ${codeH}
              ${codeW}
              ${codeHEnd}
              ${op2}

              output[global_idx] = value;
            }`;
        return poolingCode;
      } else {
        if (isChannelsLast) {
          throw new Error("Pooling with kernelShape.length > 2 is not supported for NHWC format.");
        }
        const stridesRank = attributes.kernelShape.length;
        const padsRank = attributes.pads.length;
        let padCode = "";
        if (hasPads) {
          padCode = `
                if (xIndices[j] >= uniforms.x_shape[j]) {
                  pad++;
                  isPad = true;
                  break;
                }
              }
              if (!isPad) {
                let x_val = x[${x.indicesToOffset("xIndices")}];
                ${op1}
              }`;
        } else {
          padCode = `
              }
              let x_val = x[${x.indicesToOffset("xIndices")}];
              ${op1}
            `;
        }
        const poolingCode = `
            ${shaderHelper.registerUniforms(uniforms).declareVariables(x, output)}

            ${shaderHelper.mainStart()}
              ${shaderHelper.guardAgainstOutOfBoundsWorkgroupSizes("uniforms.outputSize")}
              let indices = ${output.offsetToIndices("global_idx")};
              var xIndices = ${output.offsetToIndices("global_idx")};

              var offsets: array<u32, ${stridesRank}>;

              var value = ${dataType}(${start});
              var pad = 0;
              var isPad = false;

              for (var i: u32 = 0u; i < uniforms.kernelSize; i++) {
                var offset = i;
                for (var j = 0u; j < ${stridesRank - 1}u; j++) {
                  offsets[j] = offset / ${getElementAt("uniforms.kernelStrides", "j", stridesRank)};
                  offset -= offsets[j] * ${getElementAt("uniforms.kernelStrides", "j", stridesRank)};
                }
                offsets[${stridesRank - 1}] = offset;

                isPad = false;
                for (var j = ${rank - stridesRank}u; j < ${rank}u; j++) {
                  xIndices[j] = indices[j] * ${getElementAt(
          "uniforms.strides",
          `j - ${rank - stridesRank}u`,
          stridesRank
        )}
                    + offsets[j - ${rank - stridesRank}u] - ${getElementAt("uniforms.pads", "j - 2u", padsRank)};
                  ${padCode}
              }
              ${op2}

              output[global_idx] = value;
            }`;
        return poolingCode;
      }
    };
    createShaderKeyFromAttributes = (attributes) => `${attributes.format};${attributes.ceilMode};${attributes.autoPad};${attributes.kernelShape.length}`;
    createAveragePoolShaderKeyFromAttributes = (attributes) => `${createShaderKeyFromAttributes(attributes)};${attributes.countIncludePad}`;
    createMaxPoolShaderKeyFromAttributes = (attributes) => `${createShaderKeyFromAttributes(attributes)};${attributes.storageOrder};${attributes.dilations}`;
    parsePoolCommonAttributes = (attributes) => ({
      format: attributes.format,
      autoPad: ["NOTSET", "VALID", "SAME_UPPER", "SAME_LOWER"][attributes.auto_pad],
      ceilMode: attributes.ceil_mode,
      kernelShape: attributes.kernel_shape,
      strides: attributes.strides,
      pads: attributes.pads
    });
    createAveragePoolProgramInfo = (name, input, isGlobalOperator, attributes) => {
      const [adjustedAttributes, outputShape] = getAdjustedPoolAttributesAndOutputShape(
        input,
        attributes,
        isGlobalOperator
      );
      const x = inputVariable("x", input.dataType, input.dims.length);
      const dataType = x.type.value;
      const op1 = "value += x_val;";
      let op2 = "";
      if (adjustedAttributes.countIncludePad) {
        op2 += `value /= ${dataType}(uniforms.kernelSize);`;
      } else {
        op2 += `value /= ${dataType}(i32(uniforms.kernelSize) - pad);`;
      }
      const [programUniforms, uniforms, hasPads, pwStartEndNotZero, phStartEndNotZero] = getUniformAndPadInfo(
        outputShape,
        adjustedAttributes
      );
      programUniforms.push(...createTensorShapeVariables(input.dims, outputShape));
      const inputDependencies = ["rank"];
      return {
        name,
        shaderCache: {
          hint: `${attributes.cacheKey};${hasPads};${pwStartEndNotZero};${phStartEndNotZero}`,
          inputDependencies
        },
        getRunData: () => ({
          outputs: [{ dims: outputShape, dataType: input.dataType }],
          dispatchGroup: { x: Math.ceil(
            ShapeUtil.size(outputShape) / 64
            /* workgroup size */
          ) },
          programUniforms
        }),
        getShaderSource: (shaderHelper) => generatePoolingCode(
          shaderHelper,
          x,
          input.dims.length,
          outputShape.length,
          adjustedAttributes,
          op1,
          op2,
          0,
          uniforms,
          hasPads,
          pwStartEndNotZero,
          phStartEndNotZero
        )
      };
    };
    parseAveragePoolAttributes = (attributes) => {
      const countIncludePad = attributes.count_include_pad === 0 ? false : true;
      const attr = parsePoolCommonAttributes(attributes);
      if (attr.ceilMode !== 0) {
        throw new Error("using ceil() in shape computation is not yet supported for AveragePool");
      }
      const averagePoolAttributes = { countIncludePad, ...attr, cacheKey: "" };
      return { ...averagePoolAttributes, cacheKey: createAveragePoolShaderKeyFromAttributes(averagePoolAttributes) };
    };
    averagePool = (context, attributes) => {
      validateInputs23(context.inputs);
      context.compute(createAveragePoolProgramInfo("AveragePool", context.inputs[0], false, attributes));
    };
    globalPoolAttributes = {
      autoPad: "",
      ceilMode: 0,
      countIncludePad: false,
      kernelShape: [],
      strides: [],
      pads: [],
      storageOrder: 0,
      dilations: []
    };
    parseGlobalAveragePoolAttributes = (attributes) => {
      const format = attributes.format;
      return { format, ...globalPoolAttributes, cacheKey: format };
    };
    globalAveragePool = (context, attributes) => {
      validateInputs23(context.inputs);
      context.compute(createAveragePoolProgramInfo("GlobalAveragePool", context.inputs[0], true, attributes));
    };
    createMaxPoolProgramInfo = (name, input, isGlobalOperator, attributes) => {
      const [adjustedAttributes, outputShape] = getAdjustedPoolAttributesAndOutputShape(
        input,
        attributes,
        isGlobalOperator
      );
      const op1 = `
      value = max(x_val, value);
    `;
      const op2 = "";
      const x = inputVariable("x", input.dataType, input.dims.length);
      const inputDependencies = ["rank"];
      const [programUniforms, uniforms, hasPads, pwStartEndNotZero, phStartEndNotZero] = getUniformAndPadInfo(
        outputShape,
        adjustedAttributes
      );
      programUniforms.push(...createTensorShapeVariables(input.dims, outputShape));
      return {
        name,
        shaderCache: {
          hint: `${attributes.cacheKey};${hasPads};${pwStartEndNotZero};${phStartEndNotZero}`,
          inputDependencies
        },
        getRunData: () => ({
          outputs: [{ dims: outputShape, dataType: input.dataType }],
          dispatchGroup: { x: Math.ceil(
            ShapeUtil.size(outputShape) / 64
            /* workgroup size */
          ) },
          programUniforms
        }),
        getShaderSource: (shaderHelper) => generatePoolingCode(
          shaderHelper,
          x,
          input.dims.length,
          outputShape.length,
          adjustedAttributes,
          op1,
          op2,
          input.dataType === 10 /* float16 */ ? -65504 : -1e5,
          uniforms,
          hasPads,
          pwStartEndNotZero,
          phStartEndNotZero
        )
      };
    };
    maxPool = (context, attributes) => {
      validateInputs23(context.inputs);
      context.compute(createMaxPoolProgramInfo("MaxPool", context.inputs[0], false, attributes));
    };
    parseMaxPoolAttributes = (attributes) => {
      const storageOrder = attributes.storage_order;
      const dilations = attributes.dilations;
      const attr = parsePoolCommonAttributes(attributes);
      if (storageOrder !== 0) {
        throw new Error("column major storage order is not yet supported for MaxPool");
      }
      if (attr.ceilMode !== 0) {
        throw new Error("using ceil() in shape computation is not yet supported for MaxPool");
      }
      const maxPoolAttributes = { storageOrder, dilations, ...attr, cacheKey: "" };
      return { ...maxPoolAttributes, cacheKey: createMaxPoolShaderKeyFromAttributes(maxPoolAttributes) };
    };
    parseGlobalMaxPoolAttributes = (attributes) => {
      const format = attributes.format;
      return { format, ...globalPoolAttributes, cacheKey: format };
    };
    globalMaxPool = (context, attributes) => {
      validateInputs23(context.inputs);
      context.compute(createMaxPoolProgramInfo("GlobalMaxPool", context.inputs[0], true, attributes));
    };
  }
});

// web/lib/wasm/jsep/webgpu/ops/quantize-linear.ts
var validateInputs24, createDequantizeLinearProgramInfo, dequantizeLinear, parseDequantizeLinearAttributes;
var init_quantize_linear = __esm({
  "web/lib/wasm/jsep/webgpu/ops/quantize-linear.ts"() {
    "use strict";
    init_wasm_common();
    init_util();
    init_attribute_with_cache_key();
    init_common();
    validateInputs24 = (inputs, attributes) => {
      if (inputs.length < 2 || inputs.length > 3) {
        throw new Error("DequantizeLinear requires 2 or 3 inputs.");
      }
      if (inputs.length === 3 && inputs[1].dims === inputs[2].dims) {
        throw new Error("x-scale and x-zero-point must have the same shape.");
      }
      if (inputs.length === 3 && inputs[0].dataType !== inputs[2].dataType) {
        throw new Error("x and x-zero-point must have the same data type.");
      }
      if (inputs[0].dataType === 6 /* int32 */ && inputs.length > 2) {
        throw new Error("In the case of dequantizing int32 there is no zero point.");
      }
      if (inputs[1].dims.length !== 0 && inputs[1].dims.length !== 1 && inputs[1].dims.length !== inputs[0].dims.length) {
        throw new Error("scale input must be a scalar, a 1D tensor, or have the same rank as the input tensor.");
      }
      if (inputs.length > 2) {
        if (inputs[0].dataType !== inputs[2].dataType) {
          throw new Error("x and x-zero-point must have the same data type.");
        }
        if (inputs[1].dims.length !== inputs[2].dims.length) {
          throw new Error("scale and zero-point inputs must have the same rank.");
        }
        if (!inputs[1].dims.map((d, i) => d === inputs[2].dims[i]).reduce((a, b) => a && b, true)) {
          throw new Error("scale and zero-point inputs must have the same shape.");
        }
      }
      if (attributes.blockSize > 0) {
        if (inputs[1].dims.length === 0 || inputs[1].dims.length === 1 && inputs[1].dims[0] === 1) {
          throw new Error("blockSize must be set only for block quantization.");
        }
        if (!inputs[1].dims.map((d, i) => i === attributes.axis || d === inputs[0].dims[i]).reduce((a, b) => a && b, true)) {
          throw new Error("For block qunatization, scale input shape to match the input shape except for the axis");
        }
        if (inputs[1].dims.length !== inputs[0].dims.length) {
          throw new Error("For block qunatization the scale input rank must be the same as the x rank.");
        }
        const dI = inputs[0].dims[attributes.axis];
        const si = inputs[1].dims[attributes.axis];
        if (attributes.blockSize < Math.ceil(dI / si) || attributes.blockSize > Math.ceil(dI / (si - 1) - 1)) {
          throw new Error("blockSize must be with in the range [ceil(dI / Si), ceil(dI / (Si - 1) - 1)].");
        }
      }
    };
    createDequantizeLinearProgramInfo = (inputs, attributes) => {
      const axis = ShapeUtil.normalizeAxis(attributes.axis, inputs[0].dims.length);
      const inputType = inputs[0].dataType;
      const isSigned = inputType === 3 /* int8 */;
      const outputShape = inputs[0].dims;
      const dataType = inputs[1].dataType;
      const outputSize = ShapeUtil.size(outputShape);
      const isPacked = inputType === 3 /* int8 */ || inputType === 2 /* uint8 */;
      const inputShape = isPacked ? [Math.ceil(ShapeUtil.size(inputs[0].dims) / 4)] : inputs[0].dims;
      const scaleShape = inputs[1].dims;
      const zeroPointInput = inputs.length > 2 ? inputs[2] : void 0;
      const zeroPointShape = zeroPointInput ? isPacked ? [Math.ceil(ShapeUtil.size(zeroPointInput.dims) / 4)] : zeroPointInput.dims : void 0;
      const perLayerQuantization = scaleShape.length === 0 || scaleShape.length === 1 && scaleShape[0] === 1;
      const perAxisQuantization = perLayerQuantization === false && scaleShape.length === 1;
      const maxComponents = getMaxComponents(outputSize);
      const useComponents = perLayerQuantization && (!isPacked || maxComponents === 4);
      const components = useComponents ? maxComponents : 1;
      const inputComponent = useComponents && !isPacked ? maxComponents : 1;
      const input = inputVariable("input", isPacked ? 12 /* uint32 */ : inputType, inputShape.length, inputComponent);
      const scale = inputVariable("scale", dataType, scaleShape.length);
      const zeroPoint = zeroPointInput ? inputVariable("zero_point", isPacked ? 12 /* uint32 */ : inputType, zeroPointShape.length) : void 0;
      const output = outputVariable("output", dataType, outputShape.length, components);
      const inputVariables = [input, scale];
      if (zeroPoint) {
        inputVariables.push(zeroPoint);
      }
      const inputShapes = [inputShape, scaleShape];
      if (zeroPointInput) {
        inputShapes.push(zeroPointShape);
      }
      const programUniforms = [
        { type: 12 /* uint32 */, data: outputSize / components },
        { type: 12 /* uint32 */, data: axis },
        { type: 12 /* uint32 */, data: attributes.blockSize },
        ...createTensorShapeVariables(...inputShapes, outputShape)
      ];
      const getShaderSource = (shaderHelper) => {
        const uniforms = [
          { name: "output_size", type: "u32" },
          { name: "axis", type: "u32" },
          { name: "block_size", type: "u32" }
        ];
        return `
      ${shaderHelper.registerUniforms(uniforms).declareVariables(...inputVariables, output)}
      ${shaderHelper.mainStart()}
          ${shaderHelper.guardAgainstOutOfBoundsWorkgroupSizes("uniforms.output_size")}
          let output_indices = ${output.offsetToIndices("global_idx")};

          // Set input x
          ${(() => {
          if (isPacked) {
            return `
            let input = ${input.getByOffset("global_idx / 4")};
            let x_vec = ${isSigned ? "unpack4xI8(input)" : "unpack4xU8(input)"};
            let x_value = ${components === 1 ? "x_vec[global_idx % 4]" : "x_vec"};`;
          } else {
            return `let x_value = ${input.getByOffset("global_idx")};`;
          }
        })()};

          // Set scale input
          ${(() => {
          if (perLayerQuantization) {
            return `let scale_value= ${scale.getByOffset("0")}`;
          } else if (perAxisQuantization) {
            return `
            let scale_index = ${output.indicesGet("output_indices", "uniforms.axis")};
            let scale_value= ${scale.getByOffset("scale_index")};`;
          } else {
            return `
            var scale_indices: ${scale.type.indices} = output_indices;
            let index = ${scale.indicesGet("scale_indices", "uniforms.axis")} / uniforms.block_size;
            ${scale.indicesSet("scale_indices", "uniforms.axis", "index")};
            let scale_value= ${scale.getByIndices("scale_indices")};`;
          }
        })()};

          // Set zero-point input
          ${(() => {
          if (zeroPoint) {
            if (perLayerQuantization) {
              if (isPacked) {
                return `
                let zero_point_input = ${zeroPoint.getByOffset("0")};
                let zero_point_vec =  ${isSigned ? "unpack4xI8(zero_point_input)" : "unpack4xU8(zero_point_input)"};
                let zero_point_value= zero_point_vec[0]`;
              } else {
                return `let zero_point_value = ${zeroPoint.getByOffset("0")}`;
              }
            } else if (perAxisQuantization) {
              if (isPacked) {
                return `
                let zero_point_index = ${output.indicesGet("output_indices", "uniforms.axis")};
                let zero_point_input = ${zeroPoint.getByOffset("zero_point_index / 4")};
                let zero_point_vec =  ${isSigned ? "unpack4xI8(zero_point_input)" : "unpack4xU8(zero_point_input)"};
                let zero_point_value = zero_point_vec[zero_point_index % 4]`;
              } else {
                return `
                let zero_point_index = ${output.indicesGet("output_indices", "uniforms.axis")};
                let zero_point_value = ${zeroPoint.getByOffset("zero_point_index")};`;
              }
            } else {
              if (isPacked) {
                return `
                let zero_point_offset = ${scale.indicesToOffset("scale_indices")};
                let zero_point_input = ${zeroPoint.getByOffset("zero_point_offset / 4")};
                let zero_point_vec = ${isSigned ? "unpack4xI8(zero_point_input)" : "unpack4xU8(zero_point_input)"};
                let zero_point_value = zero_point_vec[zero_point_offset % 4];`;
              } else {
                return `let zero_point_value = ${zeroPoint.getByIndices("scale_indices")};`;
              }
            }
          } else {
            return `let zero_point_value = ${isPacked ? isSigned ? "i32" : "u32" : input.type.value}(0);`;
          }
        })()};
      // Compute and write output
      ${output.setByOffset("global_idx", `${output.type.value}(x_value - zero_point_value) * scale_value`)};
      }`;
      };
      return {
        name: "DequantizeLinear",
        shaderCache: {
          hint: attributes.cacheKey,
          inputDependencies: zeroPoint ? ["rank", "rank", "rank"] : ["rank", "rank"]
        },
        getShaderSource,
        getRunData: () => ({
          outputs: [{ dims: outputShape, dataType }],
          dispatchGroup: { x: Math.ceil(outputSize / components / 64), y: 1, z: 1 },
          programUniforms
        })
      };
    };
    dequantizeLinear = (context, attributes) => {
      validateInputs24(context.inputs, attributes);
      context.compute(createDequantizeLinearProgramInfo(context.inputs, attributes));
    };
    parseDequantizeLinearAttributes = (attributes) => createAttributeWithCacheKey({ axis: attributes.axis, blockSize: attributes.blockSize });
  }
});

// web/lib/wasm/jsep/webgpu/ops/range.ts
var validateInputsContent, createRangeProgramInfo, range;
var init_range = __esm({
  "web/lib/wasm/jsep/webgpu/ops/range.ts"() {
    "use strict";
    init_esm();
    init_wasm_common();
    init_common();
    validateInputsContent = (start, limit, delta) => {
      const sameStartLimit = start === limit;
      const increasingRangeNegativeStep = start < limit && delta < 0;
      const decreasingRangePositiveStep = start > limit && delta > 0;
      if (sameStartLimit || increasingRangeNegativeStep || decreasingRangePositiveStep) {
        throw new Error("Range these inputs' contents are invalid.");
      }
    };
    createRangeProgramInfo = (start, limit, delta, dataType) => {
      const numElements = Math.abs(Math.ceil((limit - start) / delta));
      const outputShape = [numElements];
      const outputSize = numElements;
      const programUniforms = [
        { type: 12 /* uint32 */, data: outputSize },
        { type: dataType, data: start },
        { type: dataType, data: delta },
        ...createTensorShapeVariables(outputShape)
      ];
      const getShaderSource = (shaderHelper) => {
        const output = outputVariable("output", dataType, outputShape.length);
        const wgslType = output.type.value;
        const uniforms = [
          { name: "outputSize", type: "u32" },
          { name: "start", type: wgslType },
          { name: "delta", type: wgslType }
        ];
        return `
        ${shaderHelper.registerUniforms(uniforms).declareVariables(output)}
        ${shaderHelper.mainStart()}
        ${shaderHelper.guardAgainstOutOfBoundsWorkgroupSizes("uniforms.outputSize")}
        output[global_idx] = uniforms.start + ${wgslType}(global_idx) * uniforms.delta;
      }`;
      };
      return {
        name: "Range",
        shaderCache: { hint: `${dataType}` },
        getShaderSource,
        getRunData: () => ({
          outputs: [{ dims: outputShape, dataType }],
          dispatchGroup: { x: Math.ceil(
            outputSize / 64
            /* workgroup size */
          ) },
          programUniforms
        })
      };
    };
    range = (context) => {
      let start = 0;
      let limit = 0;
      let delta = 0;
      if (context.inputs[0].dataType === 6 /* int32 */) {
        start = context.inputs[0].getInt32Array()[0];
        limit = context.inputs[1].getInt32Array()[0];
        delta = context.inputs[2].getInt32Array()[0];
      } else if (context.inputs[0].dataType === 1 /* float */) {
        start = context.inputs[0].getFloat32Array()[0];
        limit = context.inputs[1].getFloat32Array()[0];
        delta = context.inputs[2].getFloat32Array()[0];
      }
      if (env2.webgpu.validateInputContent) {
        validateInputsContent(start, limit, delta);
      }
      context.compute(createRangeProgramInfo(start, limit, delta, context.inputs[0].dataType), { inputs: [] });
    };
  }
});

// web/lib/wasm/jsep/webgpu/ops/resize.ts
var validateScales, updateScales, validateInputs25, getOriginalCoordinateFromResizedCoordinate, getNearestPixelFromOriginal, updateRoI, initOutputShape, adjustOutputShape, calculateOriginalIndicesFromOutputIndices, calculateInputIndicesFromOutputIndices, checkInputIndices, setChannelAndBatchIndices, bilinearInterpolation, bicubicInterpolation, trilinearInterpolation, createResizeProgramInfo, getOpsetVersionFromCustomDataBuffer, resize, parseResizeAttributes;
var init_resize = __esm({
  "web/lib/wasm/jsep/webgpu/ops/resize.ts"() {
    "use strict";
    init_wasm_common();
    init_util();
    init_attribute_with_cache_key();
    init_common();
    validateScales = (scales, attributes) => {
      scales.every(
        (value) => value > 0 || (() => {
          throw new Error("Resize requires scales input values to be positive");
        })
      );
      if (scales.length > 0) {
        if (attributes.mode === "linear") {
          if (!(scales.length === 2 || scales.length === 3 || scales.length === 4 && scales[0] === 1 && scales[1] === 1 || scales.length === 4 && scales[0] === 1 && scales[3] === 1 || scales.length === 5 && scales[0] === 1 && scales[1] === 1)) {
            throw new Error(
              `For linear mode, Resize requires scales to be 2D, 3D, 4D with either two outermost or one innermost and
            one outermost scale values equal to 1, or 5D with two outermost scale values equal to 1`
            );
          }
        } else if (attributes.mode === "cubic") {
          if (!(scales.length === 2 || scales.length === 4 && scales[0] === 1 && scales[1] === 1 || scales.length === 4 && scales[0] === 1 && scales[3] === 1)) {
            throw new Error("Resize requires scales input size to be 2 or 4 for cubic mode");
          }
        }
      }
    };
    updateScales = (scales, axes, rank) => {
      axes.every(
        (value) => value >= 0 && value < rank || (() => {
          throw new Error("Resize requires axes input values to be positive and less than rank");
        })
      );
      const newScales = new Array(rank).fill(1);
      axes.forEach((value, index) => newScales[value] = scales[index]);
      return newScales;
    };
    validateInputs25 = (inputs, attributes, opsetVersion, scales, sizes, roi) => {
      const [roiInputIndex, scalesInputIndex, sizesInputIndex] = opsetVersion > 10 ? [1, 2, 3] : [-1, inputs.length > 1 ? 1 : -1, -1];
      const rank = inputs[0].dims.length;
      if (roiInputIndex > 0 && inputs.length > roiInputIndex && inputs[roiInputIndex].dims.length > 0) {
        inputs[roiInputIndex].getFloat32Array().forEach((value) => roi.push(value));
      } else if (attributes.coordinateTransformMode === "tf_crop_and_resize") {
        throw new Error("Resize requires RoI input to be specified when coordinateTransformMode is tfCropAndResize");
      }
      if (scalesInputIndex > 0 && inputs.length > scalesInputIndex && inputs[scalesInputIndex].dims.length > 0) {
        inputs[scalesInputIndex].getFloat32Array().forEach((value) => scales.push(value));
        if (scales.length !== 0 && scales.length !== rank && opsetVersion >= 18 && scales.length !== attributes.axes.length) {
          throw new Error("Resize requires scales input size to be same as input rank or axes size for opset 18 and up");
        }
        validateScales(scales, attributes);
        if (attributes.axes.length > 0) {
          updateScales(scales, attributes.axes, rank).forEach((value, index) => scales[index] = value);
        }
      }
      if (sizesInputIndex > 0 && inputs.length > sizesInputIndex) {
        inputs[sizesInputIndex].getBigInt64Array().forEach((value) => sizes.push(Number(value)));
        if (sizes.length !== rank || opsetVersion >= 18 && sizes.length === attributes.axes.length) {
          throw new Error("Resize requires sizes input size to be same as input rank or axes size for opset 18 and up");
        }
      }
      if (attributes.axes.length > 0) {
        if (scales.length !== attributes.axes.length) {
          throw new Error('Resize requires "scales" input size to be of axes rank when axes attributes is specified');
        }
        if (sizes.length !== attributes.axes.length) {
          throw new Error('Resize requires "sizes" input size to be of rank axes rank when axes attributes is specified');
        }
      }
      if (typeof scales !== "undefined" && typeof sizes !== "undefined" && scales.length > 0 && sizes.length > rank) {
        throw new Error("Resize requires only of scales or sizes to be specified");
      }
    };
    getOriginalCoordinateFromResizedCoordinate = (coordinateTransferMode, dType) => `fn getOriginalCoordinateFromResizedCoordinate(xResized: u32, xScale: f32, lengthResized: u32,
     lengthOriginal: u32, roiStart: f32, roiEnd: f32) -> ${dType} { ` + (() => {
      switch (coordinateTransferMode) {
        case "asymmetric":
          return `return ${dType}(xResized) / ${dType}(xScale);`;
        case "pytorch_half_pixel":
          return `if (lengthResized > 1) {
                    return (${dType}(xResized) + 0.5) / ${dType}(xScale) - 0.5;
                  } else {
                    return 0.0;
                  }`;
        case "tf_half_pixel_for_nn":
          return `return (${dType}(xResized) + 0.5) / ${dType}(xScale);`;
        case "align_corners":
          return `if (lengthResized == 1) {
                    return 0.0;
                  } else {
                    // The whole part and the fractional part are calculated separately due to inaccuracy of floating
                    // point division. As an example, f32(21) / f32(7) may evaluate to 2.99... instead of 3, causing an
                    // offset-by-one error later in floor().
                    let whole = ${dType}(xResized * (lengthOriginal - 1) / (lengthResized - 1));
                    let fract =
                        ${dType}(xResized * (lengthOriginal - 1) % (lengthResized - 1)) / ${dType}(lengthResized - 1);
                    return whole + fract;
                  }`;
        case "tf_crop_and_resize":
          return `if (lengthResized > 1) {
                    return ${dType}(roiStart) * ${dType}(lengthOriginal - 1) +
                        (${dType}(xResized) * ${dType}(roiEnd - roiStart) * ${dType}(lengthOriginal - 1)) /
                        ${dType}(lengthResized - 1);
                  } else {
                    return 0.5 * ${dType}(roiStart + roiEnd) * ${dType}(lengthOriginal - 1);
                  }`;
        case "half_pixel_symmetric":
          return `const outputWidth = ${dType}xScale * ${dType}(lengthResized);
                  const adjustment = ${dType}(lengthResized) / outputWidth;
                  const center = ${dType}(lengthOriginal) / 2;
                  const offset = center * (1 - adjustment);
                  return offset + ((${dType}(xResized) + 0.5) / ${dType}(xScale)) - 0.5;`;
        case "half_pixel":
          return `return ((${dType}(xResized) + 0.5) / ${dType}(xScale)) - 0.5;`;
        default:
          throw new Error(`Coordinate transform mode ${coordinateTransferMode} is not supported`);
      }
    })() + "}";
    getNearestPixelFromOriginal = (nearestMode, opsetVersion, dType) => `fn getNearestPixelFromOriginal(xOriginal: ${dType}, isDownSample: bool) -> ${dType} {` + (() => {
      switch (nearestMode) {
        case "round_prefer_ceil":
          return "if (fract(xOriginal) == 0.5) {             return ceil(xOriginal);           } else {             return round(xOriginal);           }";
        case "floor":
          return "return floor(xOriginal);";
        case "ceil":
          return "return ceil(xOriginal);";
        case "round_prefer_floor":
          return "if (fract(xOriginal) == 0.5) {                     return floor(xOriginal);                   } else {                     return round(xOriginal);                   }";
        case "simple":
        default:
          if (opsetVersion < 11) {
            return "if (isDownSample)                     {                       return ceil(xOriginal);                     } else {                       return xOriginal;                     }";
          }
          throw new Error(`Nearest mode ${nearestMode} is not supported`);
      }
    })() + "}";
    updateRoI = (roi, axes, rank) => {
      const roiTmp = new Array(rank).fill(0).concat(new Array(rank).fill(1));
      const roiLocal = roi.length === 0 ? roiTmp : roi.slice();
      if (axes.length > 0) {
        axes.forEach((v, i) => {
          roiTmp[v] = roiLocal[i];
          roiTmp[i + rank] = roiLocal[axes.length + i];
        });
        return roiTmp;
      }
      return roiLocal;
    };
    initOutputShape = (inputShape, scales, sizes, axes) => {
      let outputShape = [];
      if (sizes.length > 0) {
        if (axes.length > 0) {
          inputShape.forEach((v) => outputShape.push(v));
          if (Math.max(...axes) > inputShape.length) {
            throw new Error("axes is out of bound");
          }
          axes.forEach((v, i) => outputShape[v] = sizes[i]);
        } else {
          sizes.forEach((v) => outputShape.push(v));
        }
      } else {
        if (scales.length === 0) {
          throw new Error("Resize requires either scales or sizes.");
        } else {
          outputShape = inputShape.map((value, index) => Math.round(value * scales[index]));
        }
      }
      return outputShape;
    };
    adjustOutputShape = (inputShape, scales, attributes) => {
      const scaleInPolicy = (() => {
        switch (attributes.keepAspectRatioPolicy) {
          case "not_larger":
            return attributes.axes.length > 0 ? Math.min(...attributes.axes.map((i) => scales[i]), Number.MAX_VALUE) : Math.min(...scales, Number.MAX_VALUE);
          case "not_smaller":
            return attributes.axes.length > 0 ? Math.max(...attributes.axes.map((i) => scales[i]), Number.MIN_VALUE) : Math.max(...scales, Number.MIN_VALUE);
          default:
            throw new Error(`Keep aspect ratio policy ${attributes.keepAspectRatioPolicy} is not supported`);
        }
      })();
      scales.fill(1, 0, scales.length);
      const adjustedOutputShape = inputShape.slice();
      if (attributes.axes.length > 0) {
        attributes.axes.forEach((v) => scales[v] = scaleInPolicy);
        attributes.axes.forEach((v) => adjustedOutputShape[v] = Math.round(inputShape[v] * scales[v]));
      } else {
        scales.fill(scaleInPolicy, 0, scales.length);
        adjustedOutputShape.forEach((v, i) => adjustedOutputShape[i] = Math.round(v * scales[i]));
      }
      return adjustedOutputShape;
    };
    calculateOriginalIndicesFromOutputIndices = (output, inputShape, outputShape, scalesLength, roiLength) => `
    fn calculateOriginalIndicesFromOutputIndices(output_indices: ${output.type.indices}) -> array<${output.type.value}, ${outputShape.length}> {
      var original_indices: array<${output.type.value}, ${outputShape.length}>;
      for (var i:u32 = 0; i < ${outputShape.length}; i++) {
        var output_index = ${output.indicesGet("output_indices", "i")};
        var scale = ${getElementAt("uniforms.scales", "i", scalesLength)};
        var roi_low = ${getElementAt("uniforms.roi", "i", roiLength)};
        var roi_hi = ${getElementAt("uniforms.roi", `i + ${inputShape.length}`, roiLength)};
        if (scale == 1.0) {
          original_indices[i] = ${output.type.value}(output_index);
        } else {
          var input_shape_i = ${getElementAt("uniforms.input_shape", "i", inputShape.length)};
          var output_shape_i = ${getElementAt("uniforms.output_shape", "i", outputShape.length)};
          original_indices[i] = getOriginalCoordinateFromResizedCoordinate(output_index, scale, output_shape_i,
                                                                           input_shape_i, roi_low, roi_hi);
        }
      }
      return original_indices;
    }`;
    calculateInputIndicesFromOutputIndices = (input, output, inputShape, outputShape, scalesLength, roiLength, useExtrapolation) => `
    fn calculateInputIndicesFromOutputIndices(output_indices: ${output.type.indices}) -> ${input.type.indices} {
      var input_indices: ${input.type.indices};
      for (var i:u32 = 0; i < ${outputShape.length}; i++) {
        var output_index = ${output.indicesGet("output_indices", "i")};
        var input_index: u32;
        var scale = ${getElementAt("uniforms.scales", "i", scalesLength)};
        if (scale == 1.0) {
          input_index = output_index;
        } else {
          var roi_low = ${getElementAt("uniforms.roi", "i", roiLength)};
          var roi_hi = ${getElementAt("uniforms.roi", `i + ${inputShape.length}`, roiLength)};
          var input_shape_i = ${getElementAt("uniforms.input_shape", "i", inputShape.length)};
          var output_shape_i = ${getElementAt("uniforms.output_shape", "i", outputShape.length)};
          var original_idx = getOriginalCoordinateFromResizedCoordinate(output_index, scale, output_shape_i,
                                                                        input_shape_i, roi_low, roi_hi);
          if (!${useExtrapolation} || (original_idx >= 0 && original_idx < ${output.type.value}(input_shape_i))) {
            if (original_idx < 0) {
              input_index = 0;
            } else if (original_idx > ${output.type.value}(input_shape_i - 1)) {
              input_index = input_shape_i - 1;
            } else {
              input_index = u32(getNearestPixelFromOriginal(original_idx, scale < 1));
            }
          } else {
            input_index = u32(original_idx);
          }
        }
        ${input.indicesSet("input_indices", "i", " input_index")}
      }
      return input_indices;
    }`;
    checkInputIndices = (input, inputShape) => `
    fn checkInputIndices(input_indices: ${input.type.indices}) -> bool {
      for (var i:u32 = 0; i < ${inputShape.length}; i++) {
        var input_index = ${input.indicesGet("input_indices", "i")};
        if (input_index < 0 || input_index >= ${getElementAt("uniforms.input_shape", "i", inputShape.length)}) {
          return false;
        }
      }
      return true;
    }`;
    setChannelAndBatchIndices = (input, channelIdx, batchIdx, spacialDims) => input.rank > spacialDims ? `
    ${input.indicesSet("input_indices", channelIdx, "channel")};
    ${input.indicesSet("input_indices", batchIdx, "batch")};
` : "";
    bilinearInterpolation = (input, output, inputShape, useExtrapolation, extrapolationValue) => {
      const isNchw = true;
      const [batchIdx, heightIdx, widthIdx, channelIdx] = inputShape.length === 2 ? [-1, 0, 1, -1] : isNchw ? [0, 2, 3, 1] : [0, 1, 2, 3];
      const dType = input.type.value;
      return `
    fn getInputValue(batch: u32, channel: u32, row: u32, col: u32) -> ${dType} {
      var input_indices: ${input.type.indices};
      ${input.indicesSet("input_indices", heightIdx, `max(0, min(row, ${inputShape[heightIdx]} - 1))`)};
      ${input.indicesSet("input_indices", widthIdx, `max(0, min(col, ${inputShape[widthIdx]} - 1))`)};
      ${setChannelAndBatchIndices(input, channelIdx, batchIdx, 2)}
      return ${input.getByIndices("input_indices")};
    }

    fn bilinearInterpolation(output_indices: ${output.type.indices}) -> ${dType} {
      var originalIndices = calculateOriginalIndicesFromOutputIndices(output_indices);
      var row:${dType} = originalIndices[${heightIdx}];
      var col:${dType} = originalIndices[${widthIdx}];
      ${useExtrapolation ? `if (row < 0 || row > (${inputShape[heightIdx]} - 1) || col < 0 || col > (${inputShape[widthIdx]} - 1)) {
        return ${extrapolationValue};
      }` : ""};
      row = max(0, min(row, ${inputShape[heightIdx]} - 1));
      col = max(0, min(col, ${inputShape[widthIdx]} - 1));
      var row1: u32 = u32(row);
      var col1: u32 = u32(col);
      var row2: u32 = u32(row + 1);
      var col2: u32 = u32(col + 1);
      var channel: u32 = ${inputShape.length > 2 ? `u32(originalIndices[${channelIdx}])` : "0"};
      var batch: u32 =  ${inputShape.length > 2 ? `u32(originalIndices[${batchIdx}])` : "0"};
      var x11: ${dType} = getInputValue(batch, channel, row1, col1);
      var x12: ${dType} = getInputValue(batch, channel, row1, col2);
      var x21: ${dType} = getInputValue(batch, channel, row2, col1);
      var x22: ${dType} = getInputValue(batch, channel, row2, col2);
      var dx1: ${dType} = abs(row - ${dType}(row1));
      var dx2: ${dType} = abs(${dType}(row2) - row);
      var dy1: ${dType} = abs(col - ${dType}(col1));
      var dy2: ${dType} = abs(${dType}(col2) - col);
      if (row1 == row2) {
        dx1 = 0.5;
        dx2 = 0.5;
      }
      if (col1 == col2) {
        dy1 = 0.5;
        dy2 = 0.5;
      }
      return (x11 * dx2 * dy2 + x12 * dx2 * dy1 + x21 * dx1 * dy2 + x22 * dx1 * dy1);
    }`;
    };
    bicubicInterpolation = (input, output, inputShape, outputShape, scales, roi, cubicCoeffA, useExtrapolation, extrapolationValue, excludeOutside) => {
      const is2D = inputShape.length === 2;
      const isNchw = true;
      const [heightIdx, widthIdx] = is2D ? [0, 1] : isNchw ? [2, 3] : [1, 2];
      const dType = input.type.value;
      const createCubicInterpolationFunction = (idx) => {
        const direction = idx === heightIdx ? "row" : "col";
        return `
      fn ${direction}CubicInterpolation(input_indices: ${input.type.indices}, output_indices: ${output.type.indices}) -> ${dType} {
        var output_index = ${output.indicesGet("output_indices", idx)};
        var originalIdx: ${dType} = getOriginalCoordinateFromResizedCoordinate(output_index, ${scales[idx]},
        ${outputShape[idx]}, ${inputShape[idx]}, ${roi[idx]}, ${roi[idx]} + ${inputShape.length});
        var fractOriginalIdx: ${dType} = originalIdx - floor(originalIdx);
        var coefs = getCubicInterpolationCoefs(fractOriginalIdx);

        if (${useExtrapolation} && (originalIdx < 0 || originalIdx > (${inputShape[idx]} - 1))) {
          return ${extrapolationValue};
        }
        var data: array<${dType}, 4> = array<${dType}, 4>(0.0, 0.0, 0.0, 0.0);
        for (var i: i32 = -1; i < 3; i++) {
          var ${direction}: ${dType} = originalIdx + ${dType}(i);
          if (${direction} < 0 || ${direction} >= ${inputShape[idx]}) {
            ${(() => {
          if (excludeOutside) {
            return `coefs[i + 1] = 0.0;
                        continue;`;
          } else if (useExtrapolation) {
            return `return ${extrapolationValue};`;
          } else {
            return `${direction} = max(0, min(${direction}, ${inputShape[idx]} - 1));`;
          }
        })()};
          }
        var input_indices_copy: ${input.type.indices} = input_indices;
          ${input.indicesSet("input_indices_copy", idx, `u32(${direction})`)};
          data[i + 1] = ${idx === heightIdx ? input.getByIndices("input_indices_copy") : "rowCubicInterpolation(input_indices_copy, output_indices)"};
        }
        return cubicInterpolation1D(data, coefs);
      }`;
      };
      return `
    ${createCubicInterpolationFunction(heightIdx)};
    ${createCubicInterpolationFunction(widthIdx)};
  fn getCubicInterpolationCoefs(s: ${dType}) -> array<${dType}, 4> {
    var absS = abs(s);
    var coeffs: array<${dType}, 4> = array<${dType}, 4>(0.0, 0.0, 0.0, 0.0);
    var oneMinusAbsS: ${dType} = 1.0 - absS;
    var twoMinusAbsS: ${dType} = 2.0 - absS;
    var onePlusAbsS: ${dType} = 1.0 + absS;
    coeffs[0] = ((${cubicCoeffA} * onePlusAbsS - 5 * ${cubicCoeffA}) * onePlusAbsS + 8 * ${cubicCoeffA}) * onePlusAbsS - 4 * ${cubicCoeffA};
    coeffs[1] = ((${cubicCoeffA} + 2) * absS - (${cubicCoeffA} + 3)) * absS * absS + 1;
    coeffs[2] = ((${cubicCoeffA} + 2) * oneMinusAbsS - (${cubicCoeffA} + 3)) * oneMinusAbsS * oneMinusAbsS + 1;
    coeffs[3] = ((${cubicCoeffA} * twoMinusAbsS - 5 * ${cubicCoeffA}) * twoMinusAbsS + 8 * ${cubicCoeffA}) * twoMinusAbsS - 4 * ${cubicCoeffA};
    return coeffs;
  }

  fn cubicInterpolation1D(x: array<${dType}, 4>, coefs: array<${dType}, 4>) -> ${dType} {
    var coefsSum: ${dType} = coefs[0] + coefs[1] + coefs[2] + coefs[3];
    return (x[0] * coefs[0] + x[1] * coefs[1]+ x[2] * coefs[2]+ x[3] * coefs[3]) / coefsSum;
  }

  fn bicubicInterpolation(output_indices: ${output.type.indices}) -> ${dType} {
    var input_indices: ${input.type.indices} = output_indices;
    return colCubicInterpolation(input_indices, output_indices);
  }
    `;
    };
    trilinearInterpolation = (input, output, inputShape, useExtrapolation, extrapolationValue) => {
      const isNchw = true;
      const [batchIdx, depthIdx, heightIdx, widthIdx, channelIdx] = inputShape.length === 3 ? [-1, 0, 1, 2, -1] : isNchw ? [0, 2, 3, 4, 1] : [0, 1, 2, 3, 4];
      const dType = input.type.value;
      return `
    fn getInputValue(batch: u32, channel: u32, depth:u32, height: u32, width: u32) -> ${dType} {
      var input_indices: ${input.type.indices};
      ${input.indicesSet("input_indices", depthIdx, `max(0, min(depth, ${inputShape[depthIdx]} - 1))`)};
      ${input.indicesSet("input_indices", heightIdx, `max(0, min(height, ${inputShape[heightIdx]} - 1))`)};
      ${input.indicesSet("input_indices", widthIdx, `max(0, min(width, ${inputShape[widthIdx]} - 1))`)};
      ${setChannelAndBatchIndices(input, channelIdx, batchIdx, 3)}
      return ${input.getByIndices("input_indices")};
    }

    fn trilinearInterpolation(output_indices: ${output.type.indices}) -> ${dType} {
      var originalIndices = calculateOriginalIndicesFromOutputIndices(output_indices);
      var depth:${dType} = originalIndices[${depthIdx}];
      var height:${dType} = originalIndices[${heightIdx}];
      var width:${dType} = originalIndices[${widthIdx}];
      ${useExtrapolation ? `if (depth < 0 || depth > (${inputShape[depthIdx]} - 1) || height < 0 || height > (${inputShape[heightIdx]} - 1) || width < 0 || (width > ${inputShape[widthIdx]} - 1)) {
      return ${extrapolationValue};
        }` : ""};

    depth = max(0, min(depth, ${inputShape[depthIdx]} - 1));
      height = max(0, min(height, ${inputShape[heightIdx]} - 1));
      width = max(0, min(width, ${inputShape[widthIdx]} - 1));
      var depth1: u32 = u32(depth);
      var height1: u32 = u32(height);
      var width1: u32 = u32(width);
      var depth2: u32 = u32(depth + 1);
      var height2: u32 = u32(height + 1);
      var width2: u32 = u32(width + 1);
      var channel: u32 = ${inputShape.length > 3 ? `u32(originalIndices[${channelIdx}])` : "0"};
      var batch: u32 =  ${inputShape.length > 3 ? `u32(originalIndices[${batchIdx}])` : "0"};

      var x111: ${dType} = getInputValue(batch, channel, depth1, height1, width1);
      var x112: ${dType} = getInputValue(batch, channel, depth1, height1, width2);
      var x121: ${dType} = getInputValue(batch, channel, depth1, height2, width1);
      var x122: ${dType} = getInputValue(batch, channel, depth1, height2, width2);
      var x211: ${dType} = getInputValue(batch, channel, depth2, height1, width1);
      var x212: ${dType} = getInputValue(batch, channel, depth2, height1, width2);
      var x221: ${dType} = getInputValue(batch, channel, depth2, height2, width1);
      var x222: ${dType} = getInputValue(batch, channel, depth2, height2, width2);
      var dx1: ${dType} = abs(depth - ${dType}(depth1));
      var dx2: ${dType} = abs(${dType}(depth2) - depth);
      var dy1: ${dType} = abs(height - ${dType}(height1));
      var dy2: ${dType} = abs(${dType}(height2) - height);
      var dz1: ${dType} = abs(width - ${dType}(width1));
      var dz2: ${dType} = abs(${dType}(width2) - width);
      if (depth1 == depth2) {
        dx1 = 0.5;
        dx2 = 0.5;
      }
      if (height1 == height2) {
        dy1 = 0.5;
        dy2 = 0.5;
      }
      if (width1 == width2) {
        dz1 = 0.5;
        dz2 = 0.5;
      }
      return (x111 * dx2 * dy2 * dz2 + x112 * dx2 * dy2 * dz1 + x121 * dx2 * dy1 *dz2 + x122 * dx2 * dy1 * dz1 +
              x211 * dx1 * dy2 * dz2 + x212 * dx1 * dy2 * dz1 + x221 * dx1 * dy1 *dz2 + x222 * dx1 * dy1 * dz1);
    }`;
    };
    createResizeProgramInfo = (inputTensor, attributes, opsetVersion, scalesInput, sizes, roiInput) => {
      const inputShape = inputTensor.dims;
      const roi = updateRoI(roiInput, attributes.axes, inputShape.length);
      let outputShape = initOutputShape(inputShape, scalesInput, sizes, attributes.axes);
      let scales = scalesInput.slice();
      if (scalesInput.length === 0) {
        scales = inputShape.map((value, index) => value === 0 ? 1 : outputShape[index] / value);
        if (attributes.keepAspectRatioPolicy !== "stretch") {
          outputShape = adjustOutputShape(inputShape, scales, attributes);
        }
      }
      const output = outputVariable("output", inputTensor.dataType, outputShape.length);
      const input = inputVariable("input", inputTensor.dataType, inputShape.length);
      const outputSize = ShapeUtil.size(outputShape);
      const noScale = inputShape.length === outputShape.length && inputShape.every((d, i) => d === outputShape[i]);
      const useExtrapolation = attributes.coordinateTransformMode === "tf_crop_and_resize";
      const extrapolationValue = attributes.extrapolationValue;
      const dataType = input.type.value;
      const getShaderSource = (shaderHelper) => `
      ${noScale ? "" : `
      ${getOriginalCoordinateFromResizedCoordinate(attributes.coordinateTransformMode, dataType)};
      ${(() => {
        switch (attributes.mode) {
          case "nearest":
            return `
              ${checkInputIndices(input, inputShape)};
              ${getNearestPixelFromOriginal(attributes.nearestMode, opsetVersion, dataType)};
              ${calculateInputIndicesFromOutputIndices(
              input,
              output,
              inputShape,
              outputShape,
              scales.length,
              roi.length,
              useExtrapolation
            )};
              `;
          case "linear":
            return `
              ${calculateOriginalIndicesFromOutputIndices(output, inputShape, outputShape, scales.length, roi.length)};
              ${(() => {
              if (inputShape.length === 2 || inputShape.length === 4) {
                return `${bilinearInterpolation(input, output, inputShape, useExtrapolation, extrapolationValue)}`;
              } else if (inputShape.length === 3 || inputShape.length === 5) {
                return `${trilinearInterpolation(input, output, inputShape, useExtrapolation, extrapolationValue)}`;
              } else {
                throw Error("Linear mode only supports input dims 2, 3, 4 and 5 are supported in linear mode.");
              }
            })()};
            `;
          case "cubic":
            return `
            ${(() => {
              if (inputShape.length === 2 || inputShape.length === 4) {
                return `${bicubicInterpolation(
                  input,
                  output,
                  inputShape,
                  outputShape,
                  scales,
                  roi,
                  attributes.cubicCoeffA,
                  useExtrapolation,
                  attributes.extrapolationValue,
                  attributes.excludeOutside
                )}`;
              } else {
                throw Error("Cubic mode only supports input dims 2 and 4 are supported in linear mode.");
              }
            })()};
            `;
          default:
            throw Error("Invalid resize mode");
        }
      })()};
      `}
      ${shaderHelper.registerUniform("output_size", "u32").registerUniform("scales", "f32", scales.length).registerUniform("roi", "f32", roi.length).declareVariables(input, output)}
      ${shaderHelper.mainStart()}
        ${shaderHelper.guardAgainstOutOfBoundsWorkgroupSizes("uniforms.output_size")}
        ${noScale ? "output[global_idx] = input[global_idx];" : `
        let output_indices = ${output.offsetToIndices("global_idx")};
        var input_indices: ${input.type.indices};
        ${(() => {
        switch (attributes.mode) {
          case "nearest":
            return `input_indices = calculateInputIndicesFromOutputIndices(output_indices);
                if (checkInputIndices(input_indices)) {
                  output[global_idx] = ${input.getByIndices("input_indices")};
                } else {
                  output[global_idx] = ${attributes.extrapolationValue};
                }`;
          case "linear":
            return `output[global_idx] = ${inputShape.length === 2 || inputShape.length === 4 ? "bilinearInterpolation" : "trilinearInterpolation"}(output_indices);`;
          case "cubic":
            return "output[global_idx] = bicubicInterpolation(output_indices);";
          default:
            throw Error(`Unsupported resize mode: ${attributes.mode}`);
        }
      })()};
`}
      }`;
      return {
        name: "Resize",
        shaderCache: {
          hint: `${attributes.cacheKey}|${opsetVersion}|${scales.length > 0 ? scales : ""}|${sizes.length > 0 ? sizes : ""}|${roi.length > 0 ? roi : ""}|${noScale}|${inputShape}`,
          inputDependencies: ["rank"]
        },
        getShaderSource,
        getRunData: () => ({
          outputs: [{ dims: outputShape, dataType: inputTensor.dataType }],
          dispatchGroup: { x: Math.ceil(
            outputSize / 64
            /* workgroup size */
          ) },
          programUniforms: [
            { type: 12 /* uint32 */, data: outputSize },
            { type: 1 /* float */, data: scales },
            { type: 1 /* float */, data: roi },
            ...createTensorShapeVariables(inputShape, outputShape)
          ]
        })
      };
    };
    getOpsetVersionFromCustomDataBuffer = (context) => {
      const customDataBuffer = context.customDataBuffer;
      const customDataBuffer32 = new Uint32Array(customDataBuffer, customDataBuffer.byteOffset, 1);
      const opsetVersion = customDataBuffer32[0];
      return opsetVersion;
    };
    resize = (context, attributes) => {
      const scales = [];
      const sizes = [];
      const roi = [];
      const opsetVersion = getOpsetVersionFromCustomDataBuffer(context);
      if (attributes.antialias !== 0) {
        throw Error("Only default value (0) for Antialias attribute is supported");
      }
      validateInputs25(context.inputs, attributes, opsetVersion, scales, sizes, roi);
      context.compute(createResizeProgramInfo(context.inputs[0], attributes, opsetVersion, scales, sizes, roi), {
        inputs: [0]
      });
    };
    parseResizeAttributes = (attributes) => {
      const antialias = attributes.antialias;
      const axes = attributes.axes;
      const coordinateTransformMode = attributes.coordinateTransformMode;
      const cubicCoeffA = attributes.cubicCoeffA;
      const excludeOutside = attributes.excludeOutside !== 0;
      const extrapolationValue = attributes.extrapolationValue;
      const keepAspectRatioPolicy = attributes.keepAspectRatioPolicy;
      const mode = attributes.mode;
      const nearestMode = attributes.nearestMode === "" ? "simple" : attributes.nearestMode;
      return createAttributeWithCacheKey({
        antialias,
        axes,
        coordinateTransformMode,
        cubicCoeffA,
        excludeOutside,
        extrapolationValue,
        keepAspectRatioPolicy,
        mode,
        nearestMode
      });
    };
  }
});

// web/lib/wasm/jsep/webgpu/ops/rotary-embedding.ts
var validateInputs26, createRotaryEmbeddingProgramInfo, rotaryEmbedding;
var init_rotary_embedding = __esm({
  "web/lib/wasm/jsep/webgpu/ops/rotary-embedding.ts"() {
    "use strict";
    init_wasm_common();
    init_util();
    init_attribute_with_cache_key();
    init_common();
    validateInputs26 = (inputs, attributes) => {
      const [input, positionIds, cosCache, sinCache] = inputs;
      const { numHeads, rotaryEmbeddingDim } = attributes;
      if (input.dims.length !== 3 && input.dims.length !== 4) {
        throw new Error(`Input 'x' is expected to have 3 or 4 dimensions, got ${input.dims.length}`);
      }
      if (!ShapeUtil.areEqual(positionIds.dims, []) && !ShapeUtil.areEqual(positionIds.dims, [1]) && positionIds.dims.length !== 2) {
        throw new Error(`Input 'position_ids' is expected to have 0, 1, or 2 dimensions, got ${positionIds.dims.length}`);
      }
      if (cosCache.dims.length !== 2) {
        throw new Error(`Input 'cos_cache' is expected to have 2 dimensions, got ${cosCache.dims.length}`);
      }
      if (sinCache.dims.length !== 2) {
        throw new Error(`Input 'sin_cache' is expected to have 2 dimensions, got ${sinCache.dims.length}`);
      }
      if (!ShapeUtil.areEqual(cosCache.dims, sinCache.dims)) {
        throw new Error("Inputs 'cos_cache' and 'sin_cache' are expected to have the same shape");
      }
      if (rotaryEmbeddingDim > 0 && numHeads === 0) {
        throw new Error("num_heads must be provided if rotary_embedding_dim is specified");
      }
      const batchSize = input.dims[0];
      const sequenceLength = input.dims[input.dims.length - 2];
      const maxSequenceLength = cosCache.dims[0];
      const hiddenSize = ShapeUtil.sizeFromDimension(input.dims, 1) / sequenceLength;
      const headSize = rotaryEmbeddingDim === 0 ? cosCache.dims[1] * 2 : hiddenSize / numHeads;
      if (rotaryEmbeddingDim > headSize) {
        throw new Error("rotary_embedding_dim must be less than or equal to head_size");
      }
      if (positionIds.dims.length === 2) {
        if (batchSize !== positionIds.dims[0]) {
          throw new Error(`Input 'position_ids' dimension 0 should be of size batch_size, got ${positionIds.dims[0]}`);
        }
        if (sequenceLength !== positionIds.dims[1]) {
          throw new Error(`Input 'position_ids' dimension 1 should be of size sequence_length, got ${positionIds.dims[1]}`);
        }
      }
      if (headSize / 2 !== cosCache.dims[1] && rotaryEmbeddingDim / 2 !== cosCache.dims[1]) {
        throw new Error(
          `Input 'cos_cache' dimension 1 should be same as head_size / 2 or rotary_embedding_dim / 2, got ${cosCache.dims[1]}`
        );
      }
      if (sequenceLength > maxSequenceLength) {
        throw new Error("Updating cos_cache and sin_cache in RotaryEmbedding is not currently supported");
      }
    };
    createRotaryEmbeddingProgramInfo = (inputs, attributes) => {
      const { interleaved, numHeads, rotaryEmbeddingDim, scale } = attributes;
      const batchSize = inputs[0].dims[0];
      const batchStride = ShapeUtil.sizeFromDimension(inputs[0].dims, 1);
      const sequenceLength = inputs[0].dims[inputs[0].dims.length - 2];
      const hiddenSize = batchStride / sequenceLength;
      const halfRotaryEmbeddingDim = inputs[2].dims[1];
      const headSize = rotaryEmbeddingDim === 0 ? halfRotaryEmbeddingDim * 2 : hiddenSize / numHeads;
      const globalShape = new Array(
        batchSize,
        sequenceLength,
        hiddenSize / headSize,
        headSize - halfRotaryEmbeddingDim
      );
      const globalStrides = ShapeUtil.computeStrides(globalShape);
      const programUniforms = [
        { type: 1 /* float */, data: scale },
        { type: 12 /* uint32 */, data: globalShape },
        { type: 12 /* uint32 */, data: globalStrides },
        // strides for addressing the input/output tensor, in permutated order to align with the unfolded global index,
        // i.e. BSNH
        ...inputs[0].dims.length === 3 ? new Array({ type: 12 /* uint32 */, data: [batchStride, hiddenSize, headSize, 1] }) : [],
        ...inputs[0].dims.length === 4 ? new Array({
          type: 12 /* uint32 */,
          data: [batchStride, headSize, sequenceLength * headSize, 1]
        }) : [],
        ...createTensorShapeVariables(inputs[0].dims, inputs[1].dims, inputs[2].dims, inputs[3].dims, inputs[0].dims)
      ];
      const getShaderSource = (shaderHelper) => {
        const input = inputVariable("input", inputs[0].dataType, inputs[0].dims.length);
        const positionIds = inputVariable("position_ids", inputs[1].dataType, inputs[1].dims.length);
        const cosCache = inputVariable("cos_cache", inputs[2].dataType, inputs[2].dims.length);
        const sinCache = inputVariable("sin_cache", inputs[3].dataType, inputs[3].dims.length);
        const output = outputVariable("output", inputs[0].dataType, inputs[0].dims.length);
        shaderHelper.registerUniforms([
          { name: "scale", type: "f32" },
          { name: "global_shape", type: "u32", length: globalShape.length },
          { name: "global_strides", type: "u32", length: globalStrides.length },
          { name: "input_output_strides", type: "u32", length: globalStrides.length }
        ]);
        return `
        ${shaderHelper.declareVariables(input, positionIds, cosCache, sinCache, output)}

        ${shaderHelper.mainStart(WORKGROUP_SIZE)}
          let half_rotary_emb_dim = uniforms.${cosCache.name}_shape[1];
          let bsnh = global_idx / uniforms.global_strides % uniforms.global_shape;
          let size = uniforms.global_shape[0] * uniforms.global_strides[0];
          ${shaderHelper.guardAgainstOutOfBoundsWorkgroupSizes("size")}

          if (bsnh[3] < half_rotary_emb_dim) {
            let position_ids_idx =
                ${positionIds.broadcastedIndicesToOffset("bsnh.xy", outputVariable("", positionIds.type.tensor, 2))};
            let position_id =
                u32(${positionIds.getByOffset("position_ids_idx")}) + select(0, bsnh[1], position_ids_idx == 0);
            let i = dot(bsnh, uniforms.input_output_strides) + select(0, bsnh[3], ${interleaved});
            let j = i + select(half_rotary_emb_dim, 1, ${interleaved});
            let re = ${input.getByOffset("i")} * ${cosCache.get("position_id", "bsnh[3]")} -
                ${input.getByOffset("j")} * ${sinCache.get("position_id", "bsnh[3]")};
            ${output.setByOffset("i", "re")}
            let im = ${input.getByOffset("i")} * ${sinCache.get("position_id", "bsnh[3]")} +
                ${input.getByOffset("j")} * ${cosCache.get("position_id", "bsnh[3]")};
            ${output.setByOffset("j", "im")}
          } else {
            let k = dot(bsnh, uniforms.input_output_strides) + half_rotary_emb_dim;
            ${output.setByOffset("k", input.getByOffset("k"))}
          }
        }`;
      };
      return {
        name: "RotaryEmbedding",
        shaderCache: {
          hint: createAttributeWithCacheKey({
            interleaved
          }).cacheKey,
          inputDependencies: ["rank", "rank", "rank", "rank"]
        },
        getShaderSource,
        getRunData: () => ({
          outputs: [{ dims: inputs[0].dims, dataType: inputs[0].dataType }],
          dispatchGroup: { x: Math.ceil(ShapeUtil.size(globalShape) / WORKGROUP_SIZE) },
          programUniforms
        })
      };
    };
    rotaryEmbedding = (context, attributes) => {
      validateInputs26(context.inputs, attributes);
      context.compute(createRotaryEmbeddingProgramInfo(context.inputs, attributes));
    };
  }
});

// web/lib/wasm/jsep/webgpu/ops/skip-layer-norm.ts
var validateInputs27, createSkipLayerNormProgramInfo, skipLayerNorm;
var init_skip_layer_norm = __esm({
  "web/lib/wasm/jsep/webgpu/ops/skip-layer-norm.ts"() {
    "use strict";
    init_wasm_common();
    init_util();
    init_common();
    validateInputs27 = (inputs) => {
      if (!inputs || inputs.length < 3) {
        throw new Error("layerNorm requires at least 3 inputs.");
      }
      const input = inputs[0];
      const skip = inputs[1];
      const gamma = inputs[2];
      if (input.dataType !== skip.dataType || input.dataType !== gamma.dataType) {
        throw new Error("All inputs must have the same data type");
      }
      if (input.dims.length !== 3 && input.dims.length !== 2) {
        throw new Error("Input must be 2D or 3D");
      }
      if (skip.dims.length !== 3 && skip.dims.length !== 2) {
        throw new Error("Skip must be 2D or 3D");
      }
      const hiddenSize = input.dims[input.dims.length - 1];
      const sequenceLength = input.dims[input.dims.length - 2];
      if (skip.dims[skip.dims.length - 1] !== hiddenSize) {
        throw new Error("Skip must have the same hidden size as input");
      }
      if (skip.dims[skip.dims.length - 2] !== sequenceLength) {
        throw new Error("Skip must have the same sequence length as input");
      }
      if (gamma.dims.length !== 1) {
        throw new Error("Gamma must be 1D");
      }
      if (gamma.dims[gamma.dims.length - 1] !== hiddenSize) {
        throw new Error("Gamma must have the same hidden size as input");
      }
      if (inputs.length > 3) {
        const beta = inputs[3];
        if (beta.dims.length !== 1) {
          throw new Error("Beta must be 1D");
        }
        if (beta.dims[beta.dims.length - 1] !== hiddenSize) {
          throw new Error("Beta must have the same hidden size as input");
        }
      }
      if (inputs.length > 4) {
        const bias = inputs[4];
        if (bias.dims.length !== 1) {
          throw new Error("Bias must be 1D");
        }
        if (bias.dims[bias.dims.length - 1] !== hiddenSize) {
          throw new Error("Bias must have the same hidden size as input");
        }
      }
    };
    createSkipLayerNormProgramInfo = (inputs, attributes, outputCount, isTraining) => {
      const simplified = attributes.simplified;
      const inputShape = inputs[0].dims;
      const inputSize = ShapeUtil.size(inputShape);
      const outputShape = inputShape;
      const outputSize = inputSize;
      const hiddenSize = inputShape.slice(-1)[0];
      const meanInvStdDevDim = isTraining ? inputShape.slice(0, -1).concat(1) : [];
      const hasBetaInput = !simplified && inputs.length > 3;
      const hasBiasInput = inputs.length > 4;
      const hasMeanOutput = isTraining && outputCount > 1;
      const hasInvStdDevOutput = isTraining && outputCount > 2;
      const hasInputSkipBiasSumOutput = outputCount > 3;
      const workgroupSize = 64;
      const components = getMaxComponents(hiddenSize);
      const programUniforms = [
        { type: 12 /* uint32 */, data: outputSize },
        { type: 12 /* uint32 */, data: components },
        { type: 12 /* uint32 */, data: hiddenSize },
        { type: 1 /* float */, data: attributes.epsilon }
      ];
      const getShaderSource = (shaderHelper) => {
        const uniformsArray = [
          { name: "output_size", type: "u32" },
          { name: "components", type: "u32" },
          { name: "hidden_size", type: "u32" },
          { name: "epsilon", type: "f32" }
        ];
        const variables = [
          inputVariable("x", inputs[0].dataType, inputs[0].dims, components),
          inputVariable("skip", inputs[1].dataType, inputs[1].dims, components),
          inputVariable("gamma", inputs[2].dataType, inputs[2].dims, components)
        ];
        if (hasBetaInput) {
          variables.push(inputVariable("beta", inputs[3].dataType, inputs[3].dims, components));
        }
        if (hasBiasInput) {
          variables.push(inputVariable("bias", inputs[4].dataType, inputs[4].dims, components));
        }
        variables.push(outputVariable("output", inputs[0].dataType, outputShape, components));
        if (hasMeanOutput) {
          variables.push(outputVariable("mean_output", 1 /* float */, meanInvStdDevDim));
        }
        if (hasInvStdDevOutput) {
          variables.push(outputVariable("inv_std_output", 1 /* float */, meanInvStdDevDim));
        }
        if (hasInputSkipBiasSumOutput) {
          variables.push(outputVariable("input_skip_bias_sum", inputs[0].dataType, outputShape, components));
        }
        const dataType = tensorTypeToWsglStorageType(inputs[0].dataType);
        const vecDataType = tensorTypeToWsglStorageType(1 /* float */, components);
        return `

      ${shaderHelper.registerUniforms(uniformsArray).declareVariables(...variables)}
      var<workgroup> sum_shared : array<${vecDataType}, ${workgroupSize}>;
      var<workgroup> sum_squared_shared : array<${vecDataType}, ${workgroupSize}>;

      ${shaderHelper.mainStart([workgroupSize, 1, 1])}
        let ix = local_id.x;
        let iy = global_id.x / ${workgroupSize};

        let hidden_size_vectorized: u32 = uniforms.hidden_size / uniforms.components;
        var stride = hidden_size_vectorized / ${workgroupSize};
        let offset = ix * stride + iy * hidden_size_vectorized;
        let offset1d = stride * ix;
        if (ix == ${workgroupSize - 1}) {
          stride = hidden_size_vectorized - stride * ix;
        }
        for (var i: u32 = 0; i < stride; i++) {
          let skip_value = skip[offset + i];
          let bias_value = ${hasBiasInput ? "bias[offset1d + i]" : dataType + "(0.0)"};
          let input_value = x[offset + i];
          let value = input_value + skip_value + bias_value;
          ${hasInputSkipBiasSumOutput ? "input_skip_bias_sum[offset + i] = value;" : ""}
          output[offset + i] = value;
          let f32_value = ${castToF32(dataType, components, "value")};
          sum_shared[ix] += f32_value;
          sum_squared_shared[ix] += f32_value * f32_value;
        }
        workgroupBarrier();

        var reduce_size : u32 = ${workgroupSize};
        for (var curr_size = reduce_size >> 1;  curr_size > 0; curr_size = reduce_size >> 1) {
          reduce_size = curr_size + (reduce_size & 1);
          if (ix < curr_size) {
            sum_shared[ix] += sum_shared[ix + reduce_size];
            sum_squared_shared[ix] += sum_squared_shared[ix + reduce_size];
          }
          workgroupBarrier();
        }

        let sum = sum_shared[0];
        let square_sum = sum_squared_shared[0];
        let mean = ${sumVector("sum", components)} / f32(uniforms.hidden_size);
        let inv_std_dev = inverseSqrt(${sumVector("square_sum", components)} / f32(uniforms.hidden_size) ${simplified ? "" : "- mean * mean"} + uniforms.epsilon);
        ${hasMeanOutput ? "mean_output[global_idx] = mean;" : ""}
        ${hasInvStdDevOutput ? "inv_std_output[global_idx] = inv_std_dev;" : ""}

        for (var i: u32 = 0; i < stride; i++) {
          output[offset + i] = (output[offset + i] ${simplified ? "" : `- ${dataType}(mean)`}) *
            ${dataType}(inv_std_dev) * gamma[offset1d + i]
            ${hasBetaInput ? "+ beta[offset1d + i]" : ""};
        }
      }`;
      };
      const outputs = [{ dims: outputShape, dataType: inputs[0].dataType }];
      if (outputCount > 1) {
        outputs.push({ dims: meanInvStdDevDim, dataType: 1 /* float */ });
      }
      if (outputCount > 2) {
        outputs.push({ dims: meanInvStdDevDim, dataType: 1 /* float */ });
      }
      if (outputCount > 3) {
        outputs.push({ dims: inputShape, dataType: inputs[0].dataType });
      }
      return {
        name: "SkipLayerNormalization",
        shaderCache: {
          hint: `${components};${hasMeanOutput};${hasInvStdDevOutput};${hasInputSkipBiasSumOutput}`,
          inputDependencies: inputs.map((_input, _index) => "type")
        },
        getShaderSource,
        getRunData: () => ({
          outputs,
          dispatchGroup: {
            x: Math.ceil(outputSize / hiddenSize)
          },
          programUniforms
        })
      };
    };
    skipLayerNorm = (context, attributes) => {
      const isTraining = false;
      validateInputs27(context.inputs);
      const outputs = [0];
      if (context.outputCount > 1) {
        outputs.push(isTraining ? 1 : -3);
      }
      if (context.outputCount > 2) {
        outputs.push(isTraining ? 2 : -3);
      }
      if (context.outputCount > 3) {
        outputs.push(3);
      }
      context.compute(createSkipLayerNormProgramInfo(context.inputs, attributes, context.outputCount, isTraining), {
        outputs
      });
    };
  }
});

// web/lib/wasm/jsep/webgpu/ops/slice.ts
var validateInputs28, readInput, createSliceAttributesFromInputs, fixStartEndValues, calculateInputIndicesImpl, createSliceProgramInfo, slice, parseSliceAttributes;
var init_slice = __esm({
  "web/lib/wasm/jsep/webgpu/ops/slice.ts"() {
    "use strict";
    init_wasm_common();
    init_util();
    init_attribute_with_cache_key();
    init_common();
    validateInputs28 = (inputs, attributes) => {
      if (!inputs || inputs.length < 1) {
        throw new Error("too few inputs");
      }
      if (attributes.axes.length !== 0) {
        if (attributes.axes.length !== attributes.starts.length || attributes.axes.length !== attributes.ends.length) {
          throw new Error("axes, starts and ends must have the same length");
        }
      } else if (attributes.starts.length !== attributes.ends.length) {
        throw new Error("starts and ends must have the same length");
      }
      inputs.slice(1).forEach((_, idx) => {
        if (inputs[idx + 1].dataType !== 6 /* int32 */ && inputs[idx + 1].dataType !== 7 /* int64 */) {
          throw new Error(`Input ${idx} must be an array of int32 or int64`);
        }
      });
    };
    readInput = (inputs, idx) => {
      const input = [];
      if (inputs.length > idx) {
        if (inputs[idx].dataType === 7 /* int64 */) {
          inputs[idx].getBigInt64Array().forEach((v) => input.push(Number(v)));
        } else if (inputs[idx].dataType === 6 /* int32 */) {
          inputs[idx].getInt32Array().forEach((v) => input.push(Number(v)));
        } else {
          throw new Error(`Input ${idx} must be an array of int32 or int64`);
        }
      }
      return input;
    };
    createSliceAttributesFromInputs = (inputs, attributes) => {
      if (inputs.length > 1) {
        const starts = readInput(inputs, 1);
        const ends = readInput(inputs, 2);
        let axes = readInput(inputs, 3);
        if (axes.length === 0) {
          axes = [...Array(inputs[0].dims.length).keys()];
        }
        return createAttributeWithCacheKey({ starts, ends, axes });
      } else {
        return attributes;
      }
    };
    fixStartEndValues = (value, index, inputShape, axes, steps) => {
      let newValue = value;
      if (value < 0) {
        newValue += inputShape[axes[index]];
      }
      if (steps[index] < 0) {
        return Math.max(0, Math.min(newValue, inputShape[axes[index]] - 1));
      } else {
        return Math.max(0, Math.min(newValue, inputShape[axes[index]]));
      }
    };
    calculateInputIndicesImpl = (input, output, inputShape) => `fn calculateInputIndices(output_indices: ${output.type.indices}) -> ${input.type.indices} {
          var input_indices: ${input.type.indices};
          var carry = 0u;
          for (var i = ${inputShape.length}; i >= 0; i--) {
            let input_shape_i = ${getElementAt("uniforms.input_shape", "i", inputShape.length)};
            let steps_i = ${getElementAt("uniforms.steps", "i", inputShape.length)};
            let signs_i = ${getElementAt("uniforms.signs", "i", inputShape.length)};
            let starts_i = ${getElementAt("uniforms.starts", "i", inputShape.length)};
            var output_index = ${output.indicesGet("output_indices", "i")};
            var input_index = output_index * steps_i + starts_i + carry;
            carry = input_index / input_shape_i;
            input_index = input_index % input_shape_i;
            if (signs_i < 0) {
              input_index = input_shape_i - input_index - 1u + starts_i;
            }
            ${input.indicesSet("input_indices", "i", "input_index")};
          }
          return input_indices;
      }`;
    createSliceProgramInfo = (inputs, attributes) => {
      const inputShape = inputs[0].dims;
      const inputSize = ShapeUtil.size(inputShape);
      const axes = attributes.axes.length > 0 ? ShapeUtil.normalizeAxes(attributes.axes, inputShape.length) : [...Array(inputShape.length).keys()];
      let steps = readInput(inputs, 4);
      steps.forEach(
        (step) => step !== 0 || (() => {
          throw new Error("step cannot be 0");
        })
      );
      if (steps.length === 0) {
        steps = Array(axes.length).fill(1);
      }
      const starts = attributes.starts.map((start, i) => fixStartEndValues(start, i, inputShape, axes, steps));
      const ends = attributes.ends.map((end, i) => fixStartEndValues(end, i, inputShape, axes, steps));
      if (axes.length !== starts.length || axes.length !== ends.length) {
        throw new Error("start, ends and axes should have the same number of elements");
      }
      if (axes.length !== inputShape.length) {
        for (let i = 0; i < inputShape.length; ++i) {
          if (!axes.includes(i)) {
            starts.splice(i, 0, 0);
            ends.splice(i, 0, inputShape[i]);
            steps.splice(i, 0, 1);
          }
        }
      }
      const signs = steps.map((step) => Math.sign(step));
      steps.forEach((step, i, array) => {
        if (step < 0) {
          const numSteps = (ends[i] - starts[i]) / step;
          const newEnd = starts[i];
          const newStart = newEnd + numSteps * steps[i];
          starts[i] = newStart;
          ends[i] = newEnd;
          array[i] = -step;
        }
      });
      const outputShape = inputShape.slice(0);
      axes.forEach((axis, _) => {
        outputShape[axis] = Math.ceil((ends[axis] - starts[axis]) / steps[axis]);
      });
      const outputTensorInfo = { dims: outputShape, dataType: inputs[0].dataType };
      const output = outputVariable("output", inputs[0].dataType, outputShape.length);
      const input = inputVariable("input", inputs[0].dataType, inputs[0].dims.length);
      const outputSize = ShapeUtil.size(outputShape);
      const uniforms = [
        { name: "outputSize", type: "u32" },
        { name: "starts", type: "u32", length: starts.length },
        { name: "signs", type: "i32", length: signs.length },
        { name: "steps", type: "u32", length: steps.length }
      ];
      const programUniforms = [
        { type: 12 /* uint32 */, data: outputSize },
        { type: 12 /* uint32 */, data: starts },
        { type: 6 /* int32 */, data: signs },
        { type: 12 /* uint32 */, data: steps },
        ...createTensorShapeVariables(inputs[0].dims, outputShape)
      ];
      const getShaderSource = (shaderHelper) => `
      ${shaderHelper.registerUniforms(uniforms).declareVariables(input, output)}
        ${calculateInputIndicesImpl(input, output, inputShape)}
        ${shaderHelper.mainStart()}
          ${shaderHelper.guardAgainstOutOfBoundsWorkgroupSizes("uniforms.outputSize")}
          let output_indices = ${output.offsetToIndices("global_idx")};
          let input_indices = calculateInputIndices(output_indices);
          ${output.setByOffset("global_idx", input.getByIndices("input_indices"))}
      }`;
      return {
        name: "Slice",
        shaderCache: { hint: `${signs.length}_${starts.length}_${steps.length}`, inputDependencies: ["rank"] },
        getShaderSource,
        getRunData: () => ({
          outputs: [outputTensorInfo],
          dispatchGroup: { x: Math.ceil(
            inputSize / 64
            /* workgroup size */
          ) },
          programUniforms
        })
      };
    };
    slice = (context, attributes) => {
      validateInputs28(context.inputs, attributes);
      const updatedAttributes = createSliceAttributesFromInputs(context.inputs, attributes);
      context.compute(createSliceProgramInfo(context.inputs, updatedAttributes), { inputs: [0] });
    };
    parseSliceAttributes = (attributes) => {
      const starts = attributes.starts;
      const ends = attributes.ends;
      const axes = attributes.axes;
      return createAttributeWithCacheKey({ starts, ends, axes });
    };
  }
});

// web/lib/wasm/jsep/webgpu/ops/softmax.ts
var validateInputs29, createSoftmaxProgramInfo, softmax, parseSoftmaxAttributes;
var init_softmax = __esm({
  "web/lib/wasm/jsep/webgpu/ops/softmax.ts"() {
    "use strict";
    init_wasm_common();
    init_util();
    init_attribute_with_cache_key();
    init_common();
    validateInputs29 = (inputs) => {
      if (!inputs || inputs.length !== 1) {
        throw new Error("Softmax op requires 1 input.");
      }
    };
    createSoftmaxProgramInfo = (input, attributes) => {
      const shape = input.dims;
      const outputSize = ShapeUtil.size(shape);
      const WG = 64;
      let axis = attributes.axis;
      if (axis < 0) {
        axis = shape.length + axis;
      }
      if (axis < shape.length - 1) {
        throw new Error("softmax only supports last axis for now.");
      }
      const cols = shape[axis];
      const rows = outputSize / cols;
      const components = getMaxComponents(cols);
      const packedCols = cols / components;
      const maxVector = (name, components2) => {
        if (components2 === 4) {
          return `max(max(${name}.x, ${name}.y), max(${name}.z, ${name}.w))`;
        } else if (components2 === 2) {
          return `max(${name}.x, ${name}.y)`;
        } else if (components2 === 3) {
          return `max(max(${name}.x, ${name}.y), ${name}.z)`;
        }
        return name;
      };
      const x = inputVariable("x", input.dataType, input.dims, components);
      const output = outputVariable("result", input.dataType, input.dims, components);
      const valueType = x.type.value;
      const threadMaxDecl = tensorTypeToWsglStorageType(input.dataType) === "f32" ? `var threadMax = ${valueType}(-3.402823e+38f);` : `var threadMax = ${valueType}(-65504.0h);`;
      const getShaderSource = (shaderHelper) => `
      var<workgroup> rowMaxShared : ${valueType};
      var<workgroup> rowSumShared : ${valueType};
      var<workgroup> threadShared : array<${valueType}, ${WG}>;

      fn getValue(row: i32, col: i32, row_stride: i32) -> ${valueType} {
        let index = row * row_stride + col;
        return x[index];
      }

      fn setValue(row: i32, col: i32, row_stride: i32, value: ${valueType}) {
        let index = row * row_stride + col;
        result[index] = value;
      }
      ${shaderHelper.registerUniform("packedCols", "i32").declareVariables(x, output)}
      ${shaderHelper.mainStart()}
        let gindex = i32(global_idx);
        let lindex = i32(local_idx);
        const wg = ${WG};
        let row = gindex / wg;
        let cols = uniforms.packedCols;
        let row_stride : i32 = uniforms.packedCols;

        // find the rows max
        ${threadMaxDecl}
        for (var col = lindex; col < cols; col += wg) {
          let value = getValue(row, col, row_stride);
          threadMax = max(threadMax, value);
        }
        if (lindex < cols) {
          threadShared[lindex] = threadMax;
        }
        workgroupBarrier();

        var reduceSize = min(cols, wg);
        for (var currSize = reduceSize >> 1;  currSize > 0; currSize = reduceSize >> 1) {
          reduceSize = currSize + (reduceSize & 1);
          if (lindex < currSize) {
            threadShared[lindex] = max(threadShared[lindex], threadShared[lindex + reduceSize]);
          }
          workgroupBarrier();
        }
        if (lindex == 0) {
          rowMaxShared = ${valueType}(${maxVector("threadShared[0]", components)});
        }
        workgroupBarrier();

        // find the rows sum
        var threadSum = ${valueType}(0.0);
        for (var col = lindex; col < cols; col += wg) {
          let subExp = exp(getValue(row, col, row_stride) - rowMaxShared);
          threadSum += subExp;
        }
        threadShared[lindex] = threadSum;
        workgroupBarrier();

        for (var currSize = wg >> 1;  currSize > 0; currSize = currSize >> 1) {
          if (lindex < currSize) {
            threadShared[lindex] = threadShared[lindex] + threadShared[lindex + currSize];
          }
          workgroupBarrier();
        }
        if (lindex == 0) {
          rowSumShared = ${valueType}(${sumVector("threadShared[0]", components)});
        }
        workgroupBarrier();

        // calculate final value for each element in the row
        for (var col = lindex; col < cols; col += wg) {
          let value = exp(getValue(row, col, row_stride) - rowMaxShared) / rowSumShared;
          setValue(row, col, row_stride, value);
        }
      }`;
      return {
        name: "Softmax",
        shaderCache: { hint: `${components}`, inputDependencies: ["type"] },
        getRunData: () => ({
          outputs: [{ dims: shape, dataType: input.dataType }],
          dispatchGroup: { x: rows },
          programUniforms: [{ type: 6 /* int32 */, data: packedCols }]
        }),
        getShaderSource
      };
    };
    softmax = (context, attributes) => {
      validateInputs29(context.inputs);
      context.compute(createSoftmaxProgramInfo(context.inputs[0], attributes));
    };
    parseSoftmaxAttributes = (attributes) => createAttributeWithCacheKey({ axis: attributes.axis });
  }
});

// web/lib/wasm/jsep/webgpu/ops/split.ts
var validateInputs30, createSplitAttributesFromInputs, calculateOutputIndexImpl, writeBufferDataImpl, createSplitProgramInfo, split, parseSplitAttributes;
var init_split = __esm({
  "web/lib/wasm/jsep/webgpu/ops/split.ts"() {
    "use strict";
    init_wasm_common();
    init_util();
    init_attribute_with_cache_key();
    init_common();
    validateInputs30 = (inputs) => {
      if (!inputs || inputs.length < 1) {
        throw new Error("too few inputs");
      }
    };
    createSplitAttributesFromInputs = (inputs, attributes) => {
      const splitSizes = [];
      let numOutputs = attributes.numOutputs;
      if (inputs[1].dims[0] > 0) {
        inputs[1].getBigInt64Array().forEach((v) => splitSizes.push(Number(v)));
        numOutputs = splitSizes.length;
      }
      return createAttributeWithCacheKey({ numOutputs, axis: attributes.axis, splitSizes });
    };
    calculateOutputIndexImpl = (numberOfTensors) => `
fn calculateOutputIndex(index: u32) -> u32 {
    for (var i: u32 = 0u; i < ${numberOfTensors}u; i += 1u ) {
    if (index < ${getElementAt("uniforms.size_in_split_axis", "i", numberOfTensors)}) {
        return i;
    }
    }
    return ${numberOfTensors}u;
}`;
    writeBufferDataImpl = (outputs) => {
      const numberOfTensors = outputs.length;
      const codeLines = [];
      for (let i = 0; i < numberOfTensors; ++i) {
        const returnSnippet = outputs[i].setByIndices("indices", "input[global_idx]");
        if (numberOfTensors === 1) {
          codeLines.push(returnSnippet);
        } else if (i === 0) {
          codeLines.push(`if (output_number == ${i}u) { ${returnSnippet} }`);
        } else if (i === numberOfTensors - 1) {
          codeLines.push(`else { ${returnSnippet} }`);
        } else {
          codeLines.push(`else if (output_number == ${i}) { ${returnSnippet} }`);
        }
      }
      return `
      fn writeBufferData(output_number: u32, indices: ${outputs[0].type.indices}, global_idx: u32) {
        ${codeLines.join("\n")}
      }`;
    };
    createSplitProgramInfo = (inputs, attributes) => {
      const inputShape = inputs[0].dims;
      const inputSize = ShapeUtil.size(inputShape);
      const dataType = inputs[0].dataType;
      const axis = ShapeUtil.normalizeAxis(attributes.axis, inputShape.length);
      const outputs = new Array(attributes.numOutputs);
      const input = inputVariable("input", dataType, inputShape.length);
      const sizeInSplitAxis = new Array(attributes.numOutputs);
      const outputsTensorInfo = [];
      const outputShapes = [];
      let previousSum = 0;
      const programUniforms = [{ type: 12 /* uint32 */, data: inputSize }];
      for (let i = 0; i < attributes.numOutputs; i++) {
        previousSum += attributes.splitSizes[i];
        sizeInSplitAxis[i] = previousSum;
        const outputShape = inputShape.slice();
        outputShape[axis] = attributes.splitSizes[i];
        outputShapes.push(outputShape);
        outputs[i] = outputVariable(`output${i}`, dataType, outputShape.length);
        outputsTensorInfo.push({ dims: outputShapes[i], dataType: inputs[0].dataType });
      }
      programUniforms.push(
        { type: 12 /* uint32 */, data: sizeInSplitAxis },
        ...createTensorShapeVariables(inputShape, ...outputShapes)
      );
      const getShaderSource = (shaderHelper) => `
  ${shaderHelper.registerUniform("input_size", "u32").registerUniform("size_in_split_axis", "u32", sizeInSplitAxis.length).declareVariables(input, ...outputs)}
  ${calculateOutputIndexImpl(sizeInSplitAxis.length)}
  ${writeBufferDataImpl(outputs)}

  ${shaderHelper.mainStart()}
    ${shaderHelper.guardAgainstOutOfBoundsWorkgroupSizes("uniforms.input_size")}

    var indices = ${input.offsetToIndices("global_idx")};
    var index = ${input.indicesGet("indices", axis)};
    let output_number = calculateOutputIndex(index);
    if (output_number != 0) {
      index -= ${getElementAt("uniforms.size_in_split_axis", "output_number - 1u", sizeInSplitAxis.length)};
      ${input.indicesSet("indices", axis, "index")};
    }
    writeBufferData(output_number, indices, global_idx);
  }`;
      return {
        name: "Split",
        shaderCache: { hint: attributes.cacheKey, inputDependencies: ["rank"] },
        getShaderSource,
        getRunData: () => ({
          outputs: outputsTensorInfo,
          dispatchGroup: { x: Math.ceil(
            inputSize / 64
            /* workgroup size */
          ) },
          programUniforms
        })
      };
    };
    split = (context, attributes) => {
      validateInputs30(context.inputs);
      const updatedAttributes = context.inputs.length === 1 ? attributes : createSplitAttributesFromInputs(context.inputs, attributes);
      context.compute(createSplitProgramInfo(context.inputs, updatedAttributes), { inputs: [0] });
    };
    parseSplitAttributes = (attributes) => {
      const axis = attributes.axis;
      const splitSizes = attributes.splitSizes;
      const numOutputs = attributes.numOutputs < 0 ? splitSizes.length : attributes.numOutputs;
      if (numOutputs !== splitSizes.length) {
        throw new Error("numOutputs and splitSizes lengh must be equal");
      }
      return createAttributeWithCacheKey({ axis, numOutputs, splitSizes });
    };
  }
});

// web/lib/wasm/jsep/webgpu/ops/where.ts
var createWhereOpProgramShader, createWhereOpProgramInfo, where;
var init_where = __esm({
  "web/lib/wasm/jsep/webgpu/ops/where.ts"() {
    "use strict";
    init_wasm_common();
    init_util();
    init_common();
    createWhereOpProgramShader = (shaderHelper, inputs, dimsOutput, isBroadcast, typeOutput) => {
      const output = outputVariable("output_data", typeOutput, dimsOutput.length, 4);
      const a = inputVariable("a_data", inputs[1].dataType, inputs[1].dims.length, 4);
      const b = inputVariable("b_data", inputs[2].dataType, inputs[2].dims.length, 4);
      const c = inputVariable("c_data", inputs[0].dataType, inputs[0].dims.length, 4);
      let assignment;
      const expression = (a2, b2, c2) => `select(${b2}, ${a2}, ${c2})`;
      if (!isBroadcast) {
        assignment = output.setByOffset(
          "global_idx",
          expression(a.getByOffset("global_idx"), b.getByOffset("global_idx"), c.getByOffset("global_idx"))
        );
      } else {
        const singleAssignment = (resStr, x, typeCast = "") => {
          const expressionA = `a_data[index_a${x}][component_a${x}]`;
          const expressionB = `b_data[index_b${x}][component_b${x}]`;
          const expressionC = `bool(c_data[index_c${x}] & (0xffu << (component_c${x} * 8)))`;
          return `
            let output_indices${x} = ${output.offsetToIndices(`global_idx * 4u + ${x}u`)};
            let offset_a${x} = ${a.broadcastedIndicesToOffset(`output_indices${x}`, output)};
            let offset_b${x} = ${b.broadcastedIndicesToOffset(`output_indices${x}`, output)};
            let offset_c${x} = ${c.broadcastedIndicesToOffset(`output_indices${x}`, output)};
            let index_a${x} = offset_a${x} / 4u;
            let index_b${x} = offset_b${x} / 4u;
            let index_c${x} = offset_c${x} / 4u;
            let component_a${x} = offset_a${x} % 4u;
            let component_b${x} = offset_b${x} % 4u;
            let component_c${x} = offset_c${x} % 4u;
            ${resStr}[${x}] = ${typeCast}(${expression(expressionA, expressionB, expressionC)});
          `;
        };
        if (typeOutput === 9 /* bool */) {
          assignment = `
            var data = vec4<u32>(0);
            ${singleAssignment("data", 0, "u32")}
            ${singleAssignment("data", 1, "u32")}
            ${singleAssignment("data", 2, "u32")}
            ${singleAssignment("data", 3, "u32")}
            output_data[global_idx] = dot(vec4<u32>(0x1, 0x100, 0x10000, 0x1000000), vec4<u32>(data));`;
        } else {
          assignment = `
            ${singleAssignment("output_data[global_idx]", 0)}
            ${singleAssignment("output_data[global_idx]", 1)}
            ${singleAssignment("output_data[global_idx]", 2)}
            ${singleAssignment("output_data[global_idx]", 3)}
          `;
        }
      }
      return `
        ${shaderHelper.registerUniform("vec_size", "u32").declareVariables(c, a, b, output)}
        ${shaderHelper.mainStart()}
        ${shaderHelper.guardAgainstOutOfBoundsWorkgroupSizes("uniforms.vec_size")}
        ${assignment}
      }`;
    };
    createWhereOpProgramInfo = (inputs) => {
      const dimsA = inputs[1].dims;
      const dimsB = inputs[2].dims;
      const dimsC = inputs[0].dims;
      const outputDataType = inputs[1].dataType;
      const isBroadcast = !(ShapeUtil.areEqual(dimsA, dimsB) && ShapeUtil.areEqual(dimsB, dimsC));
      let outputShape = dimsA;
      let outputSize = ShapeUtil.size(dimsA);
      if (isBroadcast) {
        const calculatedShape = BroadcastUtil.calcShape(BroadcastUtil.calcShape(dimsA, dimsB, false), dimsC, false);
        if (!calculatedShape) {
          throw new Error("Can't perform where op on the given tensors");
        }
        outputShape = calculatedShape;
        outputSize = ShapeUtil.size(outputShape);
      }
      const vecSize = Math.ceil(outputSize / 4);
      return {
        name: "Where",
        shaderCache: { inputDependencies: ["rank", "rank", "rank"] },
        getShaderSource: (shaderHelper) => createWhereOpProgramShader(shaderHelper, inputs, outputShape, isBroadcast, outputDataType),
        getRunData: () => ({
          outputs: [{ dims: outputShape, dataType: outputDataType }],
          dispatchGroup: { x: Math.ceil(
            outputSize / 64 / 4
            /* vec size */
          ) },
          programUniforms: [
            { type: 12 /* uint32 */, data: vecSize },
            ...createTensorShapeVariables(dimsC, dimsA, dimsB, outputShape)
          ]
        })
      };
    };
    where = (context) => {
      context.compute(createWhereOpProgramInfo(context.inputs));
    };
  }
});

// web/lib/wasm/jsep/webgpu/op-resolve-rules.ts
var WEBGPU_OP_RESOLVE_RULES;
var init_op_resolve_rules = __esm({
  "web/lib/wasm/jsep/webgpu/op-resolve-rules.ts"() {
    "use strict";
    init_argminmax();
    init_attention();
    init_batch_norm();
    init_bias_add();
    init_bias_split_gelu();
    init_binary_op();
    init_concat();
    init_conv();
    init_conv_transpose();
    init_cumsum();
    init_depth_to_space();
    init_einsum();
    init_expand();
    init_fast_gelu();
    init_gather();
    init_gather_block_quantized();
    init_gather_elements();
    init_gemm();
    init_group_query_attention();
    init_instance_norm();
    init_layer_norm();
    init_matmul();
    init_matmulnbits();
    init_multihead_attention();
    init_pad();
    init_pool();
    init_quantize_linear();
    init_range();
    init_reduce();
    init_resize();
    init_rotary_embedding();
    init_skip_layer_norm();
    init_slice();
    init_softmax();
    init_split();
    init_tile();
    init_transpose();
    init_unary_op();
    init_where();
    WEBGPU_OP_RESOLVE_RULES = /* @__PURE__ */ new Map([
      ["Abs", [abs]],
      ["Acos", [acos]],
      ["Acosh", [acosh]],
      ["Add", [add]],
      ["ArgMax", [argMax, parseArgMinMaxAttributes]],
      ["ArgMin", [argMin, parseArgMinMaxAttributes]],
      ["Asin", [asin]],
      ["Asinh", [asinh]],
      ["Atan", [atan]],
      ["Atanh", [atanh]],
      ["Attention", [attention]],
      // TODO: support new attributes for AveragePool-10
      ["AveragePool", [averagePool, parseAveragePoolAttributes]],
      ["BatchNormalization", [batchNorm]],
      ["BiasAdd", [biasAdd]],
      ["BiasSplitGelu", [biasSplitGelu]],
      ["Cast", [cast, parseCastAttributes]],
      ["Ceil", [ceil]],
      ["Clip", [clip]],
      ["Concat", [concat, parseConcatAttributes]],
      ["Conv", [conv, parseConvAttributes]],
      ["ConvTranspose", [convTranspose, parseConvTransposeAttributes]],
      ["Cos", [cos]],
      ["Cosh", [cosh]],
      ["CumSum", [cumsum, parseCumSumAttributes]],
      ["DepthToSpace", [depthToSpace, parseDepthToSpaceAttributes]],
      ["DequantizeLinear", [dequantizeLinear, parseDequantizeLinearAttributes]],
      ["Div", [div]],
      ["Einsum", [einsum, parseEinsumAttributes]],
      ["Elu", [elu, parseAlphaAttributes]],
      ["Equal", [equal]],
      ["Erf", [erf]],
      ["Exp", [exp]],
      ["Expand", [expand]],
      ["FastGelu", [fastGelu2]],
      ["Floor", [floor]],
      ["FusedConv", [conv, parseConvAttributes]],
      ["Gather", [gather, parseGatherAttributes]],
      ["GatherElements", [gatherElements, parseGatherElementsAttributes]],
      ["GatherBlockQuantized", [gatherBlockQuantized, parseGatherBlockQuantizedAttributes]],
      ["Gelu", [gelu]],
      ["Gemm", [gemm, parseGemmAttributes]],
      ["GlobalAveragePool", [globalAveragePool, parseGlobalAveragePoolAttributes]],
      ["GlobalMaxPool", [globalMaxPool, parseGlobalMaxPoolAttributes]],
      ["Greater", [greater]],
      ["GreaterOrEqual", [greaterOrEqual]],
      ["GroupQueryAttention", [groupQueryAttention, parseGroupQueryAttentionAttributes]],
      ["HardSigmoid", [hardSigmoid, parseHardSigmoidAttributes]],
      ["InstanceNormalization", [instanceNorm]],
      ["LayerNormalization", [layerNorm]],
      ["LeakyRelu", [leakyRelu, parseAlphaAttributes]],
      ["Less", [less]],
      ["LessOrEqual", [lessOrEqual]],
      ["Log", [log]],
      ["MatMul", [matMul]],
      ["MatMulNBits", [matMulNBits, parseMatMulNBitsAttributes]],
      // TODO: support new attributes for MaxPool-8 and MaxPool-10
      ["MaxPool", [maxPool, parseMaxPoolAttributes]],
      ["Mul", [mul]],
      ["MultiHeadAttention", [multiHeadAttention, parseMultiHeadAttentionAttributes]],
      ["Neg", [neg]],
      ["Not", [not]],
      ["Pad", [pad]],
      ["Pow", [pow]],
      ["QuickGelu", [quickgelu, parseAlphaAttributes]],
      ["Range", [range]],
      ["Reciprocal", [reciprocal]],
      ["ReduceMin", [reduceMin]],
      ["ReduceMean", [reduceMean]],
      ["ReduceMax", [reduceMax]],
      ["ReduceSum", [reduceSum]],
      ["ReduceProd", [reduceProd]],
      ["ReduceL1", [reduceL1]],
      ["ReduceL2", [reduceL2]],
      ["ReduceLogSum", [reduceLogSum]],
      ["ReduceLogSumExp", [reduceLogSumExp]],
      ["ReduceSumSquare", [reduceSumSquare]],
      ["Relu", [relu]],
      ["Resize", [resize, parseResizeAttributes]],
      ["RotaryEmbedding", [rotaryEmbedding]],
      ["Sigmoid", [sigmoid]],
      ["Sin", [sin]],
      ["Sinh", [sinh]],
      ["Slice", [slice, parseSliceAttributes]],
      ["SkipLayerNormalization", [skipLayerNorm]],
      ["Split", [split, parseSplitAttributes]],
      ["Sqrt", [sqrt]],
      ["Softmax", [softmax, parseSoftmaxAttributes]],
      ["Sub", [sub]],
      ["Tan", [tan]],
      ["Tanh", [tanh]],
      ["ThresholdedRelu", [thresholdedRelu, parseAlphaAttributes]],
      ["Tile", [tile]],
      ["Transpose", [transpose, parseTransposeAttributes]],
      ["Where", [where]]
    ]);
  }
});

// web/lib/wasm/jsep/webgpu/program-manager.ts
var ProgramManager;
var init_program_manager = __esm({
  "web/lib/wasm/jsep/webgpu/program-manager.ts"() {
    "use strict";
    init_esm();
    init_log();
    init_common();
    ProgramManager = class {
      constructor(backend) {
        this.backend = backend;
        this.repo = /* @__PURE__ */ new Map();
        this.attributesBound = false;
      }
      getArtifact(key) {
        return this.repo.get(key);
      }
      setArtifact(key, artifact) {
        this.repo.set(key, artifact);
      }
      run(buildArtifact, inputs, outputs, dispatchGroup, uniformBufferBinding) {
        TRACE_FUNC_BEGIN(buildArtifact.programInfo.name);
        const device = this.backend.device;
        const computePassEncoder = this.backend.getComputePassEncoder();
        this.backend.writeTimestamp(this.backend.pendingDispatchNumber * 2);
        const entries = [];
        for (const input of inputs) {
          entries.push({ binding: entries.length, resource: { buffer: input.buffer } });
        }
        for (const output of outputs) {
          entries.push({ binding: entries.length, resource: { buffer: output.buffer } });
        }
        if (uniformBufferBinding) {
          entries.push({ binding: entries.length, resource: uniformBufferBinding });
        }
        const bindGroup = device.createBindGroup({
          layout: buildArtifact.computePipeline.getBindGroupLayout(0),
          entries,
          label: buildArtifact.programInfo.name
        });
        if (this.backend.sessionStatus === "capturing") {
          const commandInfo = {
            kernelId: this.backend.currentKernelId,
            computePipeline: buildArtifact.computePipeline,
            bindGroup,
            dispatchGroup
          };
          const sessionCommandList = this.backend.capturedCommandList.get(this.backend.currentSessionId);
          sessionCommandList.push(commandInfo);
        }
        computePassEncoder.setPipeline(buildArtifact.computePipeline);
        computePassEncoder.setBindGroup(0, bindGroup);
        computePassEncoder.dispatchWorkgroups(...dispatchGroup);
        this.backend.writeTimestamp(this.backend.pendingDispatchNumber * 2 + 1);
        this.backend.pendingDispatchNumber++;
        if (this.backend.pendingDispatchNumber >= this.backend.maxDispatchNumber || this.backend.queryType === "at-passes") {
          this.backend.endComputePass();
        }
        if (this.backend.pendingDispatchNumber >= this.backend.maxDispatchNumber) {
          this.backend.flush();
        }
        TRACE_FUNC_END(buildArtifact.programInfo.name);
      }
      dispose() {
      }
      build(programInfo, normalizedDispatchGroupSize) {
        TRACE_FUNC_BEGIN(programInfo.name);
        const device = this.backend.device;
        const extensions = [];
        if (device.features.has("shader-f16")) {
          extensions.push("enable f16;");
        }
        const shaderHelper = createShaderHelper(normalizedDispatchGroupSize, this.backend.device.limits);
        const userCode = programInfo.getShaderSource(shaderHelper);
        const code = `${extensions.join("\n")}
${shaderHelper.additionalImplementations}
${userCode}`;
        const shaderModule = device.createShaderModule({ code, label: programInfo.name });
        LOG_DEBUG("verbose", () => `[WebGPU] ${programInfo.name} shader code: ${code}`);
        const computePipeline = device.createComputePipeline({
          compute: { module: shaderModule, entryPoint: "main" },
          layout: "auto",
          label: programInfo.name
        });
        TRACE_FUNC_END(programInfo.name);
        return { programInfo, computePipeline, uniformVariablesInfo: shaderHelper.variablesInfo };
      }
      normalizeDispatchGroupSize(dispatchGroup) {
        const x = typeof dispatchGroup === "number" ? dispatchGroup : dispatchGroup.x;
        const y = typeof dispatchGroup === "number" ? 1 : dispatchGroup.y || 1;
        const z = typeof dispatchGroup === "number" ? 1 : dispatchGroup.z || 1;
        const limitPerDimension = this.backend.device.limits.maxComputeWorkgroupsPerDimension;
        if (x <= limitPerDimension && y <= limitPerDimension && z <= limitPerDimension) {
          return [x, y, z];
        }
        const size = x * y * z;
        let dispatchAverage = Math.ceil(Math.sqrt(size));
        if (dispatchAverage > limitPerDimension) {
          dispatchAverage = Math.ceil(Math.cbrt(size));
          if (dispatchAverage > limitPerDimension) {
            throw new Error("Total dispatch size exceeds WebGPU maximum.");
          }
          return [dispatchAverage, dispatchAverage, dispatchAverage];
        } else {
          return [dispatchAverage, dispatchAverage, 1];
        }
      }
    };
  }
});

// web/lib/wasm/jsep/backend-webgpu.ts
var getProgramInputTensorInfoDependencyKey, getProgramInfoUniqueKey, AdapterInfoImpl, WebGpuBackend;
var init_backend_webgpu = __esm({
  "web/lib/wasm/jsep/backend-webgpu.ts"() {
    "use strict";
    init_esm();
    init_wasm_common();
    init_log();
    init_tensor_view();
    init_gpu_data_manager();
    init_op_resolve_rules();
    init_program_manager();
    getProgramInputTensorInfoDependencyKey = (inputTensors, inputDependencies) => {
      if (inputDependencies.length !== inputTensors.length) {
        throw new Error(
          `inputDependencies length ${inputDependencies.length} is not equal to inputTensors length ${inputTensors.length}.`
        );
      }
      const inputInfos = [];
      for (let i = 0; i < inputTensors.length; ++i) {
        const type = inputTensors[i].dataType;
        switch (inputDependencies[i]) {
          case "none": {
            inputInfos.push("");
            break;
          }
          case "type": {
            inputInfos.push(`${type}`);
            break;
          }
          case "rank": {
            const rank = inputTensors[i].dims.length;
            inputInfos.push(`${type};${rank}`);
            break;
          }
          case "dims": {
            const dims = inputTensors[i].dims.join(",");
            inputInfos.push(`${type};${dims}`);
            break;
          }
          default:
            throw new Error(`unsupported input dependency: ${inputDependencies[i]}`);
        }
      }
      return inputInfos.join("|");
    };
    getProgramInfoUniqueKey = (programInfo, inputTensors, is1DimensionDispatch) => {
      let key = programInfo.name;
      if (programInfo.shaderCache?.hint) {
        key += "[" + programInfo.shaderCache.hint + "]";
      }
      key += ":" + is1DimensionDispatch + `:${getProgramInputTensorInfoDependencyKey(
        inputTensors,
        programInfo.shaderCache?.inputDependencies ?? new Array(inputTensors.length).fill("dims")
      )}`;
      return key;
    };
    AdapterInfoImpl = class {
      constructor(adapterInfo) {
        if (adapterInfo) {
          this.architecture = adapterInfo.architecture;
          this.vendor = adapterInfo.vendor;
        }
      }
      isArchitecture(architecture) {
        return this.architecture === architecture;
      }
      isVendor(vendor) {
        return this.vendor === vendor;
      }
    };
    WebGpuBackend = class {
      constructor() {
        /**
         * representing the session ID of which is currently being run.
         * `null` means no session is being run.
         * only valid when session.run is executed.
         */
        this.currentSessionId = null;
        /**
         * representing the kernel ID of which is currently being computed (CPU code perspective).
         * `null` means no kernel is being computed.
         * only one kernel can be computed at a moment.
         */
        this.currentKernelId = null;
        this.commandEncoder = null;
        this.computePassEncoder = null;
        this.maxDispatchNumber = 16;
        this.pendingDispatchNumber = 0;
        // info of kernels pending submission for a single batch
        this.pendingKernels = [];
        // queryReadBuffer -> pendingKernels mapping for all the batches
        this.pendingQueries = /* @__PURE__ */ new Map();
        this.sessionStatus = "default";
        /**
         * a SessionID -> CommandInfo[] mapping. It's used to record all GPU commands for corresponding session.
         */
        this.capturedCommandList = /* @__PURE__ */ new Map();
        /**
         * a SessionID -> PendingKernelInfo[] mapping for profiling.
         */
        this.capturedPendingKernels = /* @__PURE__ */ new Map();
        /**
         * a SessionID -> a Map of (InputOutputIndex -> [ID, GPUBuffer]) mapping.
         */
        this.sessionExternalDataMapping = /* @__PURE__ */ new Map();
      }
      /**
       * get the custom data of the current kernel
       */
      get currentKernelCustomData() {
        if (this.currentKernelId === null) {
          throw new Error("currentKernelCustomData(): currentKernelId is null. (should not happen)");
        }
        let data = this.kernelCustomData.get(this.currentKernelId);
        if (!data) {
          data = {};
          this.kernelCustomData.set(this.currentKernelId, data);
        }
        return data;
      }
      async initialize(env3, adapter) {
        this.env = env3;
        const requiredFeatures = [];
        const deviceDescriptor = {
          requiredLimits: {
            maxComputeWorkgroupStorageSize: adapter.limits.maxComputeWorkgroupStorageSize,
            maxComputeWorkgroupsPerDimension: adapter.limits.maxComputeWorkgroupsPerDimension,
            maxStorageBufferBindingSize: adapter.limits.maxStorageBufferBindingSize,
            maxBufferSize: adapter.limits.maxBufferSize,
            maxComputeInvocationsPerWorkgroup: adapter.limits.maxComputeInvocationsPerWorkgroup,
            maxComputeWorkgroupSizeX: adapter.limits.maxComputeWorkgroupSizeX,
            maxComputeWorkgroupSizeY: adapter.limits.maxComputeWorkgroupSizeY,
            maxComputeWorkgroupSizeZ: adapter.limits.maxComputeWorkgroupSizeZ
          },
          requiredFeatures
        };
        if (adapter.features.has("chromium-experimental-timestamp-query-inside-passes")) {
          requiredFeatures.push("chromium-experimental-timestamp-query-inside-passes");
        } else if (adapter.features.has("timestamp-query")) {
          requiredFeatures.push("timestamp-query");
        }
        if (adapter.features.has("shader-f16")) {
          requiredFeatures.push("shader-f16");
        }
        this.device = await adapter.requestDevice(deviceDescriptor);
        this.adapterInfo = new AdapterInfoImpl(adapter.info || await adapter.requestAdapterInfo());
        this.gpuDataManager = createGpuDataManager(this);
        this.programManager = new ProgramManager(this);
        this.kernels = /* @__PURE__ */ new Map();
        this.kernelPersistentData = /* @__PURE__ */ new Map();
        this.kernelCustomData = /* @__PURE__ */ new Map();
        configureLogger(env3.logLevel, !!env3.debug);
        this.device.onuncapturederror = (ev) => {
          if (ev.error instanceof GPUValidationError) {
            console.error(`An uncaught WebGPU validation error was raised: ${ev.error.message}`);
          }
        };
        Object.defineProperty(this.env.webgpu, "device", {
          value: this.device,
          writable: false,
          enumerable: true,
          configurable: false
        });
        Object.defineProperty(this.env.webgpu, "adapter", {
          value: adapter,
          writable: false,
          enumerable: true,
          configurable: false
        });
        this.setQueryType();
      }
      dispose() {
        if (typeof this.querySet !== "undefined") {
          this.querySet.destroy();
        }
        this.gpuDataManager.dispose();
      }
      getCommandEncoder() {
        if (!this.commandEncoder) {
          this.commandEncoder = this.device.createCommandEncoder();
        }
        return this.commandEncoder;
      }
      getComputePassEncoder() {
        if (!this.computePassEncoder) {
          const commandEncoder = this.getCommandEncoder();
          const computePassDescriptor = {};
          if (this.queryType === "at-passes") {
            computePassDescriptor.timestampWrites = {
              querySet: this.querySet,
              beginningOfPassWriteIndex: this.pendingDispatchNumber * 2,
              endOfPassWriteIndex: this.pendingDispatchNumber * 2 + 1
            };
          }
          this.computePassEncoder = commandEncoder.beginComputePass(computePassDescriptor);
        }
        return this.computePassEncoder;
      }
      endComputePass() {
        if (this.computePassEncoder) {
          this.computePassEncoder.end();
          this.computePassEncoder = null;
        }
      }
      flush() {
        if (!this.commandEncoder) {
          return;
        }
        TRACE_FUNC_BEGIN();
        this.endComputePass();
        let queryReadBuffer;
        if (this.queryType !== "none") {
          this.commandEncoder.resolveQuerySet(
            this.querySet,
            0,
            this.pendingDispatchNumber * 2,
            this.queryResolveBuffer,
            0
          );
          queryReadBuffer = this.device.createBuffer(
            // eslint-disable-next-line no-bitwise
            { size: this.pendingDispatchNumber * 2 * 8, usage: GPUBufferUsage.MAP_READ | GPUBufferUsage.COPY_DST }
          );
          this.pendingQueries.set(queryReadBuffer, this.pendingKernels);
          this.pendingKernels = [];
          this.commandEncoder.copyBufferToBuffer(
            this.queryResolveBuffer,
            0,
            queryReadBuffer,
            0,
            this.pendingDispatchNumber * 2 * 8
          );
        }
        this.device.queue.submit([this.commandEncoder.finish()]);
        this.gpuDataManager.refreshPendingBuffers();
        this.commandEncoder = null;
        this.pendingDispatchNumber = 0;
        if (this.queryType !== "none") {
          void queryReadBuffer.mapAsync(GPUMapMode.READ).then(() => {
            const mappedData = new BigUint64Array(queryReadBuffer.getMappedRange());
            const pendingKernels = this.pendingQueries.get(queryReadBuffer);
            for (let i = 0; i < mappedData.length / 2; i++) {
              const pendingKernelInfo = pendingKernels[i];
              const kernelId = pendingKernelInfo.kernelId;
              const kernelInfo = this.kernels.get(kernelId);
              const kernelType = kernelInfo.kernelType;
              const kernelName = kernelInfo.kernelName;
              const programName = pendingKernelInfo.programName;
              const inputTensorViews = pendingKernelInfo.inputTensorViews;
              const outputTensorViews = pendingKernelInfo.outputTensorViews;
              const startTimeU64 = mappedData[i * 2];
              const endTimeU64 = mappedData[i * 2 + 1];
              if (typeof this.queryTimeBase === "undefined") {
                this.queryTimeBase = startTimeU64;
              }
              const startTime = Number(startTimeU64 - this.queryTimeBase);
              const endTime = Number(endTimeU64 - this.queryTimeBase);
              if (!Number.isSafeInteger(startTime) || !Number.isSafeInteger(endTime)) {
                throw new RangeError("incorrect timestamp range");
              }
              if (this.env.webgpu.profiling?.ondata) {
                this.env.webgpu.profiling.ondata({
                  version: 1,
                  inputsMetadata: inputTensorViews.map((value) => ({
                    dims: value.dims,
                    dataType: tensorDataTypeEnumToString(value.dataType)
                  })),
                  outputsMetadata: outputTensorViews.map((value) => ({
                    dims: value.dims,
                    dataType: tensorDataTypeEnumToString(value.dataType)
                  })),
                  kernelId,
                  kernelType,
                  kernelName,
                  programName,
                  startTime,
                  endTime
                });
              } else {
                let inputShapes = "";
                inputTensorViews.forEach((value, i2) => {
                  inputShapes += `input[${i2}]: [${value.dims}] | ${tensorDataTypeEnumToString(value.dataType)}, `;
                });
                let outputShapes = "";
                outputTensorViews.forEach((value, i2) => {
                  outputShapes += `output[${i2}]: [${value.dims}] | ${tensorDataTypeEnumToString(value.dataType)}, `;
                });
                console.log(
                  `[profiling] kernel "${kernelId}|${kernelType}|${kernelName}|${programName}" ${inputShapes}${outputShapes}execution time: ${endTime - startTime} ns`
                );
              }
              TRACE("GPU", `${programName}::${startTimeU64}::${endTimeU64}`);
            }
            queryReadBuffer.unmap();
            this.pendingQueries.delete(queryReadBuffer);
          });
        }
        TRACE_FUNC_END();
      }
      /**
       * run a WebGPU program.
       * @param program a ProgramInfo instance
       * @param inputTensorViews a TensorView array. each element represents a value already exists in GPU.
       * @param outputIndices an indices array. each element can be either -1 (temporary data), -2 (persistent data) or an
       * index to the kernel's output.
       * @param createKernelOutput a callback function that create a value to kernel's output with the given index
       * @param createIntermediateOutput a callback function that create a value as a intermediate value, either temporary
       * or persistent (owned by the current kernel)
       * @returns a TensorView array representing the result.
       */
      run(program, inputTensorViews, outputIndices, createKernelOutput, createIntermediateOutput, outputCount) {
        TRACE_FUNC_BEGIN(program.name);
        const inputDatas = [];
        for (let i = 0; i < inputTensorViews.length; ++i) {
          const data = inputTensorViews[i].data;
          if (data === 0) {
            continue;
          }
          const gpuData = this.gpuDataManager.get(data);
          if (!gpuData) {
            throw new Error(`no GPU data for input: ${data}`);
          }
          inputDatas.push(gpuData);
        }
        const { outputs, dispatchGroup, programUniforms } = program.getRunData(inputTensorViews);
        const validatedOutputIndices = outputIndices.length === 0 ? outputs.map((_, i) => i) : outputIndices;
        if (validatedOutputIndices.length !== outputs.length) {
          throw new Error(`Output size ${validatedOutputIndices.length} must be equal to ${outputs.length}.`);
        }
        const outputTensorViews = [];
        const outputDatas = [];
        for (let i = 0; i < outputs.length; ++i) {
          if (!Number.isInteger(validatedOutputIndices[i]) || validatedOutputIndices[i] < -3 || validatedOutputIndices[i] >= outputCount) {
            throw new Error(`Invalid output index: ${validatedOutputIndices[i]}`);
          }
          if (validatedOutputIndices[i] === -3) {
            continue;
          }
          const isTemporary = validatedOutputIndices[i] === -1;
          const isPersistent = validatedOutputIndices[i] === -2;
          const tensorView = isTemporary || isPersistent ? createIntermediateOutput(outputs[i].dataType, outputs[i].dims) : createKernelOutput(validatedOutputIndices[i], outputs[i].dataType, outputs[i].dims);
          outputTensorViews.push(tensorView);
          if (tensorView.data === 0) {
            continue;
          }
          const gpuData = this.gpuDataManager.get(tensorView.data);
          if (!gpuData) {
            throw new Error(`no GPU data for output: ${tensorView.data}`);
          }
          if (isTemporary) {
            this.temporaryData.push(gpuData);
          }
          if (isPersistent) {
            let persistentData = this.kernelPersistentData.get(this.currentKernelId);
            if (!persistentData) {
              persistentData = [];
              this.kernelPersistentData.set(this.currentKernelId, persistentData);
            }
            persistentData.push(gpuData);
          }
          outputDatas.push(gpuData);
        }
        if (inputDatas.length !== inputTensorViews.length || outputDatas.length !== outputTensorViews.length) {
          if (outputDatas.length === 0) {
            TRACE_FUNC_END(program.name);
            return outputTensorViews;
          }
          throw new Error(
            `Program ${program.name} has zero-sized tensor(s) in inputs or outputs. This is not supported now.`
          );
        }
        let uniformBufferBinding;
        if (programUniforms) {
          let currentOffset = 0;
          const offsets = [];
          programUniforms.forEach((v) => {
            const data = typeof v.data === "number" ? [v.data] : v.data;
            if (data.length === 0) {
              return;
            }
            const sizeOfElement = v.type === 10 /* float16 */ ? 2 : 4;
            let sizeOfVecOrMat;
            let baseAlignment;
            if (v.type === 10 /* float16 */) {
              baseAlignment = data.length > 4 ? 16 : data.length > 2 ? 8 : data.length * sizeOfElement;
              sizeOfVecOrMat = data.length > 4 ? 16 : sizeOfElement * data.length;
            } else {
              baseAlignment = data.length <= 2 ? data.length * sizeOfElement : 16;
              sizeOfVecOrMat = 16;
            }
            currentOffset = Math.ceil(currentOffset / baseAlignment) * baseAlignment;
            offsets.push(currentOffset);
            const elementPerVecOrMat = v.type === 10 /* float16 */ ? 8 : 4;
            currentOffset += data.length > 4 ? Math.ceil(data.length / elementPerVecOrMat) * sizeOfVecOrMat : data.length * sizeOfElement;
          });
          const maxAlignmentOfField = 16;
          currentOffset = Math.ceil(currentOffset / maxAlignmentOfField) * maxAlignmentOfField;
          const arrayBuffer = new ArrayBuffer(currentOffset);
          programUniforms.forEach((v, i) => {
            const offset = offsets[i];
            const data = typeof v.data === "number" ? [v.data] : v.data;
            if (v.type === 6 /* int32 */) {
              new Int32Array(arrayBuffer, offset, data.length).set(data);
            } else if (v.type === 12 /* uint32 */) {
              new Uint32Array(arrayBuffer, offset, data.length).set(data);
            } else if (v.type === 10 /* float16 */) {
              new Uint16Array(arrayBuffer, offset, data.length).set(data);
            } else if (v.type === 1 /* float */) {
              new Float32Array(arrayBuffer, offset, data.length).set(data);
            } else {
              throw new Error(`Unsupported uniform type: ${tensorDataTypeEnumToString(v.type)}`);
            }
          });
          const uniformBufferData = (
            // eslint-disable-next-line no-bitwise
            this.gpuDataManager.create(currentOffset, GPUBufferUsage.COPY_DST | GPUBufferUsage.UNIFORM)
          );
          this.device.queue.writeBuffer(uniformBufferData.buffer, 0, arrayBuffer, 0, currentOffset);
          this.gpuDataManager.release(uniformBufferData.id);
          uniformBufferBinding = { offset: 0, size: currentOffset, buffer: uniformBufferData.buffer };
        }
        const normalizedDispatchGroup = this.programManager.normalizeDispatchGroupSize(dispatchGroup);
        const is1DimensionDispatch = normalizedDispatchGroup[1] === 1 && normalizedDispatchGroup[2] === 1;
        const key = getProgramInfoUniqueKey(program, inputTensorViews, is1DimensionDispatch);
        let artifact = this.programManager.getArtifact(key);
        if (!artifact) {
          artifact = this.programManager.build(program, normalizedDispatchGroup);
          this.programManager.setArtifact(key, artifact);
          LOG_DEBUG("info", () => `[artifact] key: ${key}, programName: ${program.name}`);
        }
        if (programUniforms && artifact.uniformVariablesInfo) {
          if (programUniforms.length !== artifact.uniformVariablesInfo.length) {
            throw new Error(
              `Uniform variables count mismatch: expect ${artifact.uniformVariablesInfo.length}, got ${programUniforms.length} in program "${artifact.programInfo.name}".`
            );
          }
          for (let i = 0; i < programUniforms.length; i++) {
            const uniform = programUniforms[i];
            const actualType = uniform.type;
            const actualLength = typeof uniform.data === "number" ? 1 : uniform.data.length;
            const [type, length] = artifact.uniformVariablesInfo[i];
            if (actualType !== type || actualLength !== length) {
              throw new Error(
                `Uniform variable ${i} mismatch: expect type ${type} with size ${length}, got type ${actualType} with size ${actualLength} in program "${artifact.programInfo.name}".`
              );
            }
          }
        }
        LOG_DEBUG(
          "info",
          () => `[ProgramManager] run "${program.name}" (key=${key}) with ${normalizedDispatchGroup[0]}x${normalizedDispatchGroup[1]}x${normalizedDispatchGroup[2]}`
        );
        if (this.queryType !== "none" || this.sessionStatus === "capturing") {
          const pendingKernelInfo = {
            kernelId: this.currentKernelId,
            programName: artifact.programInfo.name,
            inputTensorViews,
            outputTensorViews
          };
          this.pendingKernels.push(pendingKernelInfo);
          if (this.sessionStatus === "capturing") {
            const sessionPendingKernels = this.capturedPendingKernels.get(this.currentSessionId);
            sessionPendingKernels.push(pendingKernelInfo);
          }
        }
        this.programManager.run(artifact, inputDatas, outputDatas, normalizedDispatchGroup, uniformBufferBinding);
        TRACE_FUNC_END(program.name);
        return outputTensorViews;
      }
      upload(gpuDataId, data) {
        this.gpuDataManager.upload(gpuDataId, data);
      }
      memcpy(src, dst) {
        this.gpuDataManager.memcpy(src, dst);
      }
      async download(gpuDataId, getTargetBuffer) {
        await this.gpuDataManager.download(gpuDataId, getTargetBuffer);
      }
      alloc(size) {
        return this.gpuDataManager.create(size).id;
      }
      free(ptr) {
        return this.gpuDataManager.release(ptr);
      }
      createKernel(kernelType, kernelId, attribute, kernelName) {
        const op = WEBGPU_OP_RESOLVE_RULES.get(kernelType);
        if (!op) {
          throw new Error(`kernel not implemented: ${kernelType}`);
        }
        const kernelInfo = {
          kernelType,
          kernelName,
          kernelEntry: op[0],
          attributes: [op[1], attribute]
        };
        this.kernels.set(kernelId, kernelInfo);
      }
      releaseKernel(kernelId) {
        const persistentData = this.kernelPersistentData.get(kernelId);
        if (persistentData) {
          for (const data of persistentData) {
            this.gpuDataManager.release(data.id);
          }
          this.kernelPersistentData.delete(kernelId);
        }
        this.kernelCustomData.delete(kernelId);
        this.kernels.delete(kernelId);
      }
      computeKernel(kernelId, context, errors) {
        const kernel = this.kernels.get(kernelId);
        if (!kernel) {
          throw new Error(`kernel not created: ${kernelId}`);
        }
        const kernelType = kernel.kernelType;
        const kernelName = kernel.kernelName;
        const kernelEntry = kernel.kernelEntry;
        const attributes = kernel.attributes;
        if (this.currentKernelId !== null) {
          throw new Error(`kernel "[${kernelType}] ${kernelName}" is not allowed to be called recursively`);
        }
        this.currentKernelId = kernelId;
        if (attributes[0]) {
          attributes[1] = attributes[0](attributes[1]);
          attributes[0] = void 0;
        }
        LOG_DEBUG("info", () => `[WebGPU] Start to run kernel "[${kernelType}] ${kernelName}"...`);
        const useErrorScope = this.env.debug;
        this.temporaryData = [];
        try {
          if (useErrorScope) {
            this.device.pushErrorScope("validation");
          }
          kernelEntry(context, attributes[1]);
          return 0;
        } catch (e) {
          errors.push(Promise.resolve(`[WebGPU] Kernel "[${kernelType}] ${kernelName}" failed. ${e}`));
          return 1;
        } finally {
          if (useErrorScope) {
            errors.push(
              this.device.popErrorScope().then(
                (err) => err ? `GPU validation error for kernel "[${kernelType}] ${kernelName}": ${err.message}` : null
              )
            );
          }
          for (const data of this.temporaryData) {
            this.gpuDataManager.release(data.id);
          }
          this.temporaryData = [];
          this.currentKernelId = null;
        }
      }
      // #region external buffer
      registerBuffer(sessionId, index, buffer, size) {
        let sessionInputOutputMapping = this.sessionExternalDataMapping.get(sessionId);
        if (!sessionInputOutputMapping) {
          sessionInputOutputMapping = /* @__PURE__ */ new Map();
          this.sessionExternalDataMapping.set(sessionId, sessionInputOutputMapping);
        }
        const previousBuffer = sessionInputOutputMapping.get(index);
        const id = this.gpuDataManager.registerExternalBuffer(buffer, size, previousBuffer?.[1]);
        sessionInputOutputMapping.set(index, [id, buffer]);
        return id;
      }
      unregisterBuffers(sessionId) {
        const sessionInputOutputMapping = this.sessionExternalDataMapping.get(sessionId);
        if (sessionInputOutputMapping) {
          sessionInputOutputMapping.forEach((bufferInfo) => this.gpuDataManager.unregisterExternalBuffer(bufferInfo[1]));
          this.sessionExternalDataMapping.delete(sessionId);
        }
      }
      getBuffer(gpuDataId) {
        const gpuData = this.gpuDataManager.get(gpuDataId);
        if (!gpuData) {
          throw new Error(`no GPU data for buffer: ${gpuDataId}`);
        }
        return gpuData.buffer;
      }
      createDownloader(gpuBuffer, size, type) {
        return async () => {
          const data = await downloadGpuData(this, gpuBuffer, size);
          return createView(data.buffer, type);
        };
      }
      // #endregion
      writeTimestamp(index) {
        if (this.queryType !== "inside-passes") {
          return;
        }
        this.computePassEncoder.writeTimestamp(this.querySet, index);
      }
      setQueryType() {
        this.queryType = "none";
        if (this.env.webgpu.profiling?.mode === "default" || (typeof this.env.trace === "undefined" ? this.env.wasm.trace : this.env.trace)) {
          if (this.device.features.has("chromium-experimental-timestamp-query-inside-passes")) {
            this.queryType = "inside-passes";
          } else if (this.device.features.has("timestamp-query")) {
            this.queryType = "at-passes";
          }
          if (this.queryType !== "none" && typeof this.querySet === "undefined") {
            this.querySet = this.device.createQuerySet({
              type: "timestamp",
              count: this.maxDispatchNumber * 2
            });
            this.queryResolveBuffer = this.device.createBuffer(
              // eslint-disable-next-line no-bitwise
              { size: this.maxDispatchNumber * 2 * 8, usage: GPUBufferUsage.COPY_SRC | GPUBufferUsage.QUERY_RESOLVE }
            );
          }
        }
      }
      captureBegin() {
        LOG_DEBUG("info", "captureBegin");
        if (!this.capturedCommandList.get(this.currentSessionId)) {
          this.capturedCommandList.set(this.currentSessionId, []);
        }
        if (!this.capturedPendingKernels.get(this.currentSessionId)) {
          this.capturedPendingKernels.set(this.currentSessionId, []);
        }
        this.flush();
        this.sessionStatus = "capturing";
      }
      captureEnd() {
        LOG_DEBUG("info", "captureEnd");
        this.flush();
        this.sessionStatus = "default";
      }
      replay() {
        LOG_DEBUG("info", "replay");
        this.sessionStatus = "replaying";
        const sessionCommandList = this.capturedCommandList.get(this.currentSessionId);
        const sessionPendingKernels = this.capturedPendingKernels.get(this.currentSessionId);
        const length = sessionCommandList.length;
        this.pendingKernels = [];
        for (let i = 0; i < length; i++) {
          const computePassEncoder = this.getComputePassEncoder();
          const command = sessionCommandList[i];
          this.writeTimestamp(this.pendingDispatchNumber * 2);
          computePassEncoder.setPipeline(command.computePipeline);
          computePassEncoder.setBindGroup(0, command.bindGroup);
          computePassEncoder.dispatchWorkgroups(...command.dispatchGroup);
          this.writeTimestamp(this.pendingDispatchNumber * 2 + 1);
          this.pendingDispatchNumber++;
          if (this.queryType !== "none") {
            this.pendingKernels.push(sessionPendingKernels[i]);
          }
          if (this.pendingDispatchNumber >= this.maxDispatchNumber || this.queryType === "at-passes") {
            this.endComputePass();
          }
          if (this.pendingDispatchNumber >= this.maxDispatchNumber) {
            this.flush();
          }
        }
        this.flush();
        this.sessionStatus = "default";
      }
      onReleaseSession(sessionId) {
        this.unregisterBuffers(sessionId);
        if (this.capturedCommandList.has(sessionId)) {
          this.capturedCommandList.delete(sessionId);
        }
        if (this.capturedPendingKernels.has(sessionId)) {
          this.capturedPendingKernels.delete(sessionId);
        }
        this.gpuDataManager.onReleaseSession(sessionId);
      }
      onRunStart(sessionId) {
        this.currentSessionId = sessionId;
        this.setQueryType();
      }
    };
  }
});

// web/lib/wasm/jsep/init.ts
var init_exports = {};
__export(init_exports, {
  init: () => init
});
var TensorViewImpl, ComputeContextImpl, init;
var init_init = __esm({
  "web/lib/wasm/jsep/init.ts"() {
    "use strict";
    init_wasm_common();
    init_backend_webgpu();
    init_log();
    init_util();
    TensorViewImpl = class _TensorViewImpl {
      constructor(module, dataType, data, dims) {
        this.module = module;
        this.dataType = dataType;
        this.data = data;
        this.dims = dims;
      }
      getUint16Array() {
        if (this.dataType !== 10 /* float16 */ && this.dataType !== 4 /* uint16 */) {
          throw new Error("Invalid data type");
        }
        const elementCount = ShapeUtil.size(this.dims);
        return elementCount === 0 ? new Uint16Array() : new Uint16Array(this.module.HEAP8.buffer, this.data, elementCount);
      }
      getFloat32Array() {
        if (this.dataType !== 1 /* float */) {
          throw new Error("Invalid data type");
        }
        const elementCount = ShapeUtil.size(this.dims);
        return elementCount === 0 ? new Float32Array() : new Float32Array(this.module.HEAP8.buffer, this.data, elementCount);
      }
      getBigInt64Array() {
        if (this.dataType !== 7 /* int64 */) {
          throw new Error("Invalid data type");
        }
        const elementCount = ShapeUtil.size(this.dims);
        return elementCount === 0 ? new BigInt64Array() : new BigInt64Array(this.module.HEAP8.buffer, this.data, elementCount);
      }
      getInt32Array() {
        if (this.dataType !== 6 /* int32 */) {
          throw new Error("Invalid data type");
        }
        const elementCount = ShapeUtil.size(this.dims);
        return elementCount === 0 ? new Int32Array() : new Int32Array(this.module.HEAP8.buffer, this.data, elementCount);
      }
      reshape(newDims) {
        if (ShapeUtil.size(newDims) !== ShapeUtil.size(this.dims)) {
          throw new Error("Invalid new shape");
        }
        return new _TensorViewImpl(this.module, this.dataType, this.data, newDims);
      }
    };
    ComputeContextImpl = class {
      constructor(module, backend, contextDataOffset) {
        this.module = module;
        this.backend = backend;
        this.customDataOffset = 0;
        this.customDataSize = 0;
        this.adapterInfo = backend.adapterInfo;
        const heapU32 = module.HEAPU32;
        let dataIndex = contextDataOffset >>> 2;
        this.opKernelContext = heapU32[dataIndex++];
        const inputCount = heapU32[dataIndex++];
        this.outputCount = heapU32[dataIndex++];
        this.customDataOffset = heapU32[dataIndex++];
        this.customDataSize = heapU32[dataIndex++];
        const inputs = [];
        for (let i = 0; i < inputCount; i++) {
          const dataType = heapU32[dataIndex++];
          const data = heapU32[dataIndex++];
          const dim = heapU32[dataIndex++];
          const dims = [];
          for (let d = 0; d < dim; d++) {
            dims.push(heapU32[dataIndex++]);
          }
          inputs.push(new TensorViewImpl(module, dataType, data, dims));
        }
        this.inputs = inputs;
      }
      get kernelCustomData() {
        return this.backend.currentKernelCustomData;
      }
      get customDataBuffer() {
        return this.module.HEAPU8.subarray(this.customDataOffset, this.customDataOffset + this.customDataSize);
      }
      getMaxComputeWorkgroupSizes() {
        return [
          this.backend.device.limits.maxComputeWorkgroupSizeX,
          this.backend.device.limits.maxComputeWorkgroupSizeY,
          this.backend.device.limits.maxComputeWorkgroupSizeZ
        ];
      }
      getMaxComputeWorkgroupStoragesize() {
        return this.backend.device.limits.maxComputeWorkgroupStorageSize;
      }
      compute(program, inputsOutputsMapping) {
        const mappedInputs = inputsOutputsMapping?.inputs?.map((i) => typeof i === "number" ? this.inputs[i] : i) ?? this.inputs;
        const outputIndices = inputsOutputsMapping?.outputs ?? [];
        const createKernelOutput = (index, dataType, dims) => new TensorViewImpl(this.module, dataType, this.output(index, dims), dims);
        const createTemporaryOutput = (dataType, dims) => {
          const bufferSize = calculateTensorSizeInBytes(dataType, dims);
          if (!bufferSize) {
            throw new Error(`Unsupported data type: ${dataType}`);
          }
          const gpuDataId = bufferSize > 0 ? this.backend.gpuDataManager.create(bufferSize).id : 0;
          return new TensorViewImpl(this.module, dataType, gpuDataId, dims);
        };
        return this.backend.run(
          program,
          mappedInputs,
          outputIndices,
          createKernelOutput,
          createTemporaryOutput,
          this.outputCount
        );
      }
      output(index, dims) {
        const stack = this.module.stackSave();
        try {
          const data = this.module.stackAlloc(
            (1 + dims.length) * 4
            /* sizeof(size_t) */
          );
          let offset = data >> 2;
          this.module.HEAPU32[offset++] = dims.length;
          for (let i = 0; i < dims.length; i++) {
            this.module.HEAPU32[offset++] = dims[i];
          }
          return this.module._JsepOutput(this.opKernelContext, index, data);
        } catch (e) {
          throw new Error(
            `Failed to generate kernel's output[${index}] with dims [${dims}]. If you are running with pre-allocated output, please make sure the output type/dims are correct. Error: ${e}`
          );
        } finally {
          this.module.stackRestore(stack);
        }
      }
    };
    init = async (name, module, env3, gpuAdapter) => {
      const jsepInit = module.jsepInit;
      if (!jsepInit) {
        throw new Error("Failed to initialize JSEP. The WebAssembly module is not built with JSEP support.");
      }
      if (name === "webgpu") {
        const backend = new WebGpuBackend();
        await backend.initialize(env3, gpuAdapter);
        jsepInit("webgpu", [
          // backend
          backend,
          // jsepAlloc()
          (size) => backend.alloc(size),
          // jsepFree()
          (ptr) => backend.free(ptr),
          // jsepCopy(src, dst, size, isSourceGpu)
          (src, dst, size, isSourceGpu = false) => {
            if (isSourceGpu) {
              LOG_DEBUG("verbose", () => `[WebGPU] jsepCopyGpuToGpu: src=${src}, dst=${dst}, size=${size}`);
              backend.memcpy(src, dst);
            } else {
              LOG_DEBUG("verbose", () => `[WebGPU] jsepCopyCpuToGpu: dataOffset=${src}, gpuDataId=${dst}, size=${size}`);
              const data = module.HEAPU8.subarray(src >>> 0, (src >>> 0) + size);
              backend.upload(dst, data);
            }
          },
          // jsepCopyAsync(src, dst, size)
          async (gpuDataId, dataOffset, size) => {
            LOG_DEBUG(
              "verbose",
              () => `[WebGPU] jsepCopyGpuToCpu: gpuDataId=${gpuDataId}, dataOffset=${dataOffset}, size=${size}`
            );
            await backend.download(gpuDataId, () => module.HEAPU8.subarray(dataOffset >>> 0, (dataOffset >>> 0) + size));
          },
          // jsepCreateKernel
          (kernelType, kernelId, attribute) => backend.createKernel(kernelType, kernelId, attribute, module.UTF8ToString(module._JsepGetNodeName(kernelId))),
          // jsepReleaseKernel
          (kernel) => backend.releaseKernel(kernel),
          // jsepRun
          (kernel, contextDataOffset, sessionHandle, errors) => {
            LOG_DEBUG(
              "verbose",
              () => `[WebGPU] jsepRun: sessionHandle=${sessionHandle}, kernel=${kernel}, contextDataOffset=${contextDataOffset}`
            );
            const context = new ComputeContextImpl(module, backend, contextDataOffset);
            return backend.computeKernel(kernel, context, errors);
          },
          // jsepCaptureBegin
          () => backend.captureBegin(),
          // jsepCaptureEnd
          () => backend.captureEnd(),
          // jsepReplay
          () => backend.replay()
        ]);
      } else {
        jsepInit("webnn");
      }
    };
  }
});

// web/lib/wasm/wasm-core-impl.ts
var initOrt, initRuntime, initEp, activeSessions, getSessionInputOutputCount, copyFromExternalBuffer, createSession, releaseSession, prepareInputOutputTensor, run, endProfiling, extractTransferableBuffers;
var init_wasm_core_impl = __esm({
  "web/lib/wasm/wasm-core-impl.ts"() {
    "use strict";
    init_run_options();
    init_session_options();
    init_wasm_common();
    init_wasm_factory();
    init_wasm_utils();
    init_wasm_utils_load_file();
    initOrt = (numThreads, loggingLevel) => {
      const errorCode = getInstance()._OrtInit(numThreads, loggingLevel);
      if (errorCode !== 0) {
        checkLastError("Can't initialize onnxruntime.");
      }
    };
    initRuntime = async (env3) => {
      initOrt(env3.wasm.numThreads, logLevelStringToEnum(env3.logLevel));
    };
    initEp = async (env3, epName) => {
      if (true) {
        const initJsep = (init_init(), __toCommonJS(init_exports)).init;
        if (epName === "webgpu") {
          if (typeof navigator === "undefined" || !navigator.gpu) {
            throw new Error("WebGPU is not supported in current environment");
          }
          let adapter = env3.webgpu.adapter;
          if (!adapter) {
            const powerPreference = env3.webgpu.powerPreference;
            if (powerPreference !== void 0 && powerPreference !== "low-power" && powerPreference !== "high-performance") {
              throw new Error(`Invalid powerPreference setting: "${powerPreference}"`);
            }
            const forceFallbackAdapter = env3.webgpu.forceFallbackAdapter;
            if (forceFallbackAdapter !== void 0 && typeof forceFallbackAdapter !== "boolean") {
              throw new Error(`Invalid forceFallbackAdapter setting: "${forceFallbackAdapter}"`);
            }
            adapter = await navigator.gpu.requestAdapter({ powerPreference, forceFallbackAdapter });
            if (!adapter) {
              throw new Error(
                'Failed to get GPU adapter. You may need to enable flag "--enable-unsafe-webgpu" if you are using Chrome.'
              );
            }
          } else {
            if (typeof adapter.limits !== "object" || typeof adapter.features !== "object" || typeof adapter.requestDevice !== "function") {
              throw new Error("Invalid GPU adapter set in `env.webgpu.adapter`. It must be a GPUAdapter object.");
            }
          }
          await initJsep("webgpu", getInstance(), env3, adapter);
        }
        if (epName === "webnn") {
          if (typeof navigator === "undefined" || !navigator.ml) {
            throw new Error("WebNN is not supported in current environment");
          }
          await initJsep("webnn", getInstance(), env3);
        }
      }
    };
    activeSessions = /* @__PURE__ */ new Map();
    getSessionInputOutputCount = (sessionHandle) => {
      const wasm2 = getInstance();
      const stack = wasm2.stackSave();
      try {
        const dataOffset = wasm2.stackAlloc(8);
        const errorCode = wasm2._OrtGetInputOutputCount(sessionHandle, dataOffset, dataOffset + 4);
        if (errorCode !== 0) {
          checkLastError("Can't get session input/output count.");
        }
        return [wasm2.HEAP32[dataOffset / 4], wasm2.HEAP32[dataOffset / 4 + 1]];
      } finally {
        wasm2.stackRestore(stack);
      }
    };
    copyFromExternalBuffer = (model) => {
      const wasm2 = getInstance();
      const modelDataOffset = wasm2._malloc(model.byteLength);
      if (modelDataOffset === 0) {
        throw new Error(`Can't create a session. failed to allocate a buffer of size ${model.byteLength}.`);
      }
      wasm2.HEAPU8.set(model, modelDataOffset);
      return [modelDataOffset, model.byteLength];
    };
    createSession = async (modelData, options) => {
      let modelDataOffset, modelDataLength;
      const wasm2 = getInstance();
      if (Array.isArray(modelData)) {
        [modelDataOffset, modelDataLength] = modelData;
      } else if (modelData.buffer === wasm2.HEAPU8.buffer) {
        [modelDataOffset, modelDataLength] = [modelData.byteOffset, modelData.byteLength];
      } else {
        [modelDataOffset, modelDataLength] = copyFromExternalBuffer(modelData);
      }
      let sessionHandle = 0;
      let sessionOptionsHandle = 0;
      let ioBindingHandle = 0;
      let allocs = [];
      const inputNamesUTF8Encoded = [];
      const outputNamesUTF8Encoded = [];
      try {
        [sessionOptionsHandle, allocs] = setSessionOptions(options);
        if (options?.externalData && wasm2.mountExternalData) {
          const loadingPromises = [];
          for (const file of options.externalData) {
            const path = typeof file === "string" ? file : file.path;
            loadingPromises.push(
              loadFile(typeof file === "string" ? file : file.data).then((data) => {
                wasm2.mountExternalData(path, data);
              })
            );
          }
          await Promise.all(loadingPromises);
        }
        for (const provider of options?.executionProviders ?? []) {
          const providerName = typeof provider === "string" ? provider : provider.name;
          if (providerName === "webnn") {
            if (wasm2.currentContext) {
              throw new Error("WebNN execution provider is already set.");
            }
            if (typeof provider !== "string") {
              const webnnOptions = provider;
              const context = webnnOptions?.context;
              const gpuDevice = webnnOptions?.gpuDevice;
              const deviceType = webnnOptions?.deviceType;
              const numThreads = webnnOptions?.numThreads;
              const powerPreference = webnnOptions?.powerPreference;
              if (context) {
                wasm2.currentContext = context;
              } else if (gpuDevice) {
                wasm2.currentContext = await navigator.ml.createContext(gpuDevice);
              } else {
                wasm2.currentContext = await navigator.ml.createContext({ deviceType, numThreads, powerPreference });
              }
            } else {
              wasm2.currentContext = await navigator.ml.createContext();
            }
            break;
          }
        }
        sessionHandle = await wasm2._OrtCreateSession(modelDataOffset, modelDataLength, sessionOptionsHandle);
        if (sessionHandle === 0) {
          checkLastError("Can't create a session.");
        }
        if (wasm2.currentContext) {
          wasm2.currentContext = void 0;
        }
        const [inputCount, outputCount] = getSessionInputOutputCount(sessionHandle);
        const enableGraphCapture = !!options?.enableGraphCapture;
        const inputNames = [];
        const outputNames = [];
        const outputPreferredLocations = [];
        for (let i = 0; i < inputCount; i++) {
          const name = wasm2._OrtGetInputName(sessionHandle, i);
          if (name === 0) {
            checkLastError("Can't get an input name.");
          }
          inputNamesUTF8Encoded.push(name);
          inputNames.push(wasm2.UTF8ToString(name));
        }
        for (let i = 0; i < outputCount; i++) {
          const name = wasm2._OrtGetOutputName(sessionHandle, i);
          if (name === 0) {
            checkLastError("Can't get an output name.");
          }
          outputNamesUTF8Encoded.push(name);
          const nameString = wasm2.UTF8ToString(name);
          outputNames.push(nameString);
          if (true) {
            if (enableGraphCapture && options?.preferredOutputLocation === void 0) {
              outputPreferredLocations.push("gpu-buffer");
              continue;
            }
            const location2 = typeof options?.preferredOutputLocation === "string" ? options.preferredOutputLocation : options?.preferredOutputLocation?.[nameString] ?? "cpu";
            if (location2 !== "cpu" && location2 !== "cpu-pinned" && location2 !== "gpu-buffer") {
              throw new Error(`Not supported preferred output location: ${location2}.`);
            }
            if (enableGraphCapture && location2 !== "gpu-buffer") {
              throw new Error(
                `Not supported preferred output location: ${location2}. Only 'gpu-buffer' location is supported when enableGraphCapture is true.`
              );
            }
            outputPreferredLocations.push(location2);
          }
        }
        let bindingState = null;
        if (outputPreferredLocations.some((l) => l === "gpu-buffer")) {
          ioBindingHandle = wasm2._OrtCreateBinding(sessionHandle);
          if (ioBindingHandle === 0) {
            checkLastError("Can't create IO binding.");
          }
          bindingState = {
            handle: ioBindingHandle,
            outputPreferredLocations,
            outputPreferredLocationsEncoded: outputPreferredLocations.map((l) => dataLocationStringToEnum(l))
          };
        }
        activeSessions.set(sessionHandle, [
          sessionHandle,
          inputNamesUTF8Encoded,
          outputNamesUTF8Encoded,
          bindingState,
          enableGraphCapture,
          false
        ]);
        return [sessionHandle, inputNames, outputNames];
      } catch (e) {
        inputNamesUTF8Encoded.forEach((buf) => wasm2._OrtFree(buf));
        outputNamesUTF8Encoded.forEach((buf) => wasm2._OrtFree(buf));
        if (ioBindingHandle !== 0) {
          wasm2._OrtReleaseBinding(ioBindingHandle);
        }
        if (sessionHandle !== 0) {
          wasm2._OrtReleaseSession(sessionHandle);
        }
        throw e;
      } finally {
        wasm2._free(modelDataOffset);
        if (sessionOptionsHandle !== 0) {
          wasm2._OrtReleaseSessionOptions(sessionOptionsHandle);
        }
        allocs.forEach((alloc) => wasm2._free(alloc));
        wasm2.unmountExternalData?.();
      }
    };
    releaseSession = (sessionId) => {
      const wasm2 = getInstance();
      const session = activeSessions.get(sessionId);
      if (!session) {
        throw new Error(`cannot release session. invalid session id: ${sessionId}`);
      }
      const [sessionHandle, inputNamesUTF8Encoded, outputNamesUTF8Encoded, ioBindingState, enableGraphCapture] = session;
      if (ioBindingState) {
        if (enableGraphCapture) {
          wasm2._OrtClearBoundOutputs(ioBindingState.handle);
        }
        wasm2._OrtReleaseBinding(ioBindingState.handle);
      }
      wasm2.jsepOnReleaseSession?.(sessionId);
      inputNamesUTF8Encoded.forEach((buf) => wasm2._OrtFree(buf));
      outputNamesUTF8Encoded.forEach((buf) => wasm2._OrtFree(buf));
      wasm2._OrtReleaseSession(sessionHandle);
      activeSessions.delete(sessionId);
    };
    prepareInputOutputTensor = (tensor, tensorHandles, allocs, sessionId, index, enableGraphCapture = false) => {
      if (!tensor) {
        tensorHandles.push(0);
        return;
      }
      const wasm2 = getInstance();
      const dataType = tensor[0];
      const dims = tensor[1];
      const location2 = tensor[3];
      let rawData;
      let dataByteLength;
      if (dataType === "string" && location2 === "gpu-buffer") {
        throw new Error("String tensor is not supported on GPU.");
      }
      if (enableGraphCapture && location2 !== "gpu-buffer") {
        throw new Error(
          `External buffer must be provided for input/output index ${index} when enableGraphCapture is true.`
        );
      }
      if (location2 === "gpu-buffer") {
        const gpuBuffer = tensor[2].gpuBuffer;
        dataByteLength = calculateTensorSizeInBytes(tensorDataTypeStringToEnum(dataType), dims);
        const registerBuffer = wasm2.jsepRegisterBuffer;
        if (!registerBuffer) {
          throw new Error('Tensor location "gpu-buffer" is not supported without using WebGPU.');
        }
        rawData = registerBuffer(sessionId, index, gpuBuffer, dataByteLength);
      } else {
        const data = tensor[2];
        if (Array.isArray(data)) {
          dataByteLength = 4 * data.length;
          rawData = wasm2._malloc(dataByteLength);
          allocs.push(rawData);
          let dataIndex = rawData / 4;
          for (let i = 0; i < data.length; i++) {
            if (typeof data[i] !== "string") {
              throw new TypeError(`tensor data at index ${i} is not a string`);
            }
            wasm2.HEAPU32[dataIndex++] = allocWasmString(data[i], allocs);
          }
        } else {
          dataByteLength = data.byteLength;
          rawData = wasm2._malloc(dataByteLength);
          allocs.push(rawData);
          wasm2.HEAPU8.set(new Uint8Array(data.buffer, data.byteOffset, dataByteLength), rawData);
        }
      }
      const stack = wasm2.stackSave();
      const dimsOffset = wasm2.stackAlloc(4 * dims.length);
      try {
        let dimIndex = dimsOffset / 4;
        dims.forEach((d) => wasm2.HEAP32[dimIndex++] = d);
        const tensor2 = wasm2._OrtCreateTensor(
          tensorDataTypeStringToEnum(dataType),
          rawData,
          dataByteLength,
          dimsOffset,
          dims.length,
          dataLocationStringToEnum(location2)
        );
        if (tensor2 === 0) {
          checkLastError(`Can't create tensor for input/output. session=${sessionId}, index=${index}.`);
        }
        tensorHandles.push(tensor2);
      } finally {
        wasm2.stackRestore(stack);
      }
    };
    run = async (sessionId, inputIndices, inputTensors, outputIndices, outputTensors, options) => {
      const wasm2 = getInstance();
      const session = activeSessions.get(sessionId);
      if (!session) {
        throw new Error(`cannot run inference. invalid session id: ${sessionId}`);
      }
      const sessionHandle = session[0];
      const inputNamesUTF8Encoded = session[1];
      const outputNamesUTF8Encoded = session[2];
      const ioBindingState = session[3];
      const enableGraphCapture = session[4];
      const inputOutputBound = session[5];
      const inputCount = inputIndices.length;
      const outputCount = outputIndices.length;
      let runOptionsHandle = 0;
      let runOptionsAllocs = [];
      const inputTensorHandles = [];
      const outputTensorHandles = [];
      const inputOutputAllocs = [];
      const beforeRunStack = wasm2.stackSave();
      const inputValuesOffset = wasm2.stackAlloc(inputCount * 4);
      const inputNamesOffset = wasm2.stackAlloc(inputCount * 4);
      const outputValuesOffset = wasm2.stackAlloc(outputCount * 4);
      const outputNamesOffset = wasm2.stackAlloc(outputCount * 4);
      try {
        [runOptionsHandle, runOptionsAllocs] = setRunOptions(options);
        for (let i = 0; i < inputCount; i++) {
          prepareInputOutputTensor(
            inputTensors[i],
            inputTensorHandles,
            inputOutputAllocs,
            sessionId,
            inputIndices[i],
            enableGraphCapture
          );
        }
        for (let i = 0; i < outputCount; i++) {
          prepareInputOutputTensor(
            outputTensors[i],
            outputTensorHandles,
            inputOutputAllocs,
            sessionId,
            inputCount + outputIndices[i],
            enableGraphCapture
          );
        }
        let inputValuesIndex = inputValuesOffset / 4;
        let inputNamesIndex = inputNamesOffset / 4;
        let outputValuesIndex = outputValuesOffset / 4;
        let outputNamesIndex = outputNamesOffset / 4;
        for (let i = 0; i < inputCount; i++) {
          wasm2.HEAPU32[inputValuesIndex++] = inputTensorHandles[i];
          wasm2.HEAPU32[inputNamesIndex++] = inputNamesUTF8Encoded[inputIndices[i]];
        }
        for (let i = 0; i < outputCount; i++) {
          wasm2.HEAPU32[outputValuesIndex++] = outputTensorHandles[i];
          wasm2.HEAPU32[outputNamesIndex++] = outputNamesUTF8Encoded[outputIndices[i]];
        }
        if (ioBindingState && !inputOutputBound) {
          const { handle, outputPreferredLocations, outputPreferredLocationsEncoded } = ioBindingState;
          if (inputNamesUTF8Encoded.length !== inputCount) {
            throw new Error(
              `input count from feeds (${inputCount}) is expected to be always equal to model's input count (${inputNamesUTF8Encoded.length}).`
            );
          }
          for (let i = 0; i < inputCount; i++) {
            const index = inputIndices[i];
            const errorCode2 = await wasm2._OrtBindInput(handle, inputNamesUTF8Encoded[index], inputTensorHandles[i]);
            if (errorCode2 !== 0) {
              checkLastError(`Can't bind input[${i}] for session=${sessionId}.`);
            }
          }
          for (let i = 0; i < outputCount; i++) {
            const index = outputIndices[i];
            const location2 = outputTensors[i]?.[3];
            if (location2) {
              const errorCode2 = wasm2._OrtBindOutput(handle, outputNamesUTF8Encoded[index], outputTensorHandles[i], 0);
              if (errorCode2 !== 0) {
                checkLastError(`Can't bind pre-allocated output[${i}] for session=${sessionId}.`);
              }
            } else {
              const errorCode2 = wasm2._OrtBindOutput(
                handle,
                outputNamesUTF8Encoded[index],
                0,
                outputPreferredLocationsEncoded[index]
              );
              if (errorCode2 !== 0) {
                checkLastError(`Can't bind output[${i}] to ${outputPreferredLocations[i]} for session=${sessionId}.`);
              }
            }
          }
          activeSessions.set(sessionId, [
            sessionHandle,
            inputNamesUTF8Encoded,
            outputNamesUTF8Encoded,
            ioBindingState,
            enableGraphCapture,
            true
          ]);
        }
        wasm2.jsepOnRunStart?.(sessionHandle);
        let errorCode;
        if (ioBindingState) {
          errorCode = await wasm2._OrtRunWithBinding(
            sessionHandle,
            ioBindingState.handle,
            outputCount,
            outputValuesOffset,
            runOptionsHandle
          );
        } else {
          errorCode = await wasm2._OrtRun(
            sessionHandle,
            inputNamesOffset,
            inputValuesOffset,
            inputCount,
            outputNamesOffset,
            outputCount,
            outputValuesOffset,
            runOptionsHandle
          );
        }
        if (errorCode !== 0) {
          checkLastError("failed to call OrtRun().");
        }
        const output = [];
        for (let i = 0; i < outputCount; i++) {
          const tensor = wasm2.HEAPU32[outputValuesOffset / 4 + i];
          if (tensor === outputTensorHandles[i]) {
            output.push(outputTensors[i]);
            continue;
          }
          const beforeGetTensorDataStack = wasm2.stackSave();
          const tensorDataOffset = wasm2.stackAlloc(4 * 4);
          let keepOutputTensor = false;
          let type, dataOffset = 0;
          try {
            const errorCode2 = wasm2._OrtGetTensorData(
              tensor,
              tensorDataOffset,
              tensorDataOffset + 4,
              tensorDataOffset + 8,
              tensorDataOffset + 12
            );
            if (errorCode2 !== 0) {
              checkLastError(`Can't access output tensor data on index ${i}.`);
            }
            let tensorDataIndex = tensorDataOffset / 4;
            const dataType = wasm2.HEAPU32[tensorDataIndex++];
            dataOffset = wasm2.HEAPU32[tensorDataIndex++];
            const dimsOffset = wasm2.HEAPU32[tensorDataIndex++];
            const dimsLength = wasm2.HEAPU32[tensorDataIndex++];
            const dims = [];
            for (let i2 = 0; i2 < dimsLength; i2++) {
              dims.push(wasm2.HEAPU32[dimsOffset / 4 + i2]);
            }
            wasm2._OrtFree(dimsOffset);
            const size = dims.reduce((a, b) => a * b, 1);
            type = tensorDataTypeEnumToString(dataType);
            const preferredLocation = ioBindingState?.outputPreferredLocations[outputIndices[i]];
            if (type === "string") {
              if (preferredLocation === "gpu-buffer") {
                throw new Error("String tensor is not supported on GPU.");
              }
              const stringData = [];
              let dataIndex = dataOffset / 4;
              for (let i2 = 0; i2 < size; i2++) {
                const offset = wasm2.HEAPU32[dataIndex++];
                const maxBytesToRead = i2 === size - 1 ? void 0 : wasm2.HEAPU32[dataIndex] - offset;
                stringData.push(wasm2.UTF8ToString(offset, maxBytesToRead));
              }
              output.push([type, dims, stringData, "cpu"]);
            } else {
              if (preferredLocation === "gpu-buffer" && size > 0) {
                const getBuffer = wasm2.jsepGetBuffer;
                if (!getBuffer) {
                  throw new Error('preferredLocation "gpu-buffer" is not supported without using WebGPU.');
                }
                const gpuBuffer = getBuffer(dataOffset);
                const bufferSize = calculateTensorSizeInBytes(dataType, size);
                if (bufferSize === void 0 || !isGpuBufferSupportedType(type)) {
                  throw new Error(`Unsupported data type: ${type}`);
                }
                keepOutputTensor = true;
                output.push([
                  type,
                  dims,
                  {
                    gpuBuffer,
                    download: wasm2.jsepCreateDownloader(gpuBuffer, bufferSize, type),
                    dispose: () => {
                      wasm2._OrtReleaseTensor(tensor);
                    }
                  },
                  "gpu-buffer"
                ]);
              } else {
                const typedArrayConstructor = tensorTypeToTypedArrayConstructor(type);
                const data = new typedArrayConstructor(size);
                new Uint8Array(data.buffer, data.byteOffset, data.byteLength).set(
                  wasm2.HEAPU8.subarray(dataOffset, dataOffset + data.byteLength)
                );
                output.push([type, dims, data, "cpu"]);
              }
            }
          } finally {
            wasm2.stackRestore(beforeGetTensorDataStack);
            if (type === "string" && dataOffset) {
              wasm2._free(dataOffset);
            }
            if (!keepOutputTensor) {
              wasm2._OrtReleaseTensor(tensor);
            }
          }
        }
        if (ioBindingState && !enableGraphCapture) {
          wasm2._OrtClearBoundOutputs(ioBindingState.handle);
          activeSessions.set(sessionId, [
            sessionHandle,
            inputNamesUTF8Encoded,
            outputNamesUTF8Encoded,
            ioBindingState,
            enableGraphCapture,
            false
          ]);
        }
        return output;
      } finally {
        wasm2.stackRestore(beforeRunStack);
        inputTensorHandles.forEach((v) => wasm2._OrtReleaseTensor(v));
        outputTensorHandles.forEach((v) => wasm2._OrtReleaseTensor(v));
        inputOutputAllocs.forEach((p) => wasm2._free(p));
        if (runOptionsHandle !== 0) {
          wasm2._OrtReleaseRunOptions(runOptionsHandle);
        }
        runOptionsAllocs.forEach((p) => wasm2._free(p));
      }
    };
    endProfiling = (sessionId) => {
      const wasm2 = getInstance();
      const session = activeSessions.get(sessionId);
      if (!session) {
        throw new Error("invalid session id");
      }
      const sessionHandle = session[0];
      const profileFileName = wasm2._OrtEndProfiling(sessionHandle);
      if (profileFileName === 0) {
        checkLastError("Can't get an profile file name.");
      }
      wasm2._OrtFree(profileFileName);
    };
    extractTransferableBuffers = (tensors) => {
      const buffers = [];
      for (const tensor of tensors) {
        const data = tensor[2];
        if (!Array.isArray(data) && "buffer" in data) {
          buffers.push(data.buffer);
        }
      }
      return buffers;
    };
  }
});

// web/lib/wasm/proxy-wrapper.ts
var isProxy, proxyWorker, initializing2, initialized2, aborted2, temporaryObjectUrl, initWasmCallbacks, queuedCallbacks, enqueueCallbacks, ensureWorker, onProxyWorkerMessage, initializeWebAssemblyAndOrtRuntime, initializeOrtEp, copyFromExternalBuffer2, createSession2, releaseSession2, run2, endProfiling2;
var init_proxy_wrapper = __esm({
  "web/lib/wasm/proxy-wrapper.ts"() {
    "use strict";
    init_esm();
    init_wasm_core_impl();
    init_wasm_factory();
    init_wasm_utils_import();
    isProxy = () => !!env2.wasm.proxy && typeof document !== "undefined";
    initializing2 = false;
    initialized2 = false;
    aborted2 = false;
    queuedCallbacks = /* @__PURE__ */ new Map();
    enqueueCallbacks = (type, callbacks) => {
      const queue = queuedCallbacks.get(type);
      if (queue) {
        queue.push(callbacks);
      } else {
        queuedCallbacks.set(type, [callbacks]);
      }
    };
    ensureWorker = () => {
      if (initializing2 || !initialized2 || aborted2 || !proxyWorker) {
        throw new Error("worker not ready");
      }
    };
    onProxyWorkerMessage = (ev) => {
      switch (ev.data.type) {
        case "init-wasm":
          initializing2 = false;
          if (ev.data.err) {
            aborted2 = true;
            initWasmCallbacks[1](ev.data.err);
          } else {
            initialized2 = true;
            initWasmCallbacks[0]();
          }
          if (temporaryObjectUrl) {
            URL.revokeObjectURL(temporaryObjectUrl);
            temporaryObjectUrl = void 0;
          }
          break;
        case "init-ep":
        case "copy-from":
        case "create":
        case "release":
        case "run":
        case "end-profiling": {
          const callbacks = queuedCallbacks.get(ev.data.type);
          if (ev.data.err) {
            callbacks.shift()[1](ev.data.err);
          } else {
            callbacks.shift()[0](ev.data.out);
          }
          break;
        }
        default:
      }
    };
    initializeWebAssemblyAndOrtRuntime = async () => {
      if (initialized2) {
        return;
      }
      if (initializing2) {
        throw new Error("multiple calls to 'initWasm()' detected.");
      }
      if (aborted2) {
        throw new Error("previous call to 'initWasm()' failed.");
      }
      initializing2 = true;
      if (isProxy()) {
        return new Promise((resolve, reject) => {
          proxyWorker?.terminate();
          void importProxyWorker().then(([objectUrl, worker]) => {
            try {
              proxyWorker = worker;
              proxyWorker.onerror = (ev) => reject(ev);
              proxyWorker.onmessage = onProxyWorkerMessage;
              initWasmCallbacks = [resolve, reject];
              const message = { type: "init-wasm", in: env2 };
              proxyWorker.postMessage(message);
              temporaryObjectUrl = objectUrl;
            } catch (e) {
              reject(e);
            }
          }, reject);
        });
      } else {
        try {
          await initializeWebAssembly(env2.wasm);
          await initRuntime(env2);
          initialized2 = true;
        } catch (e) {
          aborted2 = true;
          throw e;
        } finally {
          initializing2 = false;
        }
      }
    };
    initializeOrtEp = async (epName) => {
      if (isProxy()) {
        ensureWorker();
        return new Promise((resolve, reject) => {
          enqueueCallbacks("init-ep", [resolve, reject]);
          const message = { type: "init-ep", in: { epName, env: env2 } };
          proxyWorker.postMessage(message);
        });
      } else {
        await initEp(env2, epName);
      }
    };
    copyFromExternalBuffer2 = async (buffer) => {
      if (isProxy()) {
        ensureWorker();
        return new Promise((resolve, reject) => {
          enqueueCallbacks("copy-from", [resolve, reject]);
          const message = { type: "copy-from", in: { buffer } };
          proxyWorker.postMessage(message, [buffer.buffer]);
        });
      } else {
        return copyFromExternalBuffer(buffer);
      }
    };
    createSession2 = async (model, options) => {
      if (isProxy()) {
        if (options?.preferredOutputLocation) {
          throw new Error('session option "preferredOutputLocation" is not supported for proxy.');
        }
        ensureWorker();
        return new Promise((resolve, reject) => {
          enqueueCallbacks("create", [resolve, reject]);
          const message = { type: "create", in: { model, options: { ...options } } };
          const transferable = [];
          if (model instanceof Uint8Array) {
            transferable.push(model.buffer);
          }
          proxyWorker.postMessage(message, transferable);
        });
      } else {
        return createSession(model, options);
      }
    };
    releaseSession2 = async (sessionId) => {
      if (isProxy()) {
        ensureWorker();
        return new Promise((resolve, reject) => {
          enqueueCallbacks("release", [resolve, reject]);
          const message = { type: "release", in: sessionId };
          proxyWorker.postMessage(message);
        });
      } else {
        releaseSession(sessionId);
      }
    };
    run2 = async (sessionId, inputIndices, inputs, outputIndices, outputs, options) => {
      if (isProxy()) {
        if (inputs.some((t) => t[3] !== "cpu")) {
          throw new Error("input tensor on GPU is not supported for proxy.");
        }
        if (outputs.some((t) => t)) {
          throw new Error("pre-allocated output tensor is not supported for proxy.");
        }
        ensureWorker();
        return new Promise((resolve, reject) => {
          enqueueCallbacks("run", [resolve, reject]);
          const serializableInputs = inputs;
          const message = {
            type: "run",
            in: { sessionId, inputIndices, inputs: serializableInputs, outputIndices, options }
          };
          proxyWorker.postMessage(message, extractTransferableBuffers(serializableInputs));
        });
      } else {
        return run(sessionId, inputIndices, inputs, outputIndices, outputs, options);
      }
    };
    endProfiling2 = async (sessionId) => {
      if (isProxy()) {
        ensureWorker();
        return new Promise((resolve, reject) => {
          enqueueCallbacks("end-profiling", [resolve, reject]);
          const message = { type: "end-profiling", in: sessionId };
          proxyWorker.postMessage(message);
        });
      } else {
        endProfiling(sessionId);
      }
    };
  }
});

// web/lib/wasm/session-handler-inference.ts
var encodeTensorMetadata, decodeTensorMetadata, OnnxruntimeWebAssemblySessionHandler;
var init_session_handler_inference = __esm({
  "web/lib/wasm/session-handler-inference.ts"() {
    "use strict";
    init_esm();
    init_proxy_wrapper();
    init_wasm_common();
    init_wasm_utils_env();
    init_wasm_utils_load_file();
    encodeTensorMetadata = (tensor, getName) => {
      switch (tensor.location) {
        case "cpu":
          return [tensor.type, tensor.dims, tensor.data, "cpu"];
        case "gpu-buffer":
          return [tensor.type, tensor.dims, { gpuBuffer: tensor.gpuBuffer }, "gpu-buffer"];
        default:
          throw new Error(`invalid data location: ${tensor.location} for ${getName()}`);
      }
    };
    decodeTensorMetadata = (tensor) => {
      switch (tensor[3]) {
        case "cpu":
          return new Tensor2(tensor[0], tensor[2], tensor[1]);
        case "gpu-buffer": {
          const dataType = tensor[0];
          if (!isGpuBufferSupportedType(dataType)) {
            throw new Error(`not supported data type: ${dataType} for deserializing GPU tensor`);
          }
          const { gpuBuffer, download, dispose } = tensor[2];
          return Tensor2.fromGpuBuffer(gpuBuffer, { dataType, dims: tensor[1], download, dispose });
        }
        default:
          throw new Error(`invalid data location: ${tensor[3]}`);
      }
    };
    OnnxruntimeWebAssemblySessionHandler = class {
      async fetchModelAndCopyToWasmMemory(path) {
        return copyFromExternalBuffer2(await loadFile(path));
      }
      async loadModel(pathOrBuffer, options) {
        TRACE_FUNC_BEGIN();
        let model;
        if (typeof pathOrBuffer === "string") {
          if (isNode) {
            model = await loadFile(pathOrBuffer);
          } else {
            model = await this.fetchModelAndCopyToWasmMemory(pathOrBuffer);
          }
        } else {
          model = pathOrBuffer;
        }
        [this.sessionId, this.inputNames, this.outputNames] = await createSession2(model, options);
        TRACE_FUNC_END();
      }
      async dispose() {
        return releaseSession2(this.sessionId);
      }
      async run(feeds, fetches, options) {
        TRACE_FUNC_BEGIN();
        const inputArray = [];
        const inputIndices = [];
        Object.entries(feeds).forEach((kvp) => {
          const name = kvp[0];
          const tensor = kvp[1];
          const index = this.inputNames.indexOf(name);
          if (index === -1) {
            throw new Error(`invalid input '${name}'`);
          }
          inputArray.push(tensor);
          inputIndices.push(index);
        });
        const outputArray = [];
        const outputIndices = [];
        Object.entries(fetches).forEach((kvp) => {
          const name = kvp[0];
          const tensor = kvp[1];
          const index = this.outputNames.indexOf(name);
          if (index === -1) {
            throw new Error(`invalid output '${name}'`);
          }
          outputArray.push(tensor);
          outputIndices.push(index);
        });
        const inputs = inputArray.map(
          (t, i) => encodeTensorMetadata(t, () => `input "${this.inputNames[inputIndices[i]]}"`)
        );
        const outputs = outputArray.map(
          (t, i) => t ? encodeTensorMetadata(t, () => `output "${this.outputNames[outputIndices[i]]}"`) : null
        );
        const results = await run2(this.sessionId, inputIndices, inputs, outputIndices, outputs, options);
        const resultMap = {};
        for (let i = 0; i < results.length; i++) {
          resultMap[this.outputNames[outputIndices[i]]] = outputArray[i] ?? decodeTensorMetadata(results[i]);
        }
        TRACE_FUNC_END();
        return resultMap;
      }
      startProfiling() {
      }
      endProfiling() {
        void endProfiling2(this.sessionId);
      }
    };
  }
});

// web/lib/backend-wasm.ts
var initializeFlags, OnnxruntimeWebAssemblyBackend;
var init_backend_wasm = __esm({
  "web/lib/backend-wasm.ts"() {
    "use strict";
    init_esm();
    init_proxy_wrapper();
    init_session_handler_inference();
    init_wasm_utils_import();
    initializeFlags = () => {
      if (typeof env2.wasm.initTimeout !== "number" || env2.wasm.initTimeout < 0) {
        env2.wasm.initTimeout = 0;
      }
      if (env2.wasm.simd === false) {
        console.warn(
          'Deprecated property "env.wasm.simd" is set to false. non-SIMD build is no longer provided, and this setting will be ignored.'
        );
      }
      if (typeof env2.wasm.proxy !== "boolean") {
        env2.wasm.proxy = false;
      }
      if (typeof env2.wasm.trace !== "boolean") {
        env2.wasm.trace = false;
      }
      if (typeof env2.wasm.numThreads !== "number" || !Number.isInteger(env2.wasm.numThreads) || env2.wasm.numThreads <= 0) {
        if (typeof self !== "undefined" && !self.crossOriginIsolated) {
          env2.wasm.numThreads = 1;
        } else {
          const numCpuLogicalCores = typeof navigator === "undefined" ? __require("node:os").cpus().length : navigator.hardwareConcurrency;
          env2.wasm.numThreads = Math.min(4, Math.ceil((numCpuLogicalCores || 1) / 2));
        }
      }
      if (true) {
        if (env2.wasm.wasmPaths === void 0 && scriptSrc && scriptSrc.indexOf("blob:") !== 0) {
          env2.wasm.wasmPaths = scriptSrc.substring(0, scriptSrc.lastIndexOf("/") + 1);
        }
      }
    };
    OnnxruntimeWebAssemblyBackend = class {
      /**
       * This function initializes the WebAssembly backend.
       *
       * This function will be called only once for each backend name. It will be called the first time when
       * `ort.InferenceSession.create()` is called with a registered backend name.
       *
       * @param backendName - the registered backend name.
       */
      async init(backendName) {
        initializeFlags();
        await initializeWebAssemblyAndOrtRuntime();
        await initializeOrtEp(backendName);
      }
      async createInferenceSessionHandler(pathOrBuffer, options) {
        const handler = new OnnxruntimeWebAssemblySessionHandler();
        await handler.loadModel(pathOrBuffer, options);
        return Promise.resolve(handler);
      }
    };
  }
});

// web/lib/backend-wasm-inference.ts
var backend_wasm_inference_exports = {};
__export(backend_wasm_inference_exports, {
  wasmBackend: () => wasmBackend
});
var wasmBackend;
var init_backend_wasm_inference = __esm({
  "web/lib/backend-wasm-inference.ts"() {
    "use strict";
    init_backend_wasm();
    wasmBackend = new OnnxruntimeWebAssemblyBackend();
  }
});

// web/lib/index.ts
init_esm();
init_esm();
init_esm();

// web/lib/version.ts
var version2 = "1.20.0-dev.20240827-1d059b8702";

// web/lib/index.ts
var lib_default = esm_exports;
if (false) {
  const onnxjsBackend = null.onnxjsBackend;
  registerBackend("webgl", onnxjsBackend, -10);
}
if (true) {
  const wasmBackend2 = true ? (init_backend_wasm_inference(), __toCommonJS(backend_wasm_inference_exports)).wasmBackend : null.wasmBackend;
  if (true) {
    registerBackend("webgpu", wasmBackend2, 5);
    registerBackend("webnn", wasmBackend2, 5);
  }
  registerBackend("cpu", wasmBackend2, 10);
  registerBackend("wasm", wasmBackend2, 10);
}
Object.defineProperty(env2.versions, "web", { value: version2, enumerable: true });
export {
  InferenceSession2 as InferenceSession,
  TRACE,
  TRACE_FUNC_BEGIN,
  TRACE_FUNC_END,
  Tensor2 as Tensor,
  TrainingSession2 as TrainingSession,
  lib_default as default,
  env2 as env,
  registerBackend
};
/**
 * @license
 * Copyright 2021 Google LLC. All Rights Reserved.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * =============================================================================
 */
/**
 * @license
 * Copyright 2020 Google LLC. All Rights Reserved.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * =============================================================================
 */
/**
 * @license
 * Copyright 2019 Google LLC. All Rights Reserved.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * =============================================================================
 */
