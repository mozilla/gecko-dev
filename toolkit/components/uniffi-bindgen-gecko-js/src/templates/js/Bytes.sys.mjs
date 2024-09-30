// Export the FFIConverter object to make external types work.
export class {{ ffi_converter }} extends FfiConverterArrayBuffer {
    static read(dataStream) {
        return dataStream.readBytes()
    }

    static write(dataStream, value) {
        dataStream.writeBytes(value)
    }

    static computeSize(value) {
        // The size of the length + 1 byte / item
        return 4 + value.length
    }

    static checkType(value) {
        if (!value instanceof Uint8Array) {
            throw new UniFFITypeError(`${value} is not an Uint8Array`);
        }
    }
}
