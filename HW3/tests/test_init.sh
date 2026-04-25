#!/bin/bash

echo "rulare test_init..."

./tools/fileops.sh init > /dev/null 2>&1

REQ_DIR=("bin" "src" "include" "data" "logs" "reports" "tmp" "tests" "doc" "tools")

MISS=0

for dir in "${REQ_DIR[@]}"; do
    if [ ! -d "$dir" ]; then
        echo "FAIL: '$dir' nu a fost creat"
        MISS=$((MISS+1))
    fi
done

if [ $MISS -eq 0 ]; then
    echo "RESULT: PASS"
    exit 0
else
    echo "RESULT: FAIL ($MISS directoare lipsa)"
    exit 1
fi