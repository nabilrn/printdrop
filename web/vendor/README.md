# Vendored browser dependencies

`js-sha256` is tracked as a Git submodule at commit `9a54fb31d4594762987e1b5d175265f6bac921de` (tag `v1.0.0`).

PrintDrop uses `build/sha256.min.js` for incremental SHA-256 in the browser so large files are hashed in bounded chunks instead of being loaded into memory at once. The upstream project is MIT licensed; its license is retained inside the pinned submodule.
