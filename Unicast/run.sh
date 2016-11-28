#!/bin/sh

rm -rf files
mkdir files
touch files/activesenders.iprp

iptables -t mangle -F

for i in /proc/sys/net/ipv4/conf/*/rp_filter; do
	echo 2 > $i
done

bin/icd "$@"
