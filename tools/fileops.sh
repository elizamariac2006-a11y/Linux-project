#!/bin/bash
CFLAGS="-Itmp/scenariu_test_src/lib/include -std=c11 -Wall -Wextra"
#vad daca am parametrii
STARTTIME=$( date )
ENDTIME=$( date )
EXIT_CODE=0
TIMESTAMP_FORMAT=$(date +"%Y%m%d_%H%M%S")
COMANDA="$*"
LOG_FILE="logs/fileops_${TIMESTAMP_FORMAT}.log"

if [ $# -eq 0 ]; then 
   echo "Niciun parametru gasit.."
   ENDTIME=$( date )
   EXIT_CODE=1
   echo "Comanda : $COMANDA" >> "$LOG_FILE"
   echo "Start time : $STARTTIME" >> "$LOG_FILE"
   echo "End time : $ENDTIME" >> "$LOG_FILE"
   echo "Exit code : $EXIT_CODE" >> "$LOG_FILE"
   exit 1
fi

comanda=$1
if [ $comanda == "init" ]; then
   #bin src include data logs reports tmp tests doc tools
   eroare=0
   if [ ! -d "bin" ]; then
       mkdir -p "bin"
       echo "S a creat folderul bin"
       eroare=1
   else
       echo "Folderul bin deja existent"
   fi  
   if [ ! -d "src" ]; then
       mkdir -p "src"
       echo "S a creat folderul src"
       eroare=1
   else
       echo "Folderul src deja existent"
   fi  
   if [ ! -d "include" ]; then
       mkdir -p "include"
       echo "S a creat folderul include"
       eroare=1
   else
       echo "Folderul include deja existent"
   fi  
   if [ ! -d "data" ]; then
       mkdir -p "data"
       echo "S a creat folderul data"
       eroare=1
   else
       echo "Folderul data deja existent"
   fi  
   if [ ! -d "tmp" ]; then
       mkdir -p "tmp"
       echo "S a creat folderul tmp"
       eroare=1
   else
       echo "Folderul tmp deja existent"
   fi  
   if [ ! -d "logs" ]; then
       mkdir -p "logs"
       echo "S a creat folderul logs"
       eroare=1
   else
       echo "Folderul logs deja existent"
   fi  
   if [ ! -d "reports" ]; then
       mkdir -p "reports"
       echo "S a creat folderul reports"
       eroare=1
   else
       echo "Folderul reports deja existent"
   fi  
   if [ ! -d "tests" ]; then
       mkdir -p "tests"
       echo "S a creat folderul tests"
       eroare=1
   else
       echo "Folderul tests deja existent"
   fi  
   if [ ! -d "doc" ]; then
       mkdir -p "doc"
       echo "S a creat folderul doc"
       eroare=1
   else
       echo "Folderul doc deja existent"
   fi  
   if [ ! -d "tools" ]; then
       mkdir -p "tools"
       echo "S a creat folderul tools"
       eroare=1
   else
       echo "Folderul tools deja existent"
   fi  

   if ! command -v gcc >/dev/null 2>&1; then
       echo "gcc lipseste"
       ENDTIME=$( date )
       EXIT_CODE=1
       echo "Start time : $STARTTIME" >> "$LOG_FILE"
       echo "End time : $ENDTIME" >> "$LOG_FILE"
       echo "Exit code : $EXIT_CODE" >> "$LOG_FILE"
       exit 1 #FAIL
   else 
       echo "Exista gcc"
   fi

   if [ $eroare -eq 1 ]; then
       echo "Unul sau mai multe foldere nu existau.. acum ele exista"
       ENDTIME=$( date )
       EXIT_CODE=1
       echo "Comanda : $COMANDA" >> "$LOG_FILE"
       echo "Start time : $STARTTIME" >> "$LOG_FILE"
       echo "End time : $ENDTIME" >> "$LOG_FILE"
       echo "Exit code : $EXIT_CODE" >> "$LOG_FILE"
       exit 1 #FAIL
   fi
   
   ENDTIME=$( date )
   EXIT_CODE=0
   echo "Comanda : $COMANDA" >> "$LOG_FILE"
   echo "Start time : $STARTTIME" >> "$LOG_FILE"
   echo "End time : $ENDTIME" >> "$LOG_FILE"
   echo "Exit code : $EXIT_CODE" >> "$LOG_FILE"

   exit 0 #PASS
elif [ $comanda == "build" ]; then
   shift

   text=$1
   if [ "$text" == "--src" ]; then
       director=$2
   else 
      director="src"
   fi

   mkdir -p tmp/obj 
   for fisier in $( find "$director" -name "*.c" )
   do
       nume_fisier=$(basename "$fisier")
       nume_fara_extensie=${nume_fisier%.c}
       nume_o="tmp/obj/${nume_fara_extensie}.o" 
       
       if [ ! -f "$nume_o" ] || [ "$fisier" -nt "$nume_o" ] ; then
            echo "Se compileaza $nume_fisier"
            gcc $CFLAGS -c "$fisier" -o "$nume_o" -Wall -lssl -lcrypto -pthread
       fi
   done

   for fisier in $( find "$director" -name "main_*.c" )
   do
       nume_fisier=$(basename "$fisier")
       nume_fara_extensie=${nume_fisier%.c}
       nume_fara_main=${nume_fara_extensie#main_}
       echo "fisier care contine main : $nume_fisier"
       gcc "tmp/obj/${nume_fara_extensie}.o" -o "bin/${nume_fara_main}" -Wall -lssl -lcrypto -pthread
       chmod +x "bin/${nume_fara_main}"
   done
   ENDTIME=$( date )
   EXIT_CODE=0
   echo "Comanda : $COMANDA" >> "$LOG_FILE"
   echo "Start time : $STARTTIME" >> "$LOG_FILE"
   echo "End time : $ENDTIME" >> "$LOG_FILE"
   echo "Exit code : $EXIT_CODE" >> "$LOG_FILE"
   exit 0
elif [ $comanda == "run" ]; then
   shift
   
   if [ $# -lt 2 ]; then
       echo "Introdu numele executabilului"  
       ENDTIME=$( date )
       EXIT_CODE=1
       echo "Comanda : $COMANDA" >> "$LOG_FILE"
       echo "Start time : $STARTTIME" >> "$LOG_FILE"
       echo "End time : $ENDTIME" >> "$LOG_FILE"
       echo "Exit code : $EXIT_CODE" >> "$LOG_FILE"
       exit 1
   fi
   shift   

   fisier=$1
   executabil="bin/$fisier"
   shift
   if [ ! -x "$executabil" ]; then 
       echo "executabilul introdus nu exista sau nu poate fi executat"
       ENDTIME=$( date )
       EXIT_CODE=1
       echo "Comanda : $COMANDA" >> "$LOG_FILE"
       echo "Start time : $STARTTIME" >> "$LOG_FILE"
       echo "End time : $ENDTIME" >> "$LOG_FILE"
       echo "Exit code : $EXIT_CODE" >> "$LOG_FILE"
       exit 1
   fi
   
   "$executabil" "$@"
    ENDTIME=$( date )
    EXIT_CODE=0
    echo "Comanda : $COMANDA" >> "$LOG_FILE"
    echo "Start time : $STARTTIME" >> "$LOG_FILE"
    echo "End time : $ENDTIME" >> "$LOG_FILE"
    echo "Exit code : $EXIT_CODE" >> "$LOG_FILE"
    exit 0
elif [ $comanda == "clean" ]; then
   for fisier in $( find tmp/obj -name "*.o" )
   do
      rm "$fisier"
   done
   
   for fisier in bin/*
   do
       if [ -x "$fisier" ]; then 
          rm "$fisier"
       fi
   done
elif [ $comanda == "test" ]; then
   mkdir -p "reports"
   outputFile="reports/T2_tests.txt"
   contorFail=0
   echo "" > "$outputFile"
   echo "Run la sh-urile din /tests/"
   for fisier in $( find tests -name "*.sh" ) 
   do 
       bash "$fisier" 
       if [ $? -eq 0 ]; then
            echo "$fisier : PASS" >> "$outputFile"
       else
            echo "$fisier : FAIL" >> "$outputFile"
            contorFail=$((contorFail+1))
       fi
   done

   if [ $contorFail -ne 0 ]; then
       ENDTIME=$( date )
       EXIT_CODE=1
       echo "Comanda : $COMANDA" >> "$LOG_FILE"
       echo "Start time : $STARTTIME" >> "$LOG_FILE"
       echo "End time : $ENDTIME" >> "$LOG_FILE"
       echo "Exit code : $EXIT_CODE" >> "$LOG_FILE"
       exit 1
   else 
       ENDTIME=$( date )
       EXIT_CODE=0
       echo "Comanda : $COMANDA" >> "$LOG_FILE"
       echo "Start time : $STARTTIME" >> "$LOG_FILE"
       echo "End time : $ENDTIME" >> "$LOG_FILE"
       echo "Exit code : $EXIT_CODE" >> "$LOG_FILE"
       exit 0
   fi
fi
ENDTIME=$( date )
EXIT_CODE=0
echo "Comanda : $COMANDA" >> "$LOG_FILE"
echo "Start time : $STARTTIME" >> "$LOG_FILE"
echo "End time : $ENDTIME" >> "$LOG_FILE"
echo "Exit code : $EXIT_CODE" >> "$LOG_FILE"
exit 0
