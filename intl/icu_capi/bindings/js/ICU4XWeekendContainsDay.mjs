import wasm from "./diplomat-wasm.mjs"
import * as diplomatRuntime from "./diplomat-runtime.mjs"

export class ICU4XWeekendContainsDay {
  constructor(underlying) {
    this.monday = (new Uint8Array(wasm.memory.buffer, underlying, 1))[0] == 1;
    this.tuesday = (new Uint8Array(wasm.memory.buffer, underlying + 1, 1))[0] == 1;
    this.wednesday = (new Uint8Array(wasm.memory.buffer, underlying + 2, 1))[0] == 1;
    this.thursday = (new Uint8Array(wasm.memory.buffer, underlying + 3, 1))[0] == 1;
    this.friday = (new Uint8Array(wasm.memory.buffer, underlying + 4, 1))[0] == 1;
    this.saturday = (new Uint8Array(wasm.memory.buffer, underlying + 5, 1))[0] == 1;
    this.sunday = (new Uint8Array(wasm.memory.buffer, underlying + 6, 1))[0] == 1;
  }
}
