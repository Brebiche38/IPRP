#!/bin/bash
gcc src/icd/* src/lib/* -o bin/icd -std=c99 -lpthread -Wfatal-errors
