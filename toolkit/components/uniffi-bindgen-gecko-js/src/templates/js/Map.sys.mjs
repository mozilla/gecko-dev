// Export the FFIConverter object to make external types work.
export class {{ ffi_converter }} extends FfiConverterArrayBuffer {
    static read(dataStream) {
        const len = dataStream.readInt32();
        const map = new Map();
        for (let i = 0; i < len; i++) {
            const key = {{ key_type.ffi_converter() }}.read(dataStream);
            const value = {{ value_type.ffi_converter() }}.read(dataStream);
            map.set(key, value);
        }

        return map;
    }

    static write(dataStream, map) {
        dataStream.writeInt32(map.size);
        for (const [key, value] of map) {
            {{ key_type.ffi_converter() }}.write(dataStream, key);
            {{ value_type.ffi_converter() }}.write(dataStream, value);
        }
    }

    static computeSize(map) {
        // The size of the length
        let size = 4;
        for (const [key, value] of map) {
            size += {{ key_type.ffi_converter() }}.computeSize(key);
            size += {{ value_type.ffi_converter() }}.computeSize(value);
        }
        return size;
    }

    static checkType(map) {
        for (const [key, value] of map) {
            try {
                {{ key_type.ffi_converter() }}.checkType(key);
            } catch (e) {
                if (e instanceof UniFFITypeError) {
                    e.addItemDescriptionPart("(key)");
                }
                throw e;
            }

            try {
                {{ value_type.ffi_converter() }}.checkType(value);
            } catch (e) {
                if (e instanceof UniFFITypeError) {
                    e.addItemDescriptionPart(`[${key}]`);
                }
                throw e;
            }
        }
    }
}
