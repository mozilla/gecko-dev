// Export the FFIConverter object to make external types work.
export class {{ map.self_type.ffi_converter }} extends FfiConverterArrayBuffer {
    static read(dataStream) {
        const len = dataStream.readInt32();
        const map = new Map();
        for (let i = 0; i < len; i++) {
            const key = {{ map.key.ffi_converter }}.read(dataStream);
            const value = {{ map.value.ffi_converter }}.read(dataStream);
            map.set(key, value);
        }

        return map;
    }

     static write(dataStream, map) {
        dataStream.writeInt32(map.size);
        for (const [key, value] of map) {
            {{ map.key.ffi_converter }}.write(dataStream, key);
            {{ map.value.ffi_converter }}.write(dataStream, value);
        }
    }

    static computeSize(map) {
        // The size of the length
        let size = 4;
        for (const [key, value] of map) {
            size += {{ map.key.ffi_converter }}.computeSize(key);
            size += {{ map.value.ffi_converter }}.computeSize(value);
        }
        return size;
    }

    static checkType(map) {
        for (const [key, value] of map) {
            try {
                {{ map.key.ffi_converter }}.checkType(key);
            } catch (e) {
                if (e instanceof UniFFITypeError) {
                    e.addItemDescriptionPart("(key)");
                }
                throw e;
            }

            try {
                {{ map.value.ffi_converter }}.checkType(value);
            } catch (e) {
                if (e instanceof UniFFITypeError) {
                    e.addItemDescriptionPart(`[${key}]`);
                }
                throw e;
            }
        }
    }
}
