// Export the FFIConverter object to make external types work.
export class {{ cbi.self_type.ffi_converter }} extends FfiConverter {
    static lower(callbackObj) {
        return {{ cbi.vtable.js_handler_var }}.storeCallbackObj(callbackObj)
    }

    static lift(handleId) {
        return {{ cbi.vtable.js_handler_var }}.getCallbackObj(handleId)
    }

    static read(dataStream) {
        return this.lift(dataStream.readInt64())
    }

    static write(dataStream, callbackObj) {
        dataStream.writeInt64(this.lower(callbackObj))
    }

    static computeSize(callbackObj) {
        return 8;
    }
}

{%- let vtable = cbi.vtable %}
{%- include "CallbackInterfaceHandler.sys.mjs" %}
