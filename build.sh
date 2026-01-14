
#!/bin/bash

set -e
mkdir -p build

WARN_FLAGS="-Wall -Wextra -Wpedantic -Wuninitialized -Wcast-qual -Wdisabled-optimization -Winit-self -Wlogical-op -Wmissing-include-dirs -Wredundant-decls -Wshadow -Wstrict-prototypes -Wpointer-to-int-cast -Wint-to-pointer-cast -Wconversion -Wduplicated-cond -Wduplicated-branches -Wformat=2 -Wshift-overflow=2 -Wint-in-bool-context -Wvector-operation-performance -Wvla -Wdisabled-optimization -Wredundant-decls -Wmissing-parameter-type -Wold-style-declaration -Wlogical-not-parentheses -Waddress -Wmemset-transposed-args -Wmemset-elt-size -Wsizeof-pointer-memaccess -Wwrite-strings -Wtrampolines -Werror=implicit-function-declaration"
BASE_FLAGS="-fmax-errors=1 -funsigned-char"
if [ "$1" = 'release' ]; then
    BASE_FLAGS="$BASE_FLAGS -ggdb -O2"
else
    BASE_FLAGS="$BASE_FLAGS -ggdb"
fi
PATH_FLAGS="-I. -I/usr/include -I/usr/lib -I/usr/local/lib -I/usr/local/include"
LINK_FLAGS="-lm -Wl,-rpath=."

if [ ! -f build/stb_truetype.o ]; then
    /usr/bin/c99 -O2 -c src/stb_truetype.c -o build/stb_truetype.o
fi

if [ ! -f build/RGFW.o ]; then
    /usr/bin/c99 -O2 -c src/RGFW.c -o build/RGFW.o
fi

/usr/bin/c99 ${WARN_FLAGS} ${PATH_FLAGS} ${BASE_FLAGS} -c src/uir.c -o build/uir.o
/usr/bin/gcc ${WARN_FLAGS} ${PATH_FLAGS} ${BASE_FLAGS} examples/bench.c build/uir.o ${LINK_FLAGS} -o build/bench
/usr/bin/gcc ${WARN_FLAGS} ${PATH_FLAGS} ${BASE_FLAGS} examples/test.c build/uir.o ${LINK_FLAGS} -o build/test
/usr/bin/gcc ${WARN_FLAGS} ${PATH_FLAGS} ${BASE_FLAGS} examples/text.c build/stb_truetype.o build/uir.o ${LINK_FLAGS} -o build/text
/usr/bin/gcc ${WARN_FLAGS} ${PATH_FLAGS} ${BASE_FLAGS} examples/ui.c build/stb_truetype.o build/uir.o build/RGFW.o ${LINK_FLAGS} -lX11 -lXrandr -o build/ui
