try {
    a = principal = this
    newGlobal(a).createMappedArrayBuffer(a)
} catch (e) {
    // Didn't Crash: 👍
}
