#!/bin/bash
rm -rf bin
mkdir bin

gcc src/icd/* src/lib/* -o bin/icd -std=c99 -I inc/ -lpthread -lnfnetlink -lnetfilter_queue -Wfatal-errors
gcc src/isd/* src/lib/* -o bin/isd -std=c99 -I inc/ -lpthread -lnfnetlink -lnetfilter_queue -Wfatal-errors
gcc src/ird/* src/lib/* -o bin/ird -std=c99 -I inc/ -lpthread -lnfnetlink -lnetfilter_queue -Wfatal-errors
gcc src/imd/* src/lib/* -o bin/imd -std=c99 -I inc/ -lpthread -lnfnetlink -lnetfilter_queue -Wfatal-errors