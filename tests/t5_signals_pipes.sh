#!/bin/bash

# Cai temporare pentru test
TEST_DIR="tmp/director_test_mare"
DB_FILE="tmp/test_f5.db"
PID_FILE="tmp/manager_test.pid"

# Curat rularile anterioare 
rm -rf "$TEST_DIR" "$DB_FILE" "$PID_FILE"
mkdir -p "$TEST_DIR"

# Adaug si cateva subdirectoare ca sa tinem workerii ocupati si cu job-uri in coada
for j in {1..5}; do
    mkdir -p "$TEST_DIR/subdir_$j"
    for i in {1..20}; do
        echo "Continut fictiv fisier $i" > "$TEST_DIR/subdir_$j/file_$i.txt"
    done
done

echo "TEST: Pornesc managerul in fundal"

# Pornesc managerul cu o intarziere per director de 1000ms ca sa apucam sa dam KILL
./bin/fileops_manager --root "$TEST_DIR" --workers 3 --db "$DB_FILE" --pid-file "$PID_FILE" --simulate-work-ms 1000 &
MGR_PROC_PID=$!

# Astept ca managerul sa-si faca setup-ul initial (mmap, pipes) si sa scrie pid_file
sleep 0.5

if [ ! -f "$PID_FILE" ]; then
    echo "FAIL: Fisierul PID nu a fost creat de manager!"
    kill -9 $MGR_PROC_PID 2>/dev/null
    exit 1
fi

REAL_PID=$(cat "$PID_FILE")
echo "TEST: PID-ul citit din fisier este: $REAL_PID"

echo "TEST: Trimit SIGUSR1 pentru a verifica raportarea STATUS"
# Trimit semnalul de status (ar trebui sa se printeze "STATUS queued_jobs=... active_jobs=...")
kill -USR1 "$REAL_PID"

# Il las putin sa mai proceseze din directoare (sa umple pipe-urile si DB-ul temporar)
sleep 1

echo "TEST: Trimit SIGTERM pentru shutdown gratios"
kill -TERM "$REAL_PID"

# Astept ca procesul manager sa se termine complet gratios (wait preia exit code-ul)
wait $MGR_PROC_PID
echo "TEST: Managerul s-a oprit."

echo "TEST: Verific daca baza de date a fost creata"
if [ ! -f "$DB_FILE" ]; then
    echo "FAIL: Fisierul DB binar final nu a fost generat!"
    exit 1
fi

echo "TEST: Rulez managerul cu --dump pentru a verifica flag-ul complete = 0"
DUMP_OUT=$(./bin/fileops_manager --db "$DB_FILE" --dump)

# Caut textul exact asa cum il afiseaza managerul
echo "$DUMP_OUT" | grep "complete = 0" > /dev/null
if [ $? -ne 0 ]; then
    echo "FAIL: Eroare: Baza de date ar fi trebuit sa aiba complete = 0 (fiind oprita prin semnal)!"
    echo "Output primit de la dump a fost:"
    echo "$DUMP_OUT"
    exit 1
fi
echo "PASS: Corect: Dump-ul bazei de date contine complete = 0."

echo "TEST: Rulez managerul cu --verify (trebuie sa dea succes/0 chiar daca e complet = 0)"
./bin/fileops_manager --db "$DB_FILE" --verify
VERIFY_STATUS=$?

if [ $VERIFY_STATUS -ne 0 ]; then
    echo "FAIL: Comanda --verify a esuat (exit code $VERIFY_STATUS). Ar fi trebuit sa dea succes 0!"
    exit 1
fi
echo "PASS: Comanda --verify a intors succes (0) pe un DB valid binar dar incomplet."

echo "SUCCESS: Toate testele de control plane (semnale/pipe/DB) au trecut!"
exit 0
