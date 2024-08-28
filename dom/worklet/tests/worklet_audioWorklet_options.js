class OptionsProcessWorkletProcessor extends AudioWorkletProcessor {
  constructor(...args) {
    super(...args);
    this.port.postMessage(args[0].processorOptions);
  }

  process() {
    return true;
  }
}

registerProcessor("options", OptionsProcessWorkletProcessor);
