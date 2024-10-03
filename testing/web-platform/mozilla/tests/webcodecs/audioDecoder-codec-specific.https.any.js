// META: global=window,dedicatedworker
// META: script=/webcodecs/utils.js
// META: variant=?mp4_raw_aac_no_desc

// By spec, if the description is absent, the bitstream defaults to ADTS format.
// However, this is added to ensure compatibility and handle potential misuse cases.
const MP4_AAC_DATA_NO_DESCRIPTION = {
  src: 'sfx-aac.mp4',
  config: {
    codec: 'mp4a.40.2',
    sampleRate: 48000,
    numberOfChannels: 1,
  },
  chunks: [
    {offset: 44, size: 241},
    {offset: 285, size: 273},
    {offset: 558, size: 251},
    {offset: 809, size: 118},
    {offset: 927, size: 223},
    {offset: 1150, size: 141},
    {offset: 1291, size: 217},
    {offset: 1508, size: 159},
    {offset: 1667, size: 209},
    {offset: 1876, size: 176},
  ],
  duration: 21333
};

// Allows mutating `callbacks` after constructing the AudioDecoder, wraps calls
// in t.step().
function createAudioDecoder(t, callbacks) {
  return new AudioDecoder({
    output(frame) {
      if (callbacks && callbacks.output) {
        t.step(() => callbacks.output(frame));
      } else {
        t.unreached_func('unexpected output()');
      }
    },
    error(e) {
      if (callbacks && callbacks.error) {
        t.step(() => callbacks.error(e));
      } else {
        t.unreached_func('unexpected error()');
      }
    }
  });
}

// Create a view of an ArrayBuffer.
function view(buffer, {offset, size}) {
  return new Uint8Array(buffer, offset, size);
}

let CONFIG = null;
let CHUNK_DATA = null;
let CHUNKS = null;
promise_setup(async () => {
  const data = {
    '?mp4_raw_aac_no_desc': MP4_AAC_DATA_NO_DESCRIPTION,
  }[location.search];

  // Don't run any tests if the codec is not supported.
  assert_equals("function", typeof AudioDecoder.isConfigSupported);
  let supported = false;
  try {
    const support = await AudioDecoder.isConfigSupported({
      codec: data.config.codec,
      sampleRate: data.config.sampleRate,
      numberOfChannels: data.config.numberOfChannels
    });
    supported = support.supported;
  } catch (e) {
  }
  assert_implements_optional(supported, data.config.codec + ' unsupported');

  // Fetch the media data and prepare buffers.
  const response = await fetch(data.src);
  const buf = await response.arrayBuffer();

  CONFIG = {...data.config};
  if (data.config.description) {
    CONFIG.description = view(buf, data.config.description);
  }

  CHUNK_DATA = [];
  // For PCM, split in chunks of 1200 bytes and compute the rest
  if (data.chunks.length == 0) {
    let offset = data.offset;
    // 1200 is divisible by 2 and 3 and is a plausible packet length
    // for PCM: this means that there won't be samples split in two packet
    let PACKET_LENGTH = 1200;
    let bytesPerSample = 0;
    switch (data.config.codec) {
      case "pcm-s16": bytesPerSample = 2; break;
      case "pcm-s24": bytesPerSample = 3; break;
      case "pcm-s32": bytesPerSample = 4; break;
      case "pcm-f32": bytesPerSample = 4; break;
      default: bytesPerSample = 1; break;
    }
    while (offset < buf.byteLength) {
      let size = Math.min(buf.byteLength - offset, PACKET_LENGTH);
      assert_equals(size % bytesPerSample, 0);
      CHUNK_DATA.push(view(buf, {offset, size}));
      offset += size;
    }
    data.duration = 1000 * 1000 * PACKET_LENGTH / data.config.sampleRate / bytesPerSample;
  } else {
    CHUNK_DATA = data.chunks.map((chunk, i) => view(buf, chunk));
  }

  CHUNKS = CHUNK_DATA.map((encodedData, i) => new EncodedAudioChunk({
                            type: 'key',
                            timestamp: i * data.duration,
                            duration: data.duration,
                            data: encodedData
                          }));
});

promise_test(t => {
  return AudioDecoder.isConfigSupported(CONFIG);
}, 'Test isConfigSupported()');

