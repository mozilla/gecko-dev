import wasm from "./diplomat-wasm.mjs"
import * as diplomatRuntime from "./diplomat-runtime.mjs"

const ICU4XMeasureUnit_box_destroy_registry = new FinalizationRegistry(underlying => {
  wasm.ICU4XMeasureUnit_destroy(underlying);
});

export class ICU4XMeasureUnit {
  #lifetimeEdges = [];
  constructor(underlying, owned, edges) {
    this.underlying = underlying;
    this.#lifetimeEdges.push(...edges);
    if (owned) {
      ICU4XMeasureUnit_box_destroy_registry.register(this, underlying);
    }
  }
}
