# iPRP-IPv4
Implementation of the iPRP protocol for IPv4 hosts

Two versions, one for Unicast packets and one for Multicast packets, are available in their corresponding directories.

Compilation (Unicast version): compile.sh
Compilation (Multicast version): compile_multicast.sh

Usage: run.sh n a1 [a2 ...]
- n: number of interfaces for the host
- a1, a2, ...: IP address of each interface in order

Example: run.sh 2 10.0.1.1 10.0.2.1
