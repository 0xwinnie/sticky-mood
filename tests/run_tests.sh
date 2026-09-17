#!/bin/bash
# Host build + run of the record-store unit tests. Zero ESP-IDF.
set -e
cd "$(dirname "$0")/.."
mkdir -p build/tests

CXX=${CXX:-g++}
"$CXX" -std=c++17 -Wall -Wextra -Imain \
    main/app/record_store.cpp \
    tests/test_record_store.cpp \
    -o build/tests/test_record_store

./build/tests/test_record_store
