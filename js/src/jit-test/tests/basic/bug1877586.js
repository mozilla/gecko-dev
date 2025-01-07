// |jit-test| allow-oom

// Keep this in sync with js::ThreadType::THREAD_TYPE_PROMISE_TASK
const THREAD_TYPE_PROMISE_TASK = 8;

try {
    for (let i = 0; i < 5; i++) {
        WebAssembly.instantiateStreaming(
            wasmTextToBinary('(module (func) (export "" (func 0)))')
        );
    }
} catch (e) {}
oomAtAllocation(7, THREAD_TYPE_PROMISE_TASK);
