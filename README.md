# Mimic

Mimic is a set of tools for congestion-responsive TCP flow replay. The extract directory
contains mimic-extract tool, which extracts TCP flow information from pcap files. The replay
directory contains mimic-replay tool, which uses flow information to generate flows
using TCP sockets. Mimic-replay can also "make up" traffic based on configuration parameters.

## Flow format

A flow in mimic is represented as one CONN event (start of the flow), multiple SEND/WAIT
events (one peer sends some amount of bytes, and the other waits for it), and two CLOSE
events (end of the flow). Peers can send traffic to each other in parallel (many SENDS on
each side, followed by a single WAIT) or in a lockstep way (one peer sends a chunk, and
the second waits for it, then the second sends a chunk and the first peer waits for it).

Each event has 8 fields. CONN events have the following fields:

    CONN,cid,srcIP,srcport,->,dstIP,dstport,stime

where *cid* is a unique ID of that connection, *srcIP/dstIP* are the peer IP addresses,
*srcport/dstport* are peer ports, and *stime* denotes the relative time when the connection
starts (from the start of the trace, or from the start of replay).