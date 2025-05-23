import {
  {{ external|ffi_converter }},
} from "./{{ external.module_name }}.sys.mjs";

// Export the FFIConverter object to make external types work.
export { {{ external|ffi_converter }} };
