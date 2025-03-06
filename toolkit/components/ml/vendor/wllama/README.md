# Vendored Dependencies

This repository includes the following vendored dependency:

- wllama `2.2.1`

To build the vendored dependencies, run:
```bash
bash build.sh
```

---

## Modifications to `wllama`

- **`wllama/src/utils.ts`**: The `absoluteUrl` function was modified to return the given path directly, since we are already providing absolute URLs. The original code used the page URL as a base, which was incompatible with the ML engine.

- **`scripts/docker-compose.yml`**: The build arguments were updated to enable release mode and disable native, llamafile and aarch.  
  Keeping llamafile enabled causes the ML Engine to hang during execution. Disabling aarch removes some warning and attempts to convert
  the model by llama.cpp to aarch64.

- **`wllama/cpp/actions.hpp`**, **`wllama/cpp/glue.hpp`**, and **`wllama/src/wllama.ts`**: Added support for overriding `n_ubatch`, `flash_attn`, `n_threads`, `use_mmap`, and `use_mlock` to allow more flexibility in memory/speed configurations.

- **`wllama/src/wllama.ts`**: The `createCompletion` function has been modified to accept **tokens** in addition to raw text. This update allows input to be provided as either **pre-tokenized data** or a plain string.
