#!/bin/bash
# collect_headers.sh
mkdir -p include
find src/ -name "*.h" -exec cp --parents {} include/ \;
