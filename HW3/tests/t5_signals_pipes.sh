#!/bin/bash

#facem test tree
TEST_DIR="tmp/test_tree"
mkdir -p "$TEST_DIR"
for i in {1..20}; do
    mkdir -p "$TEST_DIR/dir_$i"
    for j in {1..5}; do
        echo "test data $i $j" > "$TEST_DIR/dir_$i/file_$j.txt"
    done
done

PID_FILE="tmp/manager.pid"
DB_FILE="data/inventory.db"
rm -f "$PID_FILE" "$DB_FILE"

# rulare manager cu sleep si timeout gratios
./tools/fileops.sh run -- fileops_manager \
    --root "$TEST_DIR" --workers 3 \
    --pid-file "$PID_FILE" --db "$DB_FILE" \
    --simulate-work-ms 50 --graceful-timeout 5 > tmp/mgr_out.txt 2>&1 &

# polling pana pid este scris
while [ ! -f "$PID_FILE" ]; do
    sleep 0.1
done
MANAGER_PID=$(cat "$PID_FILE")

# SIGUSR test
kill -USR1 "$MANAGER_PID"
sleep 0.5

#SIGTERM test
kill -TERM "$MANAGER_PID"

while kill -0 "$MANAGER_PID" 2>/dev/null; do
    sleep 0.1
done

# verificare log 
if ! grep -q "STATUS queued_jobs=" tmp/mgr_out.txt; then
    echo "FAIL: SIGUSR1 nu a scris linia de STATUS."
    exit 1
fi

# baza de date completa si valida
./tools/fileops.sh run -- fileops_manager --db "$DB_FILE" --verify
if [ $? -ne 0 ]; then
    echo "FAIL: Baza de date corupta"
    exit 1
fi

COMPLETE_FLAG=$(./tools/fileops.sh run -- fileops_manager --db "$DB_FILE" --dump | grep "complete=" | cut -d= -f2)
if [ "$COMPLETE_FLAG" -ne 0 ]; then
    echo "FAIL: DB ar trebui sa fie marcat cu complete=0 deoarece a fost întrerupt de SIGTERM!"
    exit 1
fi

echo "PASS: cerintele au fost indeplinite"

# cleanup
rm -rf "$TEST_DIR" "$PID_FILE" tmp/mgr_out.txt
exit 0
