import wasm from "./diplomat-wasm.mjs"
import * as diplomatRuntime from "./diplomat-runtime.mjs"
import { ICU4XWeekRelativeUnit_js_to_rust, ICU4XWeekRelativeUnit_rust_to_js } from "./ICU4XWeekRelativeUnit.mjs"

export class ICU4XWeekOf {
  constructor(underlying) {
    this.week = (new Uint16Array(wasm.memory.buffer, underlying, 1))[0];
    this.unit = ICU4XWeekRelativeUnit_rust_to_js[diplomatRuntime.enumDiscriminant(wasm, underlying + 4)];
  }
}
