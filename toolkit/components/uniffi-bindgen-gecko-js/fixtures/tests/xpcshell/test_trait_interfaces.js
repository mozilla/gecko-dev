/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TraitInterfaces = ChromeUtils.importESModule(
  "resource://gre/modules/RustUniffiTraitInterfaces.sys.mjs"
);

const calculator = TraitInterfaces.makeCalculator();
Assert.equal(calculator.add(4, 8), 12);

const buggyCalculator = TraitInterfaces.makeBuggyCalculator();
Assert.equal(buggyCalculator.add(4, 8), 13);
