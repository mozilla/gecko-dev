// |jit-test| skip-if: !hasFunction.oomAtAllocation; error: Error

// Keep this in sync with js::ThreadType::THREAD_TYPE_WORKER.
const THREAD_TYPE_WORKER = 12;

// OOM testing of worker threads is disallowed because it's not thread safe.
oomAtAllocation(11, THREAD_TYPE_WORKER);
