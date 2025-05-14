// Export the FFIConverter object to make external types work.
export class {{ map|ffi_converter }} extends FfiConverterArrayBuffer {
    static read(dataStream) {
        const len = dataStream.readInt32();
        const map = {};
        for (let i = 0; i < len; i++) {
            const key = {{ map.key|read_fn }}(dataStream);
            const value = {{ map.value|read_fn }}(dataStream);
            map[key] = value;
        }

        return map;
    }

    static write(dataStream, value) {
        dataStream.writeInt32(Object.keys(value).length);
        for (const key in value) {
            {{ map.key|write_fn }}(dataStream, key);
            {{ map.value|write_fn }}(dataStream, value[key]);
        }
    }

    static computeSize(value) {
        // The size of the length
        let size = 4;
        for (const key in value) {
            size += {{ map.key|compute_size_fn }}(key);
            size += {{ map.value|compute_size_fn }}(value[key]);
        }
        return size;
    }

    static checkType(value) {
        for (const key in value) {
            try {
                {{ map.key|check_type_fn }}(key);
            } catch (e) {
                if (e instanceof UniFFITypeError) {
                    e.addItemDescriptionPart("(key)");
                }
                throw e;
            }

            try {
                {{ map.value|check_type_fn }}(value[key]);
            } catch (e) {
                if (e instanceof UniFFITypeError) {
                    e.addItemDescriptionPart(`[${key}]`);
                }
                throw e;
            }
        }
    }
}
