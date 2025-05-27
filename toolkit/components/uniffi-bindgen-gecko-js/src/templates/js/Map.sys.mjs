// Export the FFIConverter object to make external types work.
export class {{ map|ffi_converter }} extends FfiConverterArrayBuffer {
    static read(dataStream) {
        const len = dataStream.readInt32();
        const map = new Map();
        for (let i = 0; i < len; i++) {
            const key = {{ map.key|read_fn }}(dataStream);
            const value = {{ map.value|read_fn }}(dataStream);
            map.set(key, value);
        }

        return map;
    }

     static write(dataStream, map) {
        dataStream.writeInt32(map.size);
        for (const [key, value] of map) {
            {{ map.key|write_fn }}(dataStream, key);
            {{ map.value|write_fn }}(dataStream, value);
        }
    }

    static computeSize(map) {
        // The size of the length
        let size = 4;
        for (const [key, value] of map) {
            size += {{ map.key|compute_size_fn }}(key);
            size += {{ map.value|compute_size_fn }}(value);
        }
        return size;
    }

    static checkType(map) {
        for (const [key, value] of map) {
            try {
                {{ map.key|check_type_fn }}(key);
            } catch (e) {
                if (e instanceof UniFFITypeError) {
                    e.addItemDescriptionPart("(key)");
                }
                throw e;
            }

            try {
                {{ map.value|check_type_fn }}(value);
            } catch (e) {
                if (e instanceof UniFFITypeError) {
                    e.addItemDescriptionPart(`[${key}]`);
                }
                throw e;
            }
        }
    }
}
