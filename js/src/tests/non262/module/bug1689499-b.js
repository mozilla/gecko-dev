// |reftest| skip -- support file
// SKIP test262 export
// Test needs drainJobQueue.

import A from "./bug1689499-a.js";
if (true) await 0;
export default "B";

