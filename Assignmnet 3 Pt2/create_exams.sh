#!/bin/bash
echo "Creating exam files..."
for i in {1..19}; do
    printf "%04d\n" $((1000 + i)) > exam_$(printf "%02d" $i).txt
done
echo "9999" > exam_20.txt
echo "Created 20 exam files"