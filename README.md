# iPRP-IPv4
Implementation of the iPRP protocol for IPv4 hosts

Two versions, one for Unicast packets and one for Multicast packets, are available in their corresponding directories.

Compilation: sh compile.sh
Usage: bin/icd n a1 [a2 ...]
- n: number of interfaces for the host
- a1, a2, ...: IP address of each interface in order

Example: bin/icd 2 10.0.1.1 10.0.2.1
