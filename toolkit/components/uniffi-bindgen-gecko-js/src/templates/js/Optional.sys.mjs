// Export the FFIConverter object to make external types work.
export class {{ optional|ffi_converter }} extends FfiConverterArrayBuffer {
    static checkType(value) {
        if (value !== undefined && value !== null) {
            {{ optional.inner|check_type_fn }}(value)
        }
    }

    static read(dataStream) {
        const code = dataStream.readUint8(0);
        switch (code) {
            case 0:
                return null
            case 1:
                return {{ optional.inner|read_fn }}(dataStream)
            default:
                throw new UniFFIError(`Unexpected code: ${code}`);
        }
    }

    static write(dataStream, value) {
        if (value === null || value === undefined) {
            dataStream.writeUint8(0);
            return;
        }
        dataStream.writeUint8(1);
        {{ optional.inner|write_fn}}(dataStream, value)
    }

    static computeSize(value) {
        if (value === null || value === undefined) {
            return 1;
        }
        return 1 + {{ optional.inner|compute_size_fn}}(value)
    }
}
