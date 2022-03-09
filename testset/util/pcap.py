from datetime import datetime
from struct import unpack, unpack_from, pack
from scapy.all import *

price_divide = 100000


class PcapGen:
    def __init__(self, pkt_template_path="./data/cme_input_arb.pcap",pcap_path="cme_input_gen.pcap"):

        self.writers = PcapWriter(pcap_path)
        # path of pkt_template
        pcap = rdpcap(pkt_template_path)
        pkt_template = pcap[0]
        del pcap
        self.template = pkt_template
        self.payload = None
        self.MsgSeqNum = 0

    def construct_payload(self, Timestamp, MDEntryType, SecurityID, MDEntryPx):
        template_payload = self.template['Raw'].load
        self.payload = b''
        self.payload += pack('<I', self.MsgSeqNum)
        self.payload += pack('<Q', Timestamp)
        self.payload += template_payload[12:26]
        # Transact time is set same as sending time
        self.payload += pack('<Q', Timestamp)
        self.payload += template_payload[34:40]
        self.payload += pack('<q', MDEntryPx)
        # AAT ignores MDEntrySize (Quantity) so it could be set to arbitrary value
        self.payload += template_payload[48:52]
        self.payload += pack('<i', SecurityID)
        # RptSeq is set same as MsgSeqNum
        self.payload += pack('<I', self.MsgSeqNum)
        # AAT ignores Number of orders
        # Price level is always set to 0 (market price)
        self.payload += template_payload[60:66]
        # MDEntryType 48=>bid 49=>ask
        self.payload += pack('<B', MDEntryType)
        self.payload += template_payload[67:]
        assert(len(self.payload) == len(template_payload))
        # Increment the sequence number
        self.MsgSeqNum += 1

    def write_pcap(self):
        assert(self.payload != None)
        pkt = Ether(raw(self.template))
        pkt['Raw'].load = self.payload
        self.writers.write(pkt=pkt)


def timestamp2str(timestamp):
    # timestamp: nano seconds since 1970/01/01 00:00
    datestr = datetime.fromtimestamp(timestamp/10**9)  # function takes seconds
    return f"{datestr.strftime('%Y/%m/%d %H:%M:%S')}.{timestamp % (10**9)}"


def print_decode_seq(payload):
    print("--------------Start for a packet--------")
    print("MsgSeqNum: ", unpack("<I", payload[0:4])[0])
    print("SendingTime: ", timestamp2str(unpack("<Q", payload[4:12])[0]))
    print("MsgSize: ", unpack("<H", payload[12:14])[0])
    print("BlockLength: ", unpack("<H", payload[14:16])[0])
    print("TemplateID: ", unpack("<H", payload[16:18])[0])
    print("SchemaID: ", unpack("<H", payload[18:20])[0])
    print("Version: ", unpack("<H", payload[20:22])[0])
    print("MsgType: ", unpack("<I", payload[22:26])[0])
    print("TransactTime: ", timestamp2str(unpack("<Q", payload[26:34])[0]))
    print("MatchEventIndicator: ", payload[34])
    # two zero padding bytes
    assert(unpack("<H", payload[35:37])[0] == 0)
    # print("ZeroPad: ",unpack("<H",payload[35:37])[0])
    print("NoMDEntries_blockLength: ", unpack("<H", payload[37:39])[0])
    print("NoMDEntries_numInGroup: ", payload[39])
    # exponent is in aat_defines.hpp
    print("MDEntryPx: ", unpack("<q", payload[40:48])[0]/price_divide)
    print("MDEntrySize: ", unpack("<i", payload[48:52])[0])
    print("SecurityID: ", unpack("<i", payload[52:56])[0])
    print("RptSeq: ", unpack("<I", payload[56:60])[0])
    print("NumberOfOrders: ", unpack("<i", payload[60:64])[0])
    assert(unpack_from("<B", payload, 64)[0] == payload[64])
    print("MDPriceLevel: ", unpack_from("<B", payload, 64)[0])
    print("MDUpdateAction: ", unpack_from("<b", payload, 65)[0])
    print("MDEntryType: ", unpack_from("<B", payload, 66)[0])
    # 5  zero padding bytes
    # print("ZeroPad: ",unpack("<I",payload[67:71])[0])
    # print("ZeroPad: ",payload[71])
    assert(unpack("<I", payload[67:71])[0] == 0)
    assert(payload[71] == 0)
    print("NoOrderIDEntries_blockLength: ", unpack("<H", payload[72:74])[0])
    # 5 zero padding bytes
    # print("ZeroPad: ",unpack("<I",payload[74:78])[0])
    # print("ZeroPad: ",payload[78])
    assert(unpack("<I", payload[74:78])[0] == 0)
    assert(payload[78] == 0)
    print("NoOrderIDEntries_numInGroup: ", payload[79])
    print("OrderID: ", unpack("<Q", payload[80:88])[0])
    print("MDOrderPriority: ", unpack("<Q", payload[88:96])[0])
    print("MDDisplayQty: ", unpack("<i", payload[96:100])[0])
    print("ReferenceID: ", payload[100])
    print("OrderUpdateAction: ", payload[101])
    # two byte zero pad
    assert(unpack("<H", payload[102:])[0] == 0)
    # print("ZeroPad: ",unpack("<H",payload[102:])[0])
    print("--------------End for a packet----------")
