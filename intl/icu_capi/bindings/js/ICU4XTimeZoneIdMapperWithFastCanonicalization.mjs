import wasm from "./diplomat-wasm.mjs"
import * as diplomatRuntime from "./diplomat-runtime.mjs"
import { ICU4XError_js_to_rust, ICU4XError_rust_to_js } from "./ICU4XError.mjs"

const ICU4XTimeZoneIdMapperWithFastCanonicalization_box_destroy_registry = new FinalizationRegistry(underlying => {
  wasm.ICU4XTimeZoneIdMapperWithFastCanonicalization_destroy(underlying);
});

export class ICU4XTimeZoneIdMapperWithFastCanonicalization {
  #lifetimeEdges = [];
  constructor(underlying, owned, edges) {
    this.underlying = underlying;
    this.#lifetimeEdges.push(...edges);
    if (owned) {
      ICU4XTimeZoneIdMapperWithFastCanonicalization_box_destroy_registry.register(this, underlying);
    }
  }

  static create(arg_provider) {
    return (() => {
      const diplomat_receive_buffer = wasm.diplomat_alloc(5, 4);
      wasm.ICU4XTimeZoneIdMapperWithFastCanonicalization_create(diplomat_receive_buffer, arg_provider.underlying);
      const is_ok = diplomatRuntime.resultFlag(wasm, diplomat_receive_buffer, 4);
      if (is_ok) {
        const ok_value = new ICU4XTimeZoneIdMapperWithFastCanonicalization(diplomatRuntime.ptrRead(wasm, diplomat_receive_buffer), true, []);
        wasm.diplomat_free(diplomat_receive_buffer, 5, 4);
        return ok_value;
      } else {
        const throw_value = ICU4XError_rust_to_js[diplomatRuntime.enumDiscriminant(wasm, diplomat_receive_buffer)];
        wasm.diplomat_free(diplomat_receive_buffer, 5, 4);
        throw new diplomatRuntime.FFIError(throw_value);
      }
    })();
  }

  canonicalize_iana(arg_value) {
    const buf_arg_value = diplomatRuntime.DiplomatBuf.str8(wasm, arg_value);
    const diplomat_out = diplomatRuntime.withWriteable(wasm, (writeable) => {
      return (() => {
        const diplomat_receive_buffer = wasm.diplomat_alloc(5, 4);
        wasm.ICU4XTimeZoneIdMapperWithFastCanonicalization_canonicalize_iana(diplomat_receive_buffer, this.underlying, buf_arg_value.ptr, buf_arg_value.size, writeable);
        const is_ok = diplomatRuntime.resultFlag(wasm, diplomat_receive_buffer, 4);
        if (is_ok) {
          const ok_value = {};
          wasm.diplomat_free(diplomat_receive_buffer, 5, 4);
          return ok_value;
        } else {
          const throw_value = ICU4XError_rust_to_js[diplomatRuntime.enumDiscriminant(wasm, diplomat_receive_buffer)];
          wasm.diplomat_free(diplomat_receive_buffer, 5, 4);
          throw new diplomatRuntime.FFIError(throw_value);
        }
      })();
    });
    buf_arg_value.free();
    return diplomat_out;
  }

  canonical_iana_from_bcp47(arg_value) {
    const buf_arg_value = diplomatRuntime.DiplomatBuf.str8(wasm, arg_value);
    const diplomat_out = diplomatRuntime.withWriteable(wasm, (writeable) => {
      return (() => {
        const diplomat_receive_buffer = wasm.diplomat_alloc(5, 4);
        wasm.ICU4XTimeZoneIdMapperWithFastCanonicalization_canonical_iana_from_bcp47(diplomat_receive_buffer, this.underlying, buf_arg_value.ptr, buf_arg_value.size, writeable);
        const is_ok = diplomatRuntime.resultFlag(wasm, diplomat_receive_buffer, 4);
        if (is_ok) {
          const ok_value = {};
          wasm.diplomat_free(diplomat_receive_buffer, 5, 4);
          return ok_value;
        } else {
          const throw_value = ICU4XError_rust_to_js[diplomatRuntime.enumDiscriminant(wasm, diplomat_receive_buffer)];
          wasm.diplomat_free(diplomat_receive_buffer, 5, 4);
          throw new diplomatRuntime.FFIError(throw_value);
        }
      })();
    });
    buf_arg_value.free();
    return diplomat_out;
  }
}
