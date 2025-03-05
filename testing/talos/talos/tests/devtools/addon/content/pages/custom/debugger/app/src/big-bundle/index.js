import './minified.js';
import { stepInTest } from './step-in-test.js';
import stepOverTest from './step-over-test.js';
import stepOutTest from './step-out-test.js';

window.hitBreakpointInBigBundle = function test() {
  stepInTest();
  stepOverTest();
  stepOutTest();
}
