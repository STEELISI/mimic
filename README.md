# Mimic

Mimic is a set of tools for congestion-responsive TCP flow replay. The *extract* directory
contains *mimic-extract* tool, which extracts TCP flow information from pcap files. The *replay*
directory contains *mimic-replay* tool, which uses flow information to generate flows
using TCP sockets. Mimic-replay can also "make up" traffic based on configuration parameters.

## Requirements

Extraction tool requires libtrace, which can be installed from
https://github.com/LibtraceTeam/libtrace.

## Building

Run
    cmake build .
    make
    sudo make install

If you don't have cmake you can install it by running

   cmake-install.sh

in the top directory
   
## Flow format

A flow in mimic is represented as one CONN event (start of the flow), multiple SEND/WAIT
events (one peer sends some amount of bytes, and the other waits for it), and two CLOSE
events (end of the flow). Peers can send traffic to each other in parallel (many SENDS on
each side, followed by a single WAIT) or in a lockstep way (one peer sends a chunk, and
the second waits for it, then the second sends a chunk and the first peer waits for it).

CONN events have the following fields:

    CONN,fid,srcIP,srcport,->,dstIP,dstport,stime

where *fid* is a unique ID of that flow, *srcIP/dstIP* are the peer IP addresses,
*srcport/dstport* are peer ports, and *stime* denotes the relative time (in seconds) when
the flow starts (from the start of the trace, or from the start of replay).

Other events have the following fields:

    EVENT,fid,eid,IP,etype,bytes,rtime,stime

where *fid* is a unique ID of the flow, *eid* is a unique ID of the event within the
flow, *IP* is the IP address of the peer performing the event, *etype* is SEND, WAIT or CLOSE,
*bytes* is the number of bytes in SEND and WAIT events (this field is ignored in CLOSE events),
*rtime* is the relative time (in seconds) from the previous event by the same IP (currently
unused), and *stime* denotes the relative time (in seconds) when the flow starts (from the
start of the trace, or from the start of replay).

Mimic currently replays events at the time denoted by *stime* field. In networks this timing
is affected both by application delays and by network propagation delays. In future releases
we plan to separate these two issues so we can emulate network delays separately, and replay
data at the application level driven only by application delays.

If you have any feedback about this tool, please contact us at <mimic@mailman.isi.edu>.