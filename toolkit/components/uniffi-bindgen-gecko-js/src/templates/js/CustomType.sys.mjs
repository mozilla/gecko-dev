// Export the FFIConverter object to make external types work.
export class {{ custom|ffi_converter }} extends FfiConverter {
    static lift(buf) {
        return {{ custom.builtin|lift_fn }}(buf);    
    }
    
    static lower(buf) {
        return {{ custom.builtin|lower_fn }}(buf);
    }
    
    static write(dataStream, value) {
        {{ custom.builtin|write_fn }}(dataStream, value);
    } 
    
    static read(buf) {
        return {{ custom.builtin|read_fn }}(buf);
    }
    
    static computeSize(value) {
        return {{ custom.builtin|compute_size_fn }}(value);
    }
}

// TODO: We should also allow JS to customize the type eventually.
