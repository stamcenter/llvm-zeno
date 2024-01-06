# Building LLVM for Zeno

These directions describe how to build the Zeno compatible version of the Clang Compiler. Note that only the Clang compiler is compatible with Zeno. Other parts of the toolchain, (linkers, runtimes, standard libraries, bin utils, etc) have not been ported yet.

These directions have been tested on Ubuntu 20.04

## Install dependencies for Ubuntu 20.04

`sudo apt-get install autoconf automake autotools-dev curl python3 python3-pip libmpc-dev libmpfr-dev libgmp-dev gawk build-essential bison flex texinfo gperf libtool patchutils bc zlib1g-dev libexpat-dev ninja-build git cmake libglib2.0-dev clang`

## Build GNU toolchain

Clone the repo

`git clone https://github.com/riscv/riscv-gnu-toolchain.git`

CD to the repo directory, the rest of this section is relative to the repo's top level directory:

`cd riscv-gnu-toolchain`

Checkout a stable release of the toolchain (Commit Hash: 65056bdb149c87db4e7223c4e8b5466cf326ff86): 

`git checkout 2023.01.31`

Configure the toolchain for rv64ima (make sure your prefix directory exists and is writeable): 

`./configure --prefix=/opt/riscv_ima_linux --with-arch=rv64ima --with-abi=lp64`

Build the toolchain

`make -j $(nproc) linux`

Make sure you build the linux version


## Building LLVM

Clone the repo

`git clone https://github.com/stamcenter/llvm-zeno.git`

CD to the directory

`cd llvm-zeno`

Prepare build directory

`mkdir build && cd build`

Configure and build. This first build will not include the compiler runtime. To include the compiler runtime, you must either
1. Build with a vanila RISC-V runtime that does not include Zeno support
2. Use this version of the compiler to Build with a runtime with Zeno support and then rebuild another compiler with the new runtime

```bash
export RISCV_GNU_BINTOOLS=/opt/riscv_ima_linux
export LLVM_ROOT=/opt/llvm-zeno
cmake -G "Unix Makefiles" \
    -DCMAKE_BUILD_TYPE="Debug" \
    -DCMAKE_INSTALL_PREFIX="$LLVM_ROOT" \
    -DLLVM_TARGETS_TO_BUILD="RISCV;X86" \
    -DLLVM_ENABLE_PROJECTS="clang;clang++;lld;lldb" \
    -DLLVM_ENABLE_PIC=ON \
    -DLLVM_PARALLEL_LINK_JOBS=8 \
    -DLLVM_ENABLE_ZLIB=OFF \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_EXE_LINKER_FLAGS="-Wl,-no-keep-memory -fuse-ld=gold" \
    -DBUILD_SHARED_LIBS=ON \
    -DLLVM_DEFAULT_TARGET_TRIPLE="riscv64-unknown-linux-gnu" \
    -DCLANG_DEFAULT_CXX_STDLIB="libstdc++" \
    -DLLVM_ENABLE_RUNTIMES="" \
    ../llvm
make -j $(nproc)
```

Install LLVM to a folder of your choosing (make sure your LLVM_ROOT directory exists and is writeable):

```bash
LLVM_ROOT=/opt/llvm-zeno
cmake -DCMAKE_INSTALL_PREFIX=$LLVM_ROOT -P cmake_install.cmake
```

## Using LLVM

You can now plug in your zeno compiler anywhere you would use a C compiler. Given a Makefile that defines `CC`, `LD`, `CFLAGS`, and `LDFLAGS`, you can override the existing build process by pasting the following lines.

The following commands will build this example program:
```c
int main() {
  int a=4;
  int b=5;
  return a*b;
}
```

```makefile
export LLVM_ROOT=/opt/llvm-zeno
export MUSL_ROOT=/opt/musl-zeno
export CC=${LLVM_ROOT}/bin/clang
export LD=${CC}
export CFLAGS=" --target=riscv64-unknown-linux-gnu -march=rv64imazzeno0p1 -menable-experimental-extensions -mabi=lp64 -nostdlib -I${MUSL_ROOT}/include"
export LDFLAGS=" -static -fuse-ld=${LLVM_ROOT}/bin/ld.lld --sysroot=${MUSL_ROOT} -rtlib=compiler-rt"
$CC $CFLAGS -O0 -o main.exe main.c
```

# Experimental Things to Try

These steps are unlikely to work without additional fixes and porting.

