assertErrorMessage(() => new WebAssembly.Tag({
    parameters: Array(700000).fill("i32"),
}), TypeError, /too many tag parameters/);;
