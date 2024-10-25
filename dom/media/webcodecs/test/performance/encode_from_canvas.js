let encoder;
let encodeTimes = [];
let outputTimes = [];

function reset() {
  encoder = null;
  encodeTimes.length = 0;
  outputTimes.length = 0;
}

self.onmessage = async event => {
  if (event.data.command === "configure") {
    const { codec, width, height, latencyMode, avcFormat } = event.data;

    encoder = new VideoEncoder({
      output: (chunk, _) => {
        const endTime = performance.now();
        outputTimes.push({
          timestamp: chunk.timestamp,
          time: endTime,
          type: chunk.type,
        });
      },
      error: _ => {},
    });

    const config = {
      codec,
      width,
      height,
      bitrate: 1000000, // 1 Mbps
      framerate: 30,
      latencyMode,
    };

    // Add H264 specific configuration
    if (codec.startsWith("avc1")) {
      config.avc = { format: avcFormat };
    }

    encoder.configure(config);
  } else if (event.data.command === "encode") {
    const { frame, isKey } = event.data;
    const encodeTime = performance.now();
    encoder.encode(frame, { keyFrame: isKey });
    encodeTimes.push({
      timestamp: frame.timestamp,
      time: encodeTime,
      type: isKey ? "key" : "delta",
    });
    frame.close();
  } else if (event.data.command === "flush") {
    await encoder.flush();
    self.postMessage({ command: "result", encodeTimes, outputTimes });
    reset();
  }
};