## Build GNU toolchain with Zeno Compiler

Building the GNU toolchain with the Zeno Compiler could be one way to create a Zeno-compatible sysroot/standard library.
Additional fixes/changes to the GNU toolchain are likely required though.

```
git checkout 2022.06.10 
mkdir build-zeno && cd build-zeno
GNU_ZENO_BAREMETAL=/home/stam-user/riscv-bintools/gnu-zeno-baremetal
../configure --prefix=$GNU_ZENO_BAREMETAL \
  --srcdir=.. \
  --with-arch=rv64ima \
  --with-abi=lp64 \
  --host=rv64ima \
  --enable-multilib \
  CC="$LLVM_ROOT/bin/clang" \
  CFLAGS="--target=riscv64-unknown-linux-gnu -march=rv64imazzeno0p1 -menable-experimental-extensions -mabi=lp64 -Os -nodefaultlibs -nostdinc -nostdlib" \
  LDFLAGS="-fuse-ld=$LLVM_ROOT/bin/ld.lld -nodefaultlibs -nostdinc -nostdlib"
```

To check the pointer size:
```echo "int main() {return sizeof(void*);}" | $LLVM_ROOT/bin/clang -x c - -c -S -o - --target=riscv64-unknown-linux-gnu -march=rv64imazzeno0p1 -menable-experimental-extensions -O3```

## Build newlib Standalone with LLVM-Zeno

An attempt to compile newlib as a standalone library with the LLVM-Zeno compiler 

```bash
git clone git://sourceware.org/git/newlib-cygwin.git
cd newlib-cygwin
mkdir build-zeno && cd zeno
STANDALONE_NEWLIB=/home/stam-user/riscv-bintools/standalone-baremetal
CFLAGS="--target=riscv64-unknown-linux-gnu -march=rv64imazzeno0p1 -menable-experimental-extensions -mabi=lp64 -Os -nodefaultlibs -nostdlib -nostdinc -I$LLVM_ROOT/lib/clang/13.0.0/include -Qunused-arguments -Wno-unknown-pragmas"
LDFLAGS="-fuse-ld=$LLVM_ROOT/bin/ld.lld -nodefaultlibs -nostdinc -nostdlib"
../configure --prefix=$STANDALONE_NEWLIB \
  --srcdir=.. \
  --host=x86_64-pc-linux-gnu \
  --target=riscv64-unknown-elf \
  --enable-newlib-nano-malloc \
  CC_FOR_TARGET="$LLVM_ROOT/bin/clang" \
  CXX_FOR_TARGET="$LLVM_ROOT/bin/clang++" \
  AR_FOR_TARGET="$LLVM_ROOT/bin/llvm-ar" \
  LD_FOR_TARGET="$LLVM_ROOT/bin/ld.lld" \
  NM_FOR_TARGET="$LLVM_ROOT/bin/llvm-nm" \
  RANLIB_FOR_TARGET="$LLVM_ROOT/bin/llvm-ranlib" \
  OBJCOPY_FOR_TARGET="$LLVM_ROOT/bin/llvm-objcopy" \
  OBJDUMP_FOR_TARGET="$LLVM_ROOT/bin/llvm-objdump" \
  READELF_FOR_TARGET="$LLVM_ROOT/bin/llvm-readelf" \
  STRIP_FOR_TARGET="$LLVM_ROOT/bin/llvm-strip" \
  CFLAGS_FOR_TARGET="$CFLAGS" \
  CXXFLAGS_FOR_TARGET="$CFLAGS" \
  LDFLAGS_FOR_TARGET="$LDFLAGS"
make -j $(nproc) && make install
```


## Re-Building the Zeno Compiler with a Runtime

If a compatible runtime can be built, the following build command provides an example of how to re-build a Zeno compiler with the given runtime.

