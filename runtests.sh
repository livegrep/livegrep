#!/bin/bash

testfiles=(testcases index unicode benchmarks)
files=('' '^arch/x86' 'sched\..' 'sunhme')

set -e

for fre in "${files[@]}"; do
    for case in "${testfiles[@]}"; do
        extra=()
        if [ "$fre" ]; then
            extra=(--noempty)
            if [ "$case" = "benchmarks" ]; then
                echo "Skipping benchmark tests with non-empty file regex...";
                continue;
            fi
        fi
        echo "Testing $case with file '$fre'..."
        node test/test.js --querylist "test/$case" "${extra[@]}" -- --file "$fre"
    done
done
