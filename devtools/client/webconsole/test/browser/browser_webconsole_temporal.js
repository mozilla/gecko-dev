/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI =
  "data:text/html,<!DOCTYPE html>Test evaluating Temporal objects";

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);

  let message = await executeAndWaitForResultMessage(
    hud,
    "new Temporal.Instant(355924804000000000n)",
    "Temporal.Instant"
  );
  is(
    message.node.innerText.trim(),
    "Temporal.Instant 1981-04-12T12:00:04Z",
    "Got expected result for Temporal.Instant"
  );

  message = await executeAndWaitForResultMessage(
    hud,
    `new Temporal.PlainDate(2021, 7, 1, "coptic")`,
    "Temporal.PlainDate"
  );
  is(
    message.node.innerText.trim(),
    "Temporal.PlainDate 2021-07-01[u-ca=coptic]",
    "Got expected result for Temporal.PlainDate"
  );

  message = await executeAndWaitForResultMessage(
    hud,
    `new Temporal.PlainDateTime(2021, 7, 1, 0, 0, 0, 0, 0, 0, "gregory")`,
    "Temporal.PlainDateTime"
  );
  is(
    message.node.innerText.trim(),
    "Temporal.PlainDateTime 2021-07-01T00:00:00[u-ca=gregory]",
    "Got expected result for Temporal.PlainDateTime"
  );

  message = await executeAndWaitForResultMessage(
    hud,
    `new Temporal.PlainMonthDay(7, 1, "chinese")`,
    "Temporal.PlainMonthDay"
  );
  is(
    message.node.innerText.trim(),
    "Temporal.PlainMonthDay 1972-07-01[u-ca=chinese]",
    "Got expected result for Temporal.PlainMonthDay"
  );

  message = await executeAndWaitForResultMessage(
    hud,
    `new Temporal.PlainTime(4, 20)`,
    "Temporal.PlainTime"
  );
  is(
    message.node.innerText.trim(),
    "Temporal.PlainTime 04:20:00",
    "Got expected result for Temporal.PlainTime"
  );

  message = await executeAndWaitForResultMessage(
    hud,
    `new Temporal.PlainYearMonth(2021, 7, "indian")`,
    "Temporal.PlainYearMonth"
  );
  is(
    message.node.innerText.trim(),
    "Temporal.PlainYearMonth 2021-07-01[u-ca=indian]",
    "Got expected result for Temporal.PlainYearMonth"
  );

  message = await executeAndWaitForResultMessage(
    hud,
    `new Temporal.ZonedDateTime(0n, "America/New_York")`,
    "Temporal.ZonedDateTime"
  );
  is(
    message.node.innerText.trim(),
    "Temporal.ZonedDateTime 1969-12-31T19:00:00-05:00[America/New_York]",
    "Got expected result for Temporal.ZonedDateTime"
  );

  message = await executeAndWaitForResultMessage(
    hud,
    `Temporal.Duration.from({ years: 1 })`,
    "Temporal.Duration"
  );
  is(
    message.node.innerText.trim(),
    "Temporal.Duration P1Y",
    "Got expected result for Temporal.Duration"
  );
});
