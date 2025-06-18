import wasm from "./diplomat-wasm.mjs"
import * as diplomatRuntime from "./diplomat-runtime.mjs"
import { ICU4XError_js_to_rust, ICU4XError_rust_to_js } from "./ICU4XError.mjs"
import { ICU4XMeasureUnit } from "./ICU4XMeasureUnit.mjs"

const ICU4XMeasureUnitParser_box_destroy_registry = new FinalizationRegistry(underlying => {
  wasm.ICU4XMeasureUnitParser_destroy(underlying);
});

export class ICU4XMeasureUnitParser {
  #lifetimeEdges = [];
  constructor(underlying, owned, edges) {
    this.underlying = underlying;
    this.#lifetimeEdges.push(...edges);
    if (owned) {
      ICU4XMeasureUnitParser_box_destroy_registry.register(this, underlying);
    }
  }

  parse(arg_unit_id) {
    const buf_arg_unit_id = diplomatRuntime.DiplomatBuf.str8(wasm, arg_unit_id);
    const diplomat_out = (() => {
      const diplomat_receive_buffer = wasm.diplomat_alloc(5, 4);
      wasm.ICU4XMeasureUnitParser_parse(diplomat_receive_buffer, this.underlying, buf_arg_unit_id.ptr, buf_arg_unit_id.size);
      const is_ok = diplomatRuntime.resultFlag(wasm, diplomat_receive_buffer, 4);
      if (is_ok) {
        const ok_value = new ICU4XMeasureUnit(diplomatRuntime.ptrRead(wasm, diplomat_receive_buffer), true, []);
        wasm.diplomat_free(diplomat_receive_buffer, 5, 4);
        return ok_value;
      } else {
        const throw_value = ICU4XError_rust_to_js[diplomatRuntime.enumDiscriminant(wasm, diplomat_receive_buffer)];
        wasm.diplomat_free(diplomat_receive_buffer, 5, 4);
        throw new diplomatRuntime.FFIError(throw_value);
      }
    })();
    buf_arg_unit_id.free();
    return diplomat_out;
  }
}
