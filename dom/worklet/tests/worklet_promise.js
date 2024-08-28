class WasmProcessWorkletProcessor extends AudioWorkletProcessor {
  constructor(...args) {
    super(...args);
    this.port.onmessage = e => {
      WebAssembly.compile(e.data).then(
        m => {
          this.port.postMessage(m);
        },
        () => {
          this.port.postMessage("error");
        }
      );
    };
  }

  process() {
    // Do nothing, output silence
    return true;
  }
}

registerProcessor("promise", WasmProcessWorkletProcessor);
