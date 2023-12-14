#!/bin/bash

# The C program to test
C_PROGRAM="./fcheck"

# Array of test programs
TEST_PROGRAMS=("addronce" "addronce2" "badaddr" "badfmt" "badindir1" "badindir2" "badinode" "badlarge" "badrefcnt" "badrefcnt2" "badroot" "badroot2" "dironce" "good" "goodlarge" "goodlink" "goodrefcnt" "goodrm" "imrkfree" "imrkused" "indirfree" "mismatch" "mrkfree" "mrkused")

# Iterate over all test programs
for TEST_PROGRAM in ${TEST_PROGRAMS[@]}
do
  echo "Running test program: $TEST_PROGRAM"
  
  # Run the C program with the test program as argument and print the output
  $C_PROGRAM $TEST_PROGRAM
done
