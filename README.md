SYSC4001 Assignment 3 – Part 2: Concurrent Exam Marking System
Overview

This project simulates a concurrent exam marking system with multiple TAs marking exams simultaneously.

Part A: Simple shared memory implementation (no semaphores – race conditions may occur)

Part B: Uses System V semaphores to synchronize marking, ensuring safe concurrent access

Files

Pt2A_101295239_101296153.c – Part A: unsafe concurrency

Pt2B_101295239_101296153.c – Part B: safe concurrency with semaphores

rubric.txt – Initial rubric for 5 questions

exam_01.txt … exam_20.txt – Sample exams

create_exams.sh – Script to generate sample exams

README.md – Instructions

SYSC 4001 Assignment 3 Part 2 Report.pdf – Assignment report

Requirements

Linux environment

GCC compiler

Compilation
gcc -o marking_system_partA Pt2A_101295239_101296153.c
gcc -o marking_system_partB Pt2B_101295239_101296153.c

Running the Program
./marking_system_partB <number_of_TAs>


<number_of_TAs> – integer ≥ 2

Example:

./marking_system_partB 4

Test Cases
1. Standard run

4 TAs marking 20 exams

Expected: All exams marked; rubric may be updated occasionally

2. Termination exam

Add "9999" in an exam file (e.g., exam_10.txt)

Expected: Program stops marking after this exam

3. Part A (unsafe concurrency)
./marking_system_partA 3


Expected: Observe potential race conditions when multiple TAs access shared data

Notes

All exam_XX.txt files and rubric.txt must be in the same directory as the executables

Shared memory and semaphores are automatically cleaned up after execution

Ensure at least 2 TAs are used to observe concurrency behavior
