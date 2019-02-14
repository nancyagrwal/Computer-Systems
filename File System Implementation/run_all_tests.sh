#!/usr/bin/env bash

make all COVERAGE=1 > /dev/null # We don't want to display output every time

# chmod +x ./test-q1.py ./test-q2.py ./test-q1.sh
# Uncomment if you run into permission errors somehow

# gdb -ex run --args python3 ./test-q1.py
# gdb -ex run --args python3 ./test-q2.py

if [[ "$1" = "-v" ]] ; then
  cmd="valgrind -q --suppressions=./valgrind-python.supp --leak-check=full"
  echo "Running with valgrind"
else
  cmd=""
  echo "NOTE: Run with -v to enable valgrind"
fi

${cmd} python3 ./test-q1.py
${cmd} python3 ./test-q2.py

./mktest -extra test.img
bash test-q1.sh test.img
rm test.img

gcov homework.c
cp  homework.c.gcov coverage.txt
echo "NOTE: Coverage can be found in coverage.txt"

make clean > /dev/null

