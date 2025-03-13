/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { LinkPreviewModel } = ChromeUtils.importESModule(
  "moz-src:///browser/components/genai/LinkPreviewModel.sys.mjs"
);
const { SentencePostProcessor } = ChromeUtils.importESModule(
  "moz-src:///browser/components/genai/LinkPreviewModel.sys.mjs"
);

/**
 * Test that text is processed for ai.
 */
add_task(function test_text_processing() {
  const text = "This is a test sentence. ";
  Assert.equal(
    LinkPreviewModel.preprocessText(text),
    "This is a test sentence.",
    "basic sentence should be trimmed"
  );
  Assert.equal(
    LinkPreviewModel.preprocessText("Too short."),
    "",
    "short sentence should be removed"
  );
  Assert.equal(
    LinkPreviewModel.preprocessText("Today is Mar. 12, 2025."),
    "Today is Mar. 12, 2025.",
    "abbreviations don't break sentences"
  );
  Assert.equal(
    LinkPreviewModel.preprocessText("Thisisatestsentence but fewwords."),
    "",
    "needs enough words"
  );
  Assert.equal(
    LinkPreviewModel.preprocessText(
      "This is a test sentence without punctuation"
    ),
    "",
    "needs punctuation"
  );
  Assert.equal(
    LinkPreviewModel.preprocessText(`${text}
    Too short.
    Today is Mar. 12, 2025.
    Thisisatestsentence but fewwords.
    This is a test sentence without punctuation`),
    "This is a test sentence. Today is Mar. 12, 2025.",
    "multiple sentences should be processed"
  );
  Assert.equal(
    LinkPreviewModel.preprocessText(text.repeat(100)),
    text.repeat(6).trim(),
    "restrict to 6 sentences"
  );
});

/**
 * Test that prefs affect text processing.
 */
add_task(function test_text_processing_prefs() {
  const text = "This is a test sentence. ";
  Services.prefs.setIntPref("browser.ml.linkPreview.inputSentences", 3);
  Assert.equal(
    LinkPreviewModel.preprocessText(text.repeat(100)),
    text.repeat(3).trim(),
    "restrict to 3 sentences"
  );
  Services.prefs.clearUserPref("browser.ml.linkPreview.inputSentences");
});

/**
 * Test that post processor provides sentences.
 */
add_task(function test_sentence_post_processor() {
  const processor = new SentencePostProcessor();
  Assert.equal(processor.put("Hello "), "", "no sentence yet");
  Assert.equal(processor.put("world. "), "", "sentence complete but not next");
  Assert.equal(
    processor.put("How "),
    "Hello world. ",
    "previous sentence complete"
  );
  Assert.equal(
    processor.put("are you today? I'm"),
    "How are you today? ",
    "question complete"
  );
  Assert.equal(processor.put(" fine. And"), "I'm fine. ", "response complete");
  Assert.equal(processor.put("you? Good"), "", "hit limit");
  Assert.equal(processor.flush(), "", "still nothing at limit");
});

/**
 * Test post processor respects limits.
 */
add_task(function test_sentence_post_processor_limits() {
  const processor = new SentencePostProcessor(1);
  Assert.equal(processor.put("Hi. There. "), "Hi. ", "first sentence");
  Assert.equal(processor.put("Nope."), "", "no more sentences");

  Services.prefs.setIntPref("browser.ml.linkPreview.outputSentences", 2);
  const viaPref = new SentencePostProcessor();
  Assert.equal(viaPref.put("Hi. There. "), "Hi. ", "first sentence");
  Assert.equal(viaPref.put("Yup. "), "There. ", "second sentence");
  Assert.equal(viaPref.put("Nope."), "", "no more sentences");
  Services.prefs.clearUserPref("browser.ml.linkPreview.outputSentences");
});
