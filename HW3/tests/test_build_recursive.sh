#!/bin/bash

echo "rulare build_recursive..."

TEST_SRC="tmp/scenariu_test_src"

rm -rf "$TEST_SRC"
mkdir -p "$TEST_SRC/app"
mkdir -p "$TEST_SRC/lib/include"

cat <<EOF > "$TEST_SRC/lib/include/util.h"
#ifndef UTIL_H
#define UTIL_H
int util_add(int a, int b);
#endif
EOF

cat <<EOF > "$TEST_SRC/lib/util.c"
#include "util.h"
int util_add(int a, int b) {
    return a + b;
}
EOF

cat <<EOF > "$TEST_SRC/app/main_demo.c"
#include <stdio.h>
#include "util.h"
int main() {
    printf("%d\n", util_add(2, 3));
    return 0;
}
EOF

export CFLAGS="-I$TEST_SRC/lib/include -std=c11 -Wall -Wextra"

./tools/fileops.sh build --src "$TEST_SRC"

if [ ! -x "bin/demo" ]; then
    echo "FAIL bin/demo nu exista sau nu este executabil"
    exit 1
fi

mkdir -p tmp
./bin/demo > tmp/demo_out.txt
RESULT=$(cat tmp/demo_out.txt)

if [ "$RESULT" == "5" ]; then
    echo "RESULT: PASS"
    exit 0
else
    echo "FAIL: out gresit. exp: 5, got: $RESULT"
    exit 1
fi