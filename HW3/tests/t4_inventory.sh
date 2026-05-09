#!/bin/bash

TEST_DIR="tmp/test_t4_scenario"
TEST_DB="data/test_t4_inventory.db"
TEST_IPC="data/test_t4_ipc.mmap"

rm -rf "$TEST_DIR" "$TEST_DB" "$TEST_IPC"
mkdir -p "$TEST_DIR/dir1/dir2"
echo "continut1" > "$TEST_DIR/file1.txt"
echo "continut2" > "$TEST_DIR/dir1/file2.txt"
echo "continut3" > "$TEST_DIR/dir1/dir2/file3.txt"
# am creat 3 fisiere regulate

# rulam direct cu 2 workeri
./bin/fileops_manager --root "$TEST_DIR" --workers 2 --ipc "$TEST_IPC" --db "$TEST_DB"
if [ $? -ne 0 ]; then
    echo "FAIL: eroare la rularea fileops_manager"
    exit 1
fi

# verificam daca db exista
if [ ! -f "$TEST_DB" ]; then
    echo "FAIL: $TEST_DB nu a fost creat"
    exit 1
fi

# rulam --verify pe DBul generat
./bin/fileops_manager --db "$TEST_DB" --verify
if [ $? -ne 0 ]; then
    echo "FAIL: --verify a returnat eroare"
    exit 1
fi

# rulam dump si filtram datele stabile
DUMP_OUT=$(./bin/fileops_manager --db "$TEST_DB" --dump)

# verificam magic=INV4
if ! echo "$DUMP_OUT" | grep -q "magic=INV4"; then
    echo "FAIL: magic din dump diferit de INV4"
    exit 1
fi

# verificam complete=1
if ! echo "$DUMP_OUT" | grep -q "complete=1"; then
    echo "FAIL: complete incorect in dump"
    exit 1
fi

# verificam file_record_count=3 (numarul de fisiere create mai sus)
if ! echo "$DUMP_OUT" | grep -q "file_record_count=3"; then
    echo "FAIL: file_record_count incorect"
    exit 1
fi

# verificam worker_count=2
if ! echo "$DUMP_OUT" | grep -q "worker_count=2"; then
    echo "FAIL: worker_count incorect"
    exit 1
fi

# curatam fisierele temporare generate la succes
rm -rf "$TEST_DIR" "$TEST_DB" "$TEST_IPC"

echo "PASS: Toate cerintele T4 au fost indeplinite cu succes!"
exit 0