promise_test(t => {
  // Define a valid config that includes a hypothetical 'futureConfigFeature',
  // which is not yet recognized by the User Agent.
  const validConfig = {
    ...CONFIG,
    futureConfigFeature: 'foo',
  };

  // The UA will evaluate validConfig as being "valid", ignoring the
  // `futureConfigFeature` it  doesn't recognize.
  return AudioDecoder.isConfigSupported(validConfig).then((decoderSupport) => {
    // AudioDecoderSupport must contain the following properites.
    assert_true(decoderSupport.hasOwnProperty('supported'));
    assert_true(decoderSupport.hasOwnProperty('config'));

    // AudioDecoderSupport.config must not contain unrecognized properties.
    assert_false(decoderSupport.config.hasOwnProperty('futureConfigFeature'));

    // AudioDecoderSupport.config must contiain the recognized properties.
    assert_equals(decoderSupport.config.codec, validConfig.codec);
    assert_equals(decoderSupport.config.sampleRate, validConfig.sampleRate);
    assert_equals(
        decoderSupport.config.numberOfChannels, validConfig.numberOfChannels);

    if (validConfig.description) {
      // The description must be copied.
      assert_false(
          decoderSupport.config.description === validConfig.description,
          'description is unique');
      assert_array_equals(
          new Uint8Array(decoderSupport.config.description, 0),
          new Uint8Array(validConfig.description, 0), 'description');
    } else {
      assert_false(
          decoderSupport.config.hasOwnProperty('description'), 'description');
    }
  });
}, 'Test that AudioDecoder.isConfigSupported() returns a parsed configuration');

promise_test(async t => {
  const decoder = createAudioDecoder(t);
  decoder.configure(CONFIG);
  assert_equals(decoder.state, 'configured', 'state');
}, 'Test configure()');

promise_test(t => {
  const decoder = createAudioDecoder(t);
  return testClosedCodec(t, decoder, CONFIG, CHUNKS[0]);
}, 'Verify closed AudioDecoder operations');

promise_test(async t => {
  const callbacks = {};
  const decoder = createAudioDecoder(t, callbacks);

  let outputs = 0;
  callbacks.output = frame => {
    outputs++;
    frame.close();
  };

  decoder.configure(CONFIG);
  CHUNKS.forEach(chunk => {
    decoder.decode(chunk);
  });

  await decoder.flush();
  assert_equals(outputs, CHUNKS.length, 'outputs');
}, 'Test decoding');

promise_test(async t => {
  const callbacks = {};
  const decoder = createAudioDecoder(t, callbacks);

  let outputs = 0;
  callbacks.output = frame => {
    outputs++;
    frame.close();
  };

  decoder.configure(CONFIG);
  decoder.decode(new EncodedAudioChunk(
      {type: 'key', timestamp: -42, data: CHUNK_DATA[0]}));

  await decoder.flush();
  assert_equals(outputs, 1, 'outputs');
}, 'Test decoding a with negative timestamp');

promise_test(async t => {
  const callbacks = {};
  const decoder = createAudioDecoder(t, callbacks);

  let outputs = 0;
  callbacks.output = frame => {
    outputs++;
    frame.close();
  };

  decoder.configure(CONFIG);
  decoder.decode(CHUNKS[0]);

  await decoder.flush();
  assert_equals(outputs, 1, 'outputs');

  decoder.decode(CHUNKS[0]);
  await decoder.flush();
  assert_equals(outputs, 2, 'outputs');
}, 'Test decoding after flush');

promise_test(async t => {
  const callbacks = {};
  const decoder = createAudioDecoder(t, callbacks);

  decoder.configure(CONFIG);
  decoder.decode(CHUNKS[0]);
  decoder.decode(CHUNKS[1]);
  const flushDone = decoder.flush();

  // Wait for the first output, then reset.
  let outputs = 0;
  await new Promise(resolve => {
    callbacks.output = frame => {
      outputs++;
      assert_equals(outputs, 1, 'outputs');
      decoder.reset();
      frame.close();
      resolve();
    };
  });

  // Flush should have been synchronously rejected.
  await promise_rejects_dom(t, 'AbortError', flushDone);

  assert_equals(outputs, 1, 'outputs');
}, 'Test reset during flush');

promise_test(async t => {
  const callbacks = {};
  const decoder = createAudioDecoder(t, callbacks);

  // No decodes yet.
  assert_equals(decoder.decodeQueueSize, 0);

  decoder.configure(CONFIG);

  // Still no decodes.
  assert_equals(decoder.decodeQueueSize, 0);

  let lastDequeueSize = Infinity;
  decoder.ondequeue = () => {
    assert_greater_than(lastDequeueSize, 0, "Dequeue event after queue empty");
    assert_greater_than(lastDequeueSize, decoder.decodeQueueSize,
                        "Dequeue event without decreased queue size");
    lastDequeueSize = decoder.decodeQueueSize;
  };

  for (let chunk of CHUNKS)
    decoder.decode(chunk);

  assert_greater_than_equal(decoder.decodeQueueSize, 0);
  assert_less_than_equal(decoder.decodeQueueSize, CHUNKS.length);

  await decoder.flush();
  // We can guarantee that all decodes are processed after a flush.
  assert_equals(decoder.decodeQueueSize, 0);
  // Last dequeue event should fire when the queue is empty.
  assert_equals(lastDequeueSize, 0);

  // Reset this to Infinity to track the decline of queue size for this next
  // batch of decodes.
  lastDequeueSize = Infinity;

  for (let chunk of CHUNKS)
    decoder.decode(chunk);

  assert_greater_than_equal(decoder.decodeQueueSize, 0);
  decoder.reset();
  assert_equals(decoder.decodeQueueSize, 0);
}, 'AudioDecoder decodeQueueSize test');
