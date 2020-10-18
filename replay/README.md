# mimic-replay

Mimic-replay replays traffic from an event file or from a made-up stream in
a congestion-responsive manner. The current version of the tool makes several assumptions:
* replay occurs between two peers
* each host owns IP addresses that are listed in its IPfile (cmdline arg)

## Usage

    mimic-replay syncConfig replayConfig IPConfig [options]

### Synchronization options

Mimic-replay first reads events from file or makes up a sequence of events
and then two peers synchronize. One peer is started a server and waits for
the client to contact it. After that replay starts on both peers. During
replay they try to loosely synchronize and ensure that one is not much
faster than the other

    syncConfig: -s | -c serverIP
     -s              this is the server, wait for connection to sync
     -c serverIP     this is a client, and will connect to serverIP to sync

### Replay from event file

replayConfig: -e eventFile | -m peerIPFile [-n numConns][-E numEvents][-b numBytes]
   -e eventFile    file with events to be replayed, usually obtained from mimic-extract
   -m peerIPFile   make up traffic to generate, using myIPFile and peerIPFile
   -n numConns     make up numConns parallel connections
   -E numEvents    make up numEvents sends per connection
   -b numBytes     each made-up send is numBytes large

IPConfig: -i IPFile
   -i myIPFile     file that specifies IPs on this machine, one per line

Options: [-h][-d][-t numThreads][-l logDir]
   -h              print this help message
   -d              turn on debug messages
   -t numThreads   use up to this number of threads
   -l logDir       if DEBUG flag is on, save output in this log directory



Mimic-replay the flow format described in [this document](../README.md).
The tool infers SEND and WAIT events from the dynamics of sequence and acknowledgment
numbers in the packets exchanged between peers. This inference is best-effort since
we lack information as to the ground truth of each application's behavior.

## Made-up traffic replay
## Event Inferrence Process

Each payload-bearing packet results in a SEND event. As optimization, multiple,
consecutive and closeby SEND events can be aggregated into one. This can speed
up the replay. Aggregation is enabled by
command-line flag -a followed by a parameter that defines how close SEND events need
to be for aggregation.

Packet that do not carry payload can result in a WAIT event, if they are acknowledging
data not previously acknowledged.

In real traces many flows are unidirectional, because the collection point does not fully
observe traffic the other direction. Mimic-extract infers the dynamics of the unobserved
direction from the dynamics of the observed one (e.g., infers what was sent from the
acknowledgment data).

To save resources we only extract payload-bearing flows.

When a flow is idle for DELTHRESH seconds (constant in the code, set to 600) we close it,
and will reopen it if the data starts flowing again.

## Usage

    mimic-extract [-c oneIP -s otherIP] [-a GAP] [-h] pcapfile

In the absence of -c and -s flags, original ports and IPs will be mined.
Otherwise, IPs will be overwritten with the IPs	you have specified
and duplicate client ports will be replaced by random other client ports.
This process is deterministic, so running the code on two different
machines will produce identical outputs. 

Additionally, if there are any ports on replay machines that are
reserved (e.g., 22), you can specify them in ports.csv file and they will be 
automatically replaced.

Flag -h prints the help message.

Flag -a followed by GAP, which is a number in decimal notation, denotes that consecutive
SEND events by the same IP within time GAP should be aggregated into one.

## Testing

File 202010011400.10K.pcap contains the first 10K TCP packets from MAWI samplepoint-F trace
provided by the WIDE project at: https://mawi.wide.ad.jp/mawi/samplepoint-F/2020/202010011400.html

Run

    mimic-extract 202010011400.10K.pcap

The output should be identical to 202010011400.10K.csv







