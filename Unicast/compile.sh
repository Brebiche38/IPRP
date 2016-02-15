#!/bin/bash
rm -rf bin
mkdir bin

gcc src/icd/* src/lib/* -o bin/icd -std=c99 -lpthread -Wfatal-errors
