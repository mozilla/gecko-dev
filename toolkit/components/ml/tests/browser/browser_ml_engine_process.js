/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Check that options are overwritten, but not if there's a null value
 */
add_task(async function test_options_overwrite() {
  const options = new PipelineOptions({
    taskName: "summarization",
    modelId: "test-echo",
    modelRevision: "main",
  });

  Assert.equal(options.taskName, "summarization");
  options.updateOptions({ taskName: "summarization2", modelId: null });
  Assert.equal(options.taskName, "summarization2");
  Assert.equal(options.modelId, "test-echo");
});

/**
 * Check that updateOptions accepts a PipelineOptions object
 */
add_task(async function test_options_updated_with_options() {
  const options = new PipelineOptions({
    taskName: "summarization",
    modelId: "test-echo",
    modelRevision: "main",
  });
  const options2 = new PipelineOptions({
    taskName: "summarization2",
    modelId: "test-echo",
    modelRevision: "main",
  });

  Assert.equal(options.taskName, "summarization");
  options.updateOptions(options2);
  Assert.equal(options.taskName, "summarization2");
});
