#!/bin/bash

init (){
    if ! command -v gcc &> /dev/null; then
    echo "Eroare, gcc nu a exista."
    exit 1
    fi

    dirs=("bin" "src" "include" "data" "logs" "reports" "tmp" "tests" "doc" "tools")

    for dir in "${dirs[@]}"; do
        if [ ! -d "$dir" ]; then
            mkdir -p "./$dir"
            echo "creat: ./$dir/"
        else
            echo "./$dir/ exista"
        fi
    done

    echo "Initializat"
    return 0
}

build (){
    SRC_DIR="src"

    while [[ $# -gt 0 ]]; do
        case $1 in
            --src)
                SRC_DIR="$2"
                shift 2
                ;;
            *)
                shift
                ;;
        esac
    done

    mkdir -p tmp/obj

    while IFS= read -r FILE_PATH; do
        FILENAME=$(basename "$FILE_PATH")
        OBJ_NAME="${FILENAME%.c}.o"
        OBJ_PATH="tmp/obj/$OBJ_NAME"

        if [ ! -f "$OBJ_PATH" ] || [ "$FILE_PATH" -nt "$OBJ_PATH" ]; then
            echo "compilare: $FILENAME -> $OBJ_PATH"
            gcc $CFLAGS -c "$FILE_PATH" -o "$OBJ_PATH" -Iinclude
                if [ $? -ne 0 ]; then
                    echo "Eroare la compilare $FILENAME"
                    return 1
                fi
        fi
    done < <(find "$SRC_DIR" -name "*.c")

    SHARED_OBJ=$(find tmp/obj -name "*.o" ! -name "main_*.o" 2>/dev/null)

    for MAIN_OBJ in tmp/obj/main_*.o; do
        if [ -f "$MAIN_OBJ" ]; then 
            EXE_NAME=$(basename "$MAIN_OBJ" | sed 's/^main_//;s/\.o$//')
            EXE_PATH="bin/$EXE_NAME"

            echo "editat exec $EXE_PATH"

            gcc $CFLAGS "$MAIN_OBJ" $SHARED_OBJ -o "$EXE_PATH"

            if [ $? -eq 0 ]; then
                echo "stergere: $MAIN_OBJ"
                rm -f "$MAIN_OBJ"
            else
                echo "eroare asamblare $EXE_NAME"
                return 1
            fi
        fi
    done
}

run() {
    if [ "$1" = "--" ]; then
        shift
    fi

    if [ -z "$1" ]; then
        echo "specifica numele executabilei"
        exit 1
    fi

    EXE_NAME=$1
    shift

    EXE_PATH="bin/$EXE_NAME"

    if [ ! -f "$EXE_PATH" ]; then
        echo "executabila '$EXE_PATH' nu a fost gasita, ai uitat sa dai build?"
        exit 1
    fi

    if [ ! -x "$EXE_PATH" ]; then
        echo "'$EXE_PATH' nu are drept de executie"
        exit 1
    fi

    exec "$EXE_PATH" "$@"
}

clean() {
    echo "stergere"

    rm -f bin/* 2>/dev/null
    rm -f tmp/obj/* 2>/dev/null

    echo "gata"
}

clean-logs() {
    echo "curatare logs..."

    rm -f logs/* 2>/dev/null

    echo "gata"
}

clean-data() {
    echo "curatare data..."

    rm -f data/* 2>/dev/null

    echo "gata"
}

test() {
    REPORTS="reports/T2_tests.txt"

    if [ ! -d "reports" ]; then 
        mkdir -p reports
    fi

    > "$REPORTS"

    GLOBAL_EXIT_CODE=0

    TEST_FILES=$(find tests -type f -name "*.sh" | sort)

    if [ -z "$TEST_FILES" ]; then
        echo "nu avem fisiere in tests"
        return 0
    fi

    echo "incepem testele..."

    for t_script in $TEST_FILES; do
        bash "$t_script" > /dev/null 2>&1
        TEST_RESULT=$?

        if [ $TEST_RESULT -eq 0 ]; then
            echo "PASS: $t_script" >> "$REPORTS"
            echo "PASS: $t_script"
        else
            echo "FAIL: $t_script" >> "$REPORTS"
            echo "FAIL: $t_script (cod: $TEST_RESULT)"
            GLOBAL_EXIT_CODE=1
        fi
    done

    echo "gata. raport generat in $REPORTS"

    return $GLOBAL_EXIT_CODE
}

#cu 2> mut erorile in /dev/null

#clean de stres fisiere, nu ala cerut
: 'clean (){
    dirs=("bin" "src" "include" "data" "logs" "reports" "tmp" "tests" "doc" "tools")

    for dir in "${dirs[@]}"; do
        if [ -d "$dir" ]; then
            rm -r "$dir"
            echo "sters: $dir/"
        else
            echo "$dir/ nu exista"
        fi
    done
}'

SUBCOM=$1
START_TIME=$(date +%s)
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
LOG_FILE="logs/fileops_${TIMESTAMP}.log"

mkdir -p logs

case "$SUBCOM" in
    init)
        echo "exec init..."
        init
        ;;

    clean)
        echo "exec clean..."
        clean
        ;;

    build)
        echo "exec build..."
        build "$@"
        ;;

    run)
        echo "exec run..."
        shift
        run "$@"
        ;;
    test)
        echo "test run..."
        test
        ;;
    help)
        echo "init: creeaza fisierele"
        echo "build: compileaza si asambleaza sursele din [file]"
        echo "clean: sterge continutul din bin si tmp/obj"
        echo "run [exec]: ruleaza executabila primita ca argument"
        echo "test: ruleaza scripturile din tests si produce rezultatul acestora in tests"
        echo "clean-logs: sterge continutul fisierului logs(in afara de apelul clean-logs in sine)"
        echo "clean-data: sterge continutul fisierului data(in afara de apelul clean-data in sine)"
        ;;
    clean-logs)
        clean-logs
        ;;
    clean-data)
        clean-data
        ;;
    *)
        echo "subcomanda gresita/inexistenta"
        exit 1
        ;;
esac

EXIT_CODE=$?

END_TIME=$(date +%s)
DUR=$((END_TIME - START_TIME))

{
    echo "Subcomanda rulata: $SUBCOM"
    echo "Timestamp start: $START_TIME"
    echo "TImestamp end: $END_TIME"
    echo "Durata: ${DUR}s"
    echo "EXIT CODE: $EXIT_CODE"
} > "$LOG_FILE"

exit $EXIT_CODE