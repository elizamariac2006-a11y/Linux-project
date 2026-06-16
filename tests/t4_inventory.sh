#!/bin/bash

echo "Initializare mediu de testare"

TEST_DIR="test_scenario_t4"
IPC_FILE="data/ipc.mmap"
DB_FILE="data/inventory.db"
WORKERS=2 

# Curatare rulari anterioare
rm -rf "$TEST_DIR" "$IPC_FILE" "$DB_FILE"
mkdir -p "$TEST_DIR/folder_a"
mkdir -p "$TEST_DIR/folder_b/folder_c"
mkdir -p "data"

# Creez 3 fisiere regulate si un symlink (care trebuie ignorat)
echo "continut 1" > "$TEST_DIR/f1.txt"
echo "continut 2" > "$TEST_DIR/folder_a/f2.txt"
echo "continut 3" > "$TEST_DIR/folder_b/folder_c/f3.bin"
ln -s "$TEST_DIR/f1.txt" "$TEST_DIR/symlink_ignorat.txt"

EXPECTED_FILES=3

echo "Rulare fileops_manager: "

./tools/fileops.sh run -- fileops_manager \
    --root "$TEST_DIR" \
    --workers "$WORKERS" \
    --ipc "$IPC_FILE" \
    --db "$DB_FILE"

if [ $? -ne 0 ]; then
    echo "EROARE: Executia managerului a esuat"
    exit 1
fi

echo "Validare rezultate:"

# Verificare existenta DB
if [ ! -f "$DB_FILE" ]; then
    echo "EROARE: Baza de date binara '$DB_FILE' nu a fost creata"
    exit 1
fi

# Verificare structurala folosind --verify
echo "-> Rulare: --verify"
./tools/fileops.sh run -- fileops_manager --db "$DB_FILE" --verify
if [ $? -ne 0 ]; then
    echo "EROARE: Flag-ul --verify a raportat un DB invalid sau corupt"
    exit 1
fi

#  Sumarizare folosind --dump
echo "-> Rulare: --dump"
DUMP_OUTPUT=$(./tools/fileops.sh run -- fileops_manager --db "$DB_FILE" --dump)

echo "Output --dump:"
echo "$DUMP_OUTPUT"

#  Validare campuri din dump 
# Verificare Magic
if ! echo "$DUMP_OUTPUT" | grep -q "magic = INV4"; then
    echo "EROARE: Campul 'magic' nu corespunde cu 'INV4'."
    exit 1
fi

# Verificare statusul complete
if ! echo "$DUMP_OUTPUT" | grep -q "complete = 1"; then
    echo "EROARE: Flag-ul 'complete' nu este 1. Procesarea nu s-a terminat normal"
    exit 1
fi

# Verificare numarul de fisiere
if ! echo "$DUMP_OUTPUT" | grep -q "file_record_count = $EXPECTED_FILES"; then
    echo "EROARE: 'file_record_count' incorect! Asteptat: $EXPECTED_FILES."
    exit 1
fi

# Verificare numarul de workeri inregistrati
if ! echo "$DUMP_OUTPUT" | grep -q "worker_count = $WORKERS"; then
    echo "EROARE: 'worker_count' incorect! Workeri asteptati: $WORKERS."
    exit 1
fi

echo "Toate verificările au trecut cu succes! (PASS)"

# Curatare finala dupa succes
rm -rf "$TEST_DIR" "$IPC_FILE" "$DB_FILE"
exit 0