```bash
RISCV_GNU_BINTOOLS=/opt/riscv_ima_linux
LLVM_ROOT=/opt/llvm-zeno
cmake -G "Unix Makefiles" \
    -DCMAKE_BUILD_TYPE="Debug" \
    -DCMAKE_INSTALL_PREFIX="$LLVM_ROOT" \
    -DLLVM_TARGETS_TO_BUILD="RISCV;X86" \
    -DLLVM_ENABLE_PROJECTS="clang;clang++;lld;lldb" \
    -DLLVM_ENABLE_PIC=ON \
    -DLLVM_PARALLEL_LINK_JOBS=8 \
    -DLLVM_ENABLE_ZLIB=OFF \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_EXE_LINKER_FLAGS="-Wl,-no-keep-memory -fuse-ld=gold" \
    -DBUILD_SHARED_LIBS=ON \
    -DLLVM_DEFAULT_TARGET_TRIPLE="riscv64-unknown-linux-gnu" \
    -DCLANG_DEFAULT_CXX_STDLIB="libstdc++" \
    -DLLVM_ENABLE_RUNTIMES="compiler-rt" \
    -DCOMPILER_RT_DEFAULT_TARGET_ONLY=ON \
    -DHAVE_DECL_FE_ALL_EXCEPT=0 \
    -DCOMPILER_RT_BUILD_BUILTINS=ON \
    -DCOMPILER_RT_BUILD_SANITIZERS=OFF \
    -DCOMPILER_RT_BUILD_XRAY=OFF \
    -DCOMPILER_RT_BUILD_MEMPROF=OFF \
    -DCOMPILER_RT_BUILD_LIBFUZZER=OFF \
    -DCOMPILER_RT_BUILD_PROFILE=OFF \
    -DCOMPILER_RT_CFLAGS="-march=rv64imazzeno0p1 -menable-experimental-extensions -mabi=lp64 -nodefaultlibs -nostdlib -Os --sysroot=$RISCV_GNU_BINTOOLS/sysroot --gcc-toolchain=$RISCV_GNU_BINTOOLS" \
    ../llvm
make -j $(nproc)
```

Another example to attempt building against musl:
```bash
MUSL_CLANG_ROOT=/home/stam-user/riscv-bintools/musl-clang
cmake -G $BUILD_SYSTEM \
    -DCMAKE_BUILD_TYPE="Debug" \
    -DCMAKE_INSTALL_PREFIX=$LLVM_ROOT \
    -DLLVM_TARGETS_TO_BUILD="RISCV;X86" \
    -DLLVM_ENABLE_PROJECTS="clang;clang++;lld;lldb" \
    -DCMAKE_INSTALL_PREFIX="$LLVM_ROOT" \
    -DLLVM_ENABLE_PIC=ON \
    -DLLVM_PARALLEL_LINK_JOBS=8 \
    -DLLVM_ENABLE_ZLIB=OFF \
    -DCMAKE_C_COMPILER=clang-12 \
    -DCMAKE_CXX_COMPILER=clang++-12 \
    -DCMAKE_EXE_LINKER_FLAGS="-Wl,-no-keep-memory -fuse-ld=gold" \
    -DBUILD_SHARED_LIBS=ON \
    -DLLVM_DEFAULT_TARGET_TRIPLE="riscv64-unknown-linux-gnu" \
    -DCLANG_DEFAULT_CXX_STDLIB="libstdc++" \
    -DLLVM_ENABLE_RUNTIMES="compiler-rt" \
    -DCOMPILER_RT_DEFAULT_TARGET_ONLY=ON \
    -DHAVE_DECL_FE_ALL_EXCEPT=0 \
    -DCOMPILER_RT_BUILD_BUILTINS=ON \
    -DCOMPILER_RT_BUILD_SANITIZERS=OFF \
    -DCOMPILER_RT_BUILD_XRAY=OFF \
    -DCOMPILER_RT_BUILD_MEMPROF=OFF \
    -DCOMPILER_RT_BUILD_LIBFUZZER=OFF \
    -DCOMPILER_RT_BUILD_PROFILE=OFF \
    -DCOMPILER_RT_CFLAGS="-march=rv64imazzeno0p1 -menable-experimental-extensions -mabi=lp64 -nodefaultlibs -nostdinc -nostdlib -Os -fno-exceptions -I$MUSL_CLANG_ROOT/include --sysroot=$MUSL_CLANG_ROOT --gcc-toolchain=$RISCV_GNU_BINTOOLS" \
    ../llvm
```

