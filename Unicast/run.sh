#!/bin/sh

rm -rf files
mkdir files
touch files/activesenders.iprp

iptables -t mangle -F

bin/icd "$@"
