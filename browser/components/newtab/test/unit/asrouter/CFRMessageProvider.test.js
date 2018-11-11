import {CFRMessageProvider} from "lib/CFRMessageProvider.jsm";
const messages = CFRMessageProvider.getMessages();

const REGULAR_IDS = [
  "FACEBOOK_CONTAINER",
  "GOOGLE_TRANSLATE",
  "YOUTUBE_ENHANCE",
  "WIKIPEDIA_CONTEXT_MENU_SEARCH",
  "REDDIT_ENHANCEMENT",
];

describe("CFRMessageProvider", () => {
  it("should have a total of 10 messages", () => {
    assert.lengthOf(messages, 10);
  });
  it("should two variants for each of the five regular addons", () => {
    for (const id of REGULAR_IDS) {
      const cohort1 = messages.find(msg => msg.id === `${id}_1`);
      assert.ok(cohort1, `contains one day cohort for ${id}`);
      assert.deepEqual(cohort1.frequency, {lifetime: 1}, "one day cohort has the right frequency cap");
      assert.include(cohort1.targeting, `(providerCohorts.cfr in ["one_per_day", "nightly"])`);

      const cohort3 = messages.find(msg => msg.id === `${id}_3`);
      assert.ok(cohort3, `contains three day cohort for ${id}`);
      assert.deepEqual(cohort3.frequency, {lifetime: 3}, "three day cohort has the right frequency cap");
      assert.include(cohort3.targeting, `(providerCohorts.cfr == "three_per_day")`);

      assert.deepEqual(cohort1.content, cohort3.content, "cohorts should have the same content");
    }
  });
  it("should always have xpinstallEnabled as targeting if it is an addon", () => {
    for (const message of messages) {
      // Ensure that the CFR messages that are recommending an addon have this targeting.
      // In the future when we can do targeting based on category, this test will change.
      // See bug 1494778 and 1497653
      assert.include(message.targeting, `(xpinstallEnabled == true)`);
    }
  });
});
