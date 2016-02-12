#!/bin/bash
gcc src/icd/* src/lib/* -o bin/icd -std=c11 -lpthread -Wfatal-errors
