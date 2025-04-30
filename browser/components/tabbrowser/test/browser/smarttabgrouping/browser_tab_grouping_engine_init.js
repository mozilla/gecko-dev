/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

add_task(async function test_model_revision_topic_engine() {
  const featureId = "smart-tab-topic";
  let initData = {
    featureId,
  };
  Assert.equal(
    SmartTabGroupingManager.getUpdatedInitData(initData, featureId)
      .modelRevision,
    undefined,
    "No model revision in init should return undefined"
  );

  initData.modelRevision = "v0.1.0";
  Assert.equal(
    SmartTabGroupingManager.getUpdatedInitData(initData, featureId)
      .modelRevision,
    "v0.1.0",
    "Model revision set should return proper value"
  );

  await SpecialPowers.pushPrefEnv({
    set: [["browser.tabs.groups.smart.topicModelRevision", "v0.2.0"]],
  });
  Assert.equal(
    SmartTabGroupingManager.getUpdatedInitData(initData, featureId)
      .modelRevision,
    "v0.2.0",
    "Override through nimbus or config should take precedence"
  );

  initData = {
    featureId,
  };

  await SpecialPowers.pushPrefEnv({
    set: [["browser.tabs.groups.smart.topicModelRevision", "latest"]],
  });
  Assert.equal(
    SmartTabGroupingManager.getUpdatedInitData(initData, featureId)
      .modelRevision,
    undefined,
    "'latest' config should return undefined for model revision init "
  );
});

add_task(async function test_model_revision_embedding_engine() {
  const featureId = "smart-tab-embedding";
  let initData = {
    featureId,
  };
  Assert.equal(
    SmartTabGroupingManager.getUpdatedInitData(initData, featureId)
      .modelRevision,
    undefined,
    "No model revision in init should return undefined"
  );

  initData.modelRevision = "v0.1.0";
  Assert.equal(
    SmartTabGroupingManager.getUpdatedInitData(initData, featureId)
      .modelRevision,
    "v0.1.0",
    "Model revision set should return proper value"
  );

  await SpecialPowers.pushPrefEnv({
    set: [["browser.tabs.groups.smart.embeddingModelRevision", "v0.2.0"]],
  });
  Assert.equal(
    SmartTabGroupingManager.getUpdatedInitData(initData, featureId)
      .modelRevision,
    "v0.2.0",
    "Override through nimbus or config should take precedence"
  );

  initData = {
    featureId,
  };

  await SpecialPowers.pushPrefEnv({
    set: [["browser.tabs.groups.smart.embeddingModelRevision", "latest"]],
  });
  Assert.equal(
    SmartTabGroupingManager.getUpdatedInitData(initData, featureId)
      .modelRevision,
    undefined,
    "'latest' config should return undefined for model revision init "
  );
});
