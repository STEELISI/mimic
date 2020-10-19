# mimic-replay

Mimic-replay replays traffic from an event file or from a made-up stream in
a congestion-responsive manner. The current version of the tool makes several assumptions:
* replay occurs between two peers
* each host owns IP addresses that are listed in its IPfile (cmdline arg)

We shift all the traffic by 10 seconds during extract, so replay should start roughly
10 seconds after you start the code. This is controled by parameter SHIFT in ../extract/mimic-extract.cc
for event file replay and by variable my_time in src/fileWorker.cc for made-up traffic replay.

** Note: you should run mimic-replay with sudo privileges since the replayed traffic
likely uses privileged port numbers **

## Usage

    mimic-replay syncConfig replayConfig IPConfig [options]

### Synchronization options

Mimic-replay first reads events from file or makes up a sequence of events
and then two peers synchronize. One peer is started a server and waits for
the client to contact it. After that replay starts on both peers. During
replay they try to loosely synchronize and ensure that one is not much
faster than the other.

The cmdline args below set up synchronization parameters:

    syncConfig: -s | -c serverIP
     -s              this is the server, wait for connection to sync
     -c serverIP     this is a client, and will connect to serverIP to sync

### Replay from event file

Mimic-replay assumes that event file is obtained by running mimic-extract
on a pcap file to extract TCP flow information. The flow format described
in [this document](../README.md). It is also possible to write a small program
to generate flows in the given format, with any desired dynamics. Care should
be taken that each flow balances SEND and WAIT events, i.e., that for each peer in
the flow the sum of bytes in SEND statements is equal to the sum of bytes in
WAIT statements of the other peer. Both peers must also have CLOSE statements
as the last statement on the flow. If flows are constructed that deviate from
these requirements replay will eventually stall.

The cmdline args below set up event replay parameters:

    replayConfig: -e eventFile 
     -e eventFile    file with events to be replayed, usually obtained from mimic-extract

### Made-up replay

Mimic-replay can also make up traffic as it goes. This process is controlled by three
arguments: the number of parallel connections (numConns), the number of bytes per SEND
(numBytes), and the number of events per connection (numEvents). Assume two peers
replaying traffic are A and B. Each event looks like this:
  A sends numBytes to B
  B waits for numBytes
  B sends numBytes to A
  A waits for numBytes

The cmdline args below control made-up traffic replay

    replayConfig: -m peerIPFile [-n numConns][-E numEvents][-b numBytes]
     -m peerIPFile   generate make up traffic between the first IP in myIPFile and in peerIPFile
     -n numConns     make up numConns parallel connections
     -E numEvents    make up numEvents sends per connection
     -b numBytes     each made-up send is numBytes large

### IP arguments

IP file is currently the required argument for mimic-replay. It lists all IPs assigned to
the given peer. 

    IPConfig: -i IPFile
     -i myIPFile     file that specifies IPs on this machine, one per line

### Other options

Here are other options supported by mimic-replay
    Options: [-h][-d][-t numThreads][-l logDir]
     -h              print this help message
     -d              turn on debug messages
     -t numThreads   use up to this number of threads
     -l logDir       if DEBUG flag is on, save output in this log directory

Debug messages are very verbose and will slow down replay by at least 3-4 times.
The messages go into logDir. FileWorker messages are stored in file.<hostname>
and EventHandler messages are stored in thread.<hostname>.<threadnum>.

You can set the number of threads to use to replay traffic. Default is 5, which
gave us best results. You can experiment with a bit higher or lower values. If you
go past 50, replay usually slows down a lot, and most time is spent on thread
switching.

### Output

As code runs, it will print out the total throughput (sent+recvd bytes) and
total number of events processed each second. It also prints average
delay per event (compared to the time when this event was planned to occur).
Once you kill the program, it will also print out each connection's statistics,
such as when it started, when it completed, what was its final state and
its cumulative delay.

## Testing

Assume there are two machines we will be using for replay, with IP addresses
10.1.1.2 (machine a) and 10.1.1.3 (machine b)

* Process file testing/202010011400.10K.pcap as follows

    mimic-extract -s 10.1.1.2 -c 10.1.1.3 testing/202010011400.10K.pcap > testing/202010011400.10K.rew.csv

* Copy the file 202010011400.10K.rew.csv to both machines

3. On A run

   echo "10.1.1.2" > a.ips

4. On B run

    echo "10.1.1.3" > b.ips

5. On A machine run:

     sudo mimic-replay -c 10.1.1.3 -i a.ips -e 202010011400.10K.rew.csv 

6. On B machine run:

     sudo mimic-replay -s -i b.ips -e 202010011400.10K.rew.csv

Output should look like this:

    Successfully completed 0/704 avg delay 0 ms, throughput 0 Gbps, events 0
    Successfully completed 0/704 avg delay 0 ms, throughput 0 Gbps, events 0
    Successfully completed 0/704 avg delay 0 ms, throughput 0 Gbps, events 0
    Successfully completed 0/704 avg delay 0 ms, throughput 0 Gbps, events 0
    Successfully completed 0/704 avg delay 0 ms, throughput 0 Gbps, events 0
    Successfully completed 0/704 avg delay 0 ms, throughput 0 Gbps, events 1235
    Successfully completed 0/704 avg delay 0 ms, throughput 0 Gbps, events 0
    Successfully completed 0/704 avg delay 0 ms, throughput 0 Gbps, events 0
    Successfully completed 0/704 avg delay 0 ms, throughput 0 Gbps, events 14
    Successfully completed 2/704 avg delay 0.5 ms, throughput 0.0319403 Gbps, events 2932
    Successfully completed 704/704 avg delay 1.52699 ms, throughput 0 Gbps, events 866

Connection statistics look like this:

    Conn 0 state DONE total events 3 last event 2 delay 0.001 s, started 10 completed 11.31
    Conn 2 state DONE total events 3 last event 2 delay 0.002 s, started 10.001 completed 11.292
    Conn 4 state DONE total events 3 last event 2 delay 0.003 s, started 10.001 completed 11.007
   