If issues with `clear_cache.c` appear, try building it with the following command. This is untested and may or may not be useful.
```
/home/stam-user/llvm-zeno/build/./bin/clang \
  --target=riscv64-unknown-linux-gnu \
  -DVISIBILITY_HIDDEN  \
  -g \
  -DCOMPILER_RT_HAS_FLOAT16 \
  -std=c11 \
  -fPIC \
  -fno-builtin \
  -fvisibility=hidden \
  -march=rv64imazzeno0p1 \
  -menable-experimental-extensions \
  -mabi=lp64 \
  -nodefaultlibs \
  -nostdinc \
  -nostdlib \
  -Os \
  -I/home/stam-user/riscv-bintools/musl-clang/include \
  --sysroot=/home/stam-user/riscv-bintools/musl-clang \
  --gcc-toolchain=/home/stam-user/riscv-bintools/riscv_ima_linux \
  -MD \
  -MT CMakeFiles/clang_rt.builtins-riscv64.dir/clear_cache.c.o \
  -MF CMakeFiles/clang_rt.builtins-riscv64.dir/clear_cache.c.o.d \
  -o CMakeFiles/clang_rt.builtins-riscv64.dir/clear_cache.c.o \
  -c /home/stam-user/llvm-zeno/compiler-rt/lib/builtins/clear_cache.c
```

## Build Zeno Compatible MUSL LibC Library


An attempt to compile MUSL as a standalone library with the LLVM-Zeno compiler.
If MUSL can be built with the Zeno compiler, the Zeno compatible MUSL can be used as the runtime in a re-build of another Zeno compiler.

Clone the source

`git clone git://git.musl-libc.org/musl`

Enter the directory

`cd musl`

Attempt to build MUSL with one of the following build commands:

### Build a Vanilla/Generic MUSL Library with the Zeno Compiler

Configure, build, and install (make sure your MUSL_ROOT directory exists and is writeable):

```bash
MUSL_ROOT=/opt/musl-zeno
./configure --prefix=$MUSL_ROOT \
    --target=riscv64-unknown-linux-gnu \
    --disable-shared \
    CC="$LLVM_ROOT/bin/clang" \
    CFLAGS="--target=riscv64-unknown-linux-gnu -march=rv64imazzeno0p1 -menable-experimental-extensions -mabi=lp64 -Os -nodefaultlibs -nostdinc -nostdlib" \
    LDFLAGS="-fuse-ld=$LLVM_ROOT/bin/ld.lld -nodefaultlibs -nostdinc -nostdlib --sysroot=$LLVM_ROOT -L$LLVM_ROOT/lib/clang/13.0.0/lib/riscv64-unknown-linux-gnu" \
    AR="$LLVM_ROOT/bin/llvm-ar" \
    RANLIB="$LLVM_ROOT/bin/llvm-ranlib" \
    LIBCC="-lclang_rt.builtins"
make -j $(nproc)
make install
```


### Build a Vanilla/Generic MUSL Library with a RISC-V Compiler

While debugging, it may be helpful to try building a Vanila RISC-V verison of MUSL. To do so, run the following command(s).

You may need Clang-12, if you get errors with your version of Clang, try installing a specific version:
```bash
sudo apt-get install clang-12 lld-12
```

```bash
mkdir build-clang && cd build-clang
MUSL_CLANG_ROOT=/opt/musl-clang
../configure --prefix=$MUSL_CLANG_ROOT \
    --srcdir=.. \
    --target=zeno \
    --disable-shared \
    CC="clang-12" \
    CFLAGS="--target=riscv64-unknown-linux-gnu -mabi=lp64 -Os -nodefaultlibs -nostdinc -nostdlib -mno-relax" \
    LD="ld.lld-12" \
    LDFLAGS="-fuse-ld=$(which ld.lld-12) -nodefaultlibs -nostdinc -nostdlib -mno-relax" \
    AR="llvm-ar-12" \
    RANLIB="llvm-ranlib-12"
make -j $(nproc)
make install
cd $MUSL_CLANG_ROOT/include && ln -s . linux && cd -
```

### Compile Zeno Fork of MUSL

This example command was used to build a custom Zeno target architecture in the MUSL project, but the custom Zeno target is incomplete and not stable.

```bash
../configure --prefix=$MUSL_CLANG_ROOT \
    --srcdir=.. \
    --target=zeno \
    --disable-shared \
    CC="$LLVM_ROOT/bin/clang" \
    CFLAGS="--target=riscv64-unknown-linux-gnu -march=rv64imazzeno0p1 -menable-experimental-extensions -mabi=lp64 -Os -nodefaultlibs -nostdinc -nostdlib" \
    LD="$LLVM_ROOT/bin/ld.lld" \
    LDFLAGS="-fuse-ld=$LLVM_ROOT/bin/ld.lld -nodefaultlibs -nostdinc -nostdlib" \
    AR="$LLVM_ROOT/bin/llvm-ar" \
    RANLIB="$LLVM_ROOT/bin/llvm-ranlib" 
```

