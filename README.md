# One More Line Clone

All code is human written

## Building wasm
On mac, you need to install the llvm on homebrew, since the mac supplied version
does not support wasm compilation. Also you need lld.
```
brew install llvm
brew instlal lld
```
The Makefile expects `/opt/homebrew/opt/llvm/bin/clang` location for clang binary
run `make wasm` to compile a wasm build
