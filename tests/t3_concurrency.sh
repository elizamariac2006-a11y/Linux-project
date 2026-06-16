#!/bin/bash

set -e

mkdir -p data reports tests/sandbox

touch tests/sandbox/file1.txt tests/sandbox/file2.txt

rm -f data/index.db

for i in {1..5}; do
        ./tools/fileops.sh run -- fileops_indexer --root tests/sandbox --db data/index.db &
        PIDS[$i]=$!
done

wait "${PIDS[@]}"
echo "Toate instantele fileops_indexer s-au terminat"

if [ -f "data/index.db" ]; then
        echo "data/index.db a fost generat"
else
        echo "data/index.db lipseste"
        exit 1
fi

rm -f data/proc.db

for i in {1..5}; do
        ./tools/fileops.sh run -- proc_snapshot --db data/proc.db &
        PROC_PIDS[$i]=$!
done

wait "${PROC_PIDS[@]}"
echo "Toate instantele proc_snapshot s-au terminat"

if [ -f "data/proc.db" ]; then
        echo "data/proc.db a fost generat"
else
        echo "data/proc.db lipseste"
        exit 1
fi

./tools/fileops.sh run -- fileops_indexer --root tests/sandbox --db data/index_old.db

touch tests/sandbox/file_nou.txt
rm tests/sandbox/file1.txt

./tools/fileops.sh run -- fileops_indexer --root tests/sandbox --db data/index_new.db

./tools/fileops.sh run -- db_diff --old data/index_old.db --new data/index_new.db --out reports/T3_filediff.txt

if [ -s "reports/T3_filediff.txt" ]; then
        echo "reports/T3_filediff.txt generat cu succes"
else
        echo "Raportul de fisiere lipseste sau este gol"
        exit 1
fi

./tools/fileops.sh run -- proc_snapshot --db data/proc_old.db
sleep 2 & 
TEMP_PID=$!
./tools/fileops.sh run -- proc_snapshot --db data/proc_new.db
./tools/fileops.sh run -- db_diff --old data/proc_old.db --new data/proc_new.db --out reports/T3_procdiff.txt

if [ -s "reports/T3_procdiff.txt" ]; then
        echo "reports/T3_procdiff.txt generat cu succes"
else
        echo "Raportul de procese lipseste sau e gol"
        exit 1
fi
