#!/bin/bash

echo "pornire test concurenta"

mkdir -p data reports
rm -f data/*.db reports/T3_*.txt

echo ""
echo "3 indexeri paraleli"

./tools/fileops.sh run fileops_indexer src data/index1.db & PID1=$!

./tools/fileops.sh run fileops_indexer include data/index1.db & PID2=$!

./tools/fileops.sh run fileops_indexer tools data/index1.db & PID3=$!

echo "asteptam $PID1, $PID2, $PID3"
wait $PID1 $PID2 $PID3
echo "indexare concurenta gata"

echo "generare snapshot 2"
cp data/index1.db data/index2.db
./tools/fileops.sh run fileops_indexer src data/index2.db

echo "test concurenta pe proc_snapshot"
./tools/fileops.sh run proc_snapshot data/proc1.db &
PID4=$!

./tools/fileops.sh run proc_snapshot data/proc1.db &
PID5=$!

./tools/fileops.sh run proc_snapshot data/proc1.db &
PID6=$!

wait $PID4 $PID5 $PID6
echo "gata snapshot"

sleep 2

./tools/fileops.sh run proc_snapshot data/proc2.db