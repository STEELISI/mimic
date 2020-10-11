# mimic-extract

Mimic-extract tool extracts application data units (ADUs) from TCP flows in a pcap trace.
Other protocols are ignored.

Mimic-extract tool uses the flow format described in [this document](../README.md).
The tool infers SEND and WAIT events from the dynamics of sequence and acknowledgment
numbers in the packets exchanged between peers. This inference is best-effort since
we lack information as to the ground truth of each application's behavior.

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





