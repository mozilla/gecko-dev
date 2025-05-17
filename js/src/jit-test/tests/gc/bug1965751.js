function a(b, c) {
    d = "a".repeat(1000)
    e = ensureLinearString(newRope(d, "Unknown"))
    ensureLinearString(newRope(e, "abcdef", {
        nursery: c
    }))
    gc()
}
a(true, false)
