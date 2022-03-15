# Sample script on how to parse SEEL logs generated by "SEEL_gateway_node.ino"

# This script assumes NODEs are run uninterrupted. If NODES are unplugged, paused, and then re-plugged,
# the script will state that NODE missed BCASTS during the duration of the pause.Quick power cycles
# are okay. 
# If node join entries are not captured in the logs (logging created on an established network), node IDs can be added in the "Hardcoded Section"

# Tested with Python 3.5.2

# To run: python3 <path_to_this_file>/SEEL_log_parser.py <path_to_data_file>/<data_file>

# Expected input format (All <> is 1 byte):
# Broadcast time: "BT: <time>"
# Broadcast data: "BD: <INDEX BCAST 0> <INDEX BCAST 1> ....
# Node data: <INDEX DATA 0> <INDEX DATA 1> ....

# Make sure the "Indexes Section" in this script matches that in "SEEL_Defines.h" and "SEEL_sensor_node.ino"

import sys
import math
import matplotlib.pyplot as plt
import networkx as nx
import statistics

############################################################################
# Parameters

PLOT_RSSI_ANALYSIS = True;

############################################################################
# Hardcode Section
USE_HARDCODED_NODE_JOINS = False;
HARDCODED_NODE_JOINS = [
    # Format: [actual ID, assigned ID, cycle join]
    [6, 6, 0],
    [7, 7, 0],
    [8, 8, 0],
    [10, 123, 0],
    [11, 127, 0],
    [12, 126, 0],
    [13, 121, 0],
    [14, 14, 0],
    [15, 124, 0],
    [16, 16, 0],
    [17, 17, 0],
    [18, 18, 0],
    [19, 19, 0],
    [20, 20, 0]
]
HC_NJ_ACTUAL_ID_IDX = 0
HC_NJ_ASSIGNED_ID_IDX = 1
HC_NJ_CYCLE_JOIN_IDX = 2

USE_HARDCODED_NODE_LOCS = False;
HARDCODED_NODE_LOCS = {
    # Format: actual ID: (loc_x, loc_y)
    0: (0, 0),
    6: (-0.0225715, 0.0015167),
    7: (0.0023394, -0.0056706),
    8: (-0.0070375, -0.0082093),
    10: (0.0189258, -0.0001402),
    11: (-0.0069662, -0.000512),
    12: (0.0096659, 0.0001555),
    13: (-0.0129977, -0.0084158),
    14: (0.0342914, 0.0005615),
    15: (-0.0005662, -0.0033766),
    16: (0.0169483, 0.000237),
    17: (-0.0057943, -0.0170177),
    18: (-0.0173758, -0.0086746),
    19: (0.0258428, -0.0004104),
    20: (0.0034875, 0.0006273),
}

NETWORK_DRAW_OPTIONS = {
    "node_font_size": 10,
    "node_size": 250,
    "node_color": "white",
    "node_edge_color": "black",
    "node_width": 1,
    "edge_width": 1,
}

############################################################################
# Indexes Section

INDEX_HEADER = 0

INDEX_BT_TIME = 1

INDEX_BD_FIRST = 1
INDEX_BD_BCAST_COUNT = 2
INDEX_BD_SYS_TIME_0 = 3
INDEX_BD_SYS_TIME_1 = 4
INDEX_BD_SYS_TIME_2 = 5
INDEX_BD_SYS_TIME_3 = 6
INDEX_BD_SNODE_AWAKE_TIME_0 = 7
INDEX_BD_SNODE_AWAKE_TIME_1 = 8
INDEX_BD_SNODE_AWAKE_TIME_2 = 9
INDEX_BD_SNODE_AWAKE_TIME_3 = 10
INDEX_BD_SNODE_SLEEP_TIME_0 = 11
INDEX_BD_SNODE_SLEEP_TIME_1 = 12
INDEX_BD_SNODE_SLEEP_TIME_2 = 13
INDEX_BD_SNODE_SLEEP_TIME_3 = 14
INDEX_BD_PATH_HC = 15
INDEX_BD_PATH_RSSI = 16
INDEX_BD_SNODE_JOIN_ID = 17 # Repeated
INDEX_BD_SNODE_JOIN_RESPONSE = 18 # Repeated

INDEX_DATA_ORIGINAL_ID = 0
INDEX_DATA_ASSIGNED_ID = 1
INDEX_DATA_PARENT_ID = 2
INDEX_DATA_PARENT_RSSI = 3
INDEX_DATA_BCAST_COUNT = 4
INDEX_DATA_WTB_0 = 5
INDEX_DATA_WTB_1 = 6
INDEX_DATA_WTB_2 = 7
INDEX_DATA_WTB_3 = 8
INDEX_DATA_SEND_COUNT_0 = 9
INDEX_DATA_SEND_COUNT_1 = 10
INDEX_DATA_PREV_TRANS = 11
INDEX_DATA_MISSED_BCASTS = 12
INDEX_DATA_QUEUE_SIZE = 13
INDEX_DATA_CRC_FAILS = 14
INDEX_DATA_ASSERT_FAIL = 15

############################################################################
# Misc Params
PARAM_COUNT_WRAP_SAFETY = 10 # Send count will not have wrapped within this many counts, keep it lower to account for node restarts too

############################################################################
# Global Variables

bcast_times = []
bcast_info = []
bcast_instances = {} # per node, since nodes may join at different times

node_mapping = {}
node_assignments = []
node_info = []

############################################################################

class Bcast_Info:
    def __init__(self, sys_time, awk_time, slp_time):
        self.sys_time = sys_time
        self.awk_time = awk_time
        self.slp_time = slp_time

    def __str__(self):
        return "System time: " + str(self.sys_time) + " Awake time: " + str(self.awk_time) + " Sleep time: " + str(self.slp_time)

class Node_Info:
    def __init__(self, bcast_num, wtb, prev_trans, node_id, parent_id, parent_rssi, send_count, queue_size, missed_bcasts, crc_fails):
        self.bcast_num = bcast_num
        self.wtb = wtb
        self.prev_trans = prev_trans
        self.node_id = node_id
        self.parent_id = parent_id
        self.parent_rssi = parent_rssi
        self.send_count = send_count
        self.queue_size = queue_size
        self.missed_bcasts = missed_bcasts
        self.crc_fails = crc_fails

    def __str__(self):
        return "Node ID: " + str(self.node_id) + "\tParent ID: " + str(self.parent_id) + "\tSend Count: " + \
            str(self.send_count) + "\tParent RSSI: " + str(self.parent_rssi) + "\tPrevious Transmissions: " + \
            str(self.prev_trans) + "\tWTB: " + str(self.wtb) + "\tQ Size: " + str(self.queue_size) + "\tMissed Bcasts: " + \
            str(self.missed_bcasts) + "\tCRC Fails: " + str(self.crc_fails)

def node_entry(actual_id, assigned_id, bcast_join):
    print("join id: " + str(actual_id) + "\tresponse: " + str(assigned_id) + "\tB. Join: " + str(bcast_join))
    if assigned_id in node_mapping:
        print("WARNING: Assigned ID " + str(assigned_id) + " is assigned to multiple SNODEs")
    node_mapping[assigned_id] = actual_id
    if node_assignments.count(actual_id) == 0:
        node_assignments.append(actual_id)
        node_info.append([])
        bcast_instances[actual_id] = bcast_join

def main():
    if len(sys.argv) <= 1:
        print("Unspecified data file")
        exit()

    df_name = sys.argv[1]

    df = open(df_name)
    df_read = df.readlines()
    df_length = len(df_read)

    if USE_HARDCODED_NODE_JOINS:
        print("Using HARDCODED Node Joins")
        for i in HARDCODED_NODE_JOINS:
            node_entry(i[HC_NJ_ACTUAL_ID_IDX], i[HC_NJ_ASSIGNED_ID_IDX], i[HC_NJ_CYCLE_JOIN_IDX]);

    if USE_HARDCODED_NODE_LOCS:
        print("Using HARDCODED Node Locs")
        # Uncomment and use for location normalization
        #for i in HARDCODED_NODE_LOCS:
            #print("[" + str(i) + ", " + str(round(HARDCODED_NODE_LOCS[i][0] - <NORM_X>, 7)) + ", " + str(round(HARDCODED_NODE_LOCS[i][1] - <NORM_Y>, 7)) + "]")

    # Parse Logs
    current_line = 0
    while current_line < df_length:
        line = df_read[current_line].split()
        if len(line) == 0:
            current_line += 1
            continue
        line[1:len(line)] = list(map(int, line[1:len(line)]))

        if line[INDEX_HEADER] == "BT:": # Bcast time
            bcast_times.append(line[INDEX_BT_TIME])
        elif line[INDEX_HEADER] == "BD:": # Bcast data
            sys_time = 0
            awk_time = 0
            slp_time = 0
            sys_time += line[INDEX_BD_SYS_TIME_0] << 24
            sys_time += line[INDEX_BD_SYS_TIME_1] << 16
            sys_time += line[INDEX_BD_SYS_TIME_2] << 8
            sys_time += line[INDEX_BD_SYS_TIME_3]
            awk_time += line[INDEX_BD_SNODE_AWAKE_TIME_0] << 24
            awk_time += line[INDEX_BD_SNODE_AWAKE_TIME_1] << 16
            awk_time += line[INDEX_BD_SNODE_AWAKE_TIME_2] << 8
            awk_time += line[INDEX_BD_SNODE_AWAKE_TIME_3]
            slp_time += line[INDEX_BD_SNODE_SLEEP_TIME_0] << 24
            slp_time += line[INDEX_BD_SNODE_SLEEP_TIME_1] << 16
            slp_time += line[INDEX_BD_SNODE_SLEEP_TIME_2] << 8
            slp_time += line[INDEX_BD_SNODE_SLEEP_TIME_3]
            bcast_info.append(Bcast_Info(sys_time, awk_time, slp_time))

            for i in range(math.floor((len(line) - INDEX_BD_SNODE_JOIN_ID) / 2)):
                repeat_index = i * 2
                join_id = line[INDEX_BD_SNODE_JOIN_ID + repeat_index]
                if join_id != 0:
                    response = line[INDEX_BD_SNODE_JOIN_RESPONSE + repeat_index]
                    if response != 0: # Reponse of 0 means error
                        node_entry(join_id, response, len(bcast_times))
        else: # Node Data
            wtb = 0
            send_count = 0
            wtb += line[INDEX_DATA_WTB_0] << 24
            wtb += line[INDEX_DATA_WTB_1] << 16
            wtb += line[INDEX_DATA_WTB_2] << 8
            wtb += line[INDEX_DATA_WTB_3]
            send_count += line[INDEX_DATA_SEND_COUNT_0] << 8
            send_count += line[INDEX_DATA_SEND_COUNT_1]
            if line[INDEX_DATA_ASSIGNED_ID] in node_mapping:
                original_node_id = node_mapping[line[INDEX_DATA_ASSIGNED_ID]]
                node_info[node_assignments.index(original_node_id)].append(Node_Info(len(bcast_times), wtb, line[INDEX_DATA_PREV_TRANS], original_node_id, 
                    line[INDEX_DATA_PARENT_ID], line[INDEX_DATA_PARENT_RSSI] - 256, send_count, line[INDEX_DATA_QUEUE_SIZE], line[INDEX_DATA_MISSED_BCASTS], line[INDEX_DATA_CRC_FAILS]))
        current_line += 1

    # Analysis
    total_bcasts = len(bcast_times)
    node_assignments.append(0) # For gateway
    node_mapping[0] = 0
    if USE_HARDCODED_NODE_LOCS:
        G = nx.DiGraph()
    if PLOT_RSSI_ANALYSIS:
        analysis_rssi = []
        analysis_transmissions = []

    print("Total Bcasts: " + str(total_bcasts))

    for i in range(len(node_info)):
        node = node_info[i]
        print("********************************************************")
        print("Node " + str(node_assignments[i]))
        if not node: # Empty
            print("No messages received")
            continue
        duplicate_msg_tracker = {}
        wtb = []
        node_id = node[0].node_id
        node_msgs = len(node)
        total_transmissions = 0
        total_parent_rssi = 0
        total_rssi_counts = 0
        send_count_tracker = 0
        dropped_packets = 0
        dropped_packet_tracker = []
        duplicate_msg = 0
        connection_count = 0
        prev_bcast_num = -1
        connections = [0] * len(node_assignments)
        connections_rssi = [0] * len(node_assignments)
        queue_size_counter = 0
        max_queue_size = 0
        total_crc_fails = 0
        first_wtb = True
        total_bcasts_for_node = total_bcasts - bcast_instances[node_id] + 1

        if PLOT_RSSI_ANALYSIS:
            analysis_reset = True # Resets on first time or missed bcasts
            analysis_prev_data = [0, 0, 0]

        for msg in node:
            if msg.bcast_num != prev_bcast_num:
                connection_count += 1
                prev_bcast_num = msg.bcast_num

            print("Bcast num: " + str(msg.bcast_num) + "\t" + str(msg))

            dup = False
            # Nodes may send duplicate messages, filter those out so their stats are not double-counted
            if msg.send_count in duplicate_msg_tracker:
                if msg.bcast_num - duplicate_msg_tracker[msg.send_count] < PARAM_COUNT_WRAP_SAFETY:
                    # likely a duplicate message
                    duplicate_msg += 1
                    dup = True
                duplicate_msg_tracker[msg.send_count] = msg.bcast_num
            else:
                # not a duplicate message
                duplicate_msg_tracker[msg.send_count] = msg.bcast_num

            if not dup:
                # remove previously missed packet if the packet came in late
                if dropped_packet_tracker.count(msg.send_count) > 0:
                    dropped_packet_tracker.remove(msg.send_count)
                    dropped_packets -= 1

                for i in range(send_count_tracker + 1, msg.send_count):
                    dropped_packets += 1
                    if dropped_packet_tracker.count(i) == 0:
                        dropped_packet_tracker.append(i)
                send_count_tracker = msg.send_count

                # Ignore first WTB during analysis since not system sync'd yet
                if not first_wtb:
                    wtb.append(msg.wtb)
                else:
                    first_wtb = False

                # Not all sent msgs are received, "prev_trans" indicates how many total transmissions 
                # were done by the node in the previous cycle. Duplicates within a cycle can exist

                total_transmissions += msg.prev_trans
                total_crc_fails += msg.crc_fails

                if msg.parent_rssi != -256: # Impossible value, used to flag RSSI unavailable
                    total_parent_rssi += msg.parent_rssi
                    
                if msg.parent_id in node_mapping:
                    connections[node_assignments.index(node_mapping[msg.parent_id])] += 1
                    connections_rssi[node_assignments.index(node_mapping[msg.parent_id])] += msg.parent_rssi

                queue_size_counter += msg.queue_size
                if msg.queue_size > max_queue_size:
                    max_queue_size = msg.queue_size

                if PLOT_RSSI_ANALYSIS:

                    # Compare against queue size 0 because otherwise we don't know how many msgs we got this cycle
                    if analysis_reset or msg.prev_trans == 0 or msg.queue_size != 0 or msg.parent_id != 0 or msg.bcast_num != (analysis_prev_data[0] + 1):
                        analysis_prev_data = [msg.bcast_num, msg.parent_rssi, msg.queue_size]
                        analysis_reset = False
                    else:
                        tpm = msg.prev_trans / (analysis_prev_data[2] - msg.queue_size + 1); # Average transmissions per msg

                        analysis_rssi.append(analysis_prev_data[1]);
                        analysis_transmissions.append(tpm);

                        analysis_prev_data = [msg.bcast_num, msg.parent_rssi, msg.queue_size]
                        analysis_reset = False

        # Print Analysis
        print("\tJoined Network on Bcast: " + str(bcast_instances[node_id]))
        print("\tTotal Received Messages: " + str(node_msgs))
        print("\tDuplicate Messages: " + str(duplicate_msg))
        #print("\tDropped Packets: " + str(dropped_packets))
        print("\tDropped Packets: " + str(total_bcasts_for_node - (node_msgs - duplicate_msg)))
        print("\tConnection Percentage: " + str(connection_count / total_bcasts_for_node))
        print("\tReceived Percentage: " + str((node_msgs - duplicate_msg) / total_bcasts_for_node))
        if len(wtb) > 0:
            print("\tMean WTB: " + str(statistics.mean(wtb)))
            print("\tMedian WTB: " + str(statistics.median(wtb)))
            print("\tStd Dev. WTB: " + str(statistics.stdev(wtb)))
        print("\tAvg Transmissions per received msg: " + str(total_transmissions / node_msgs))
        print("\tAvg Transmissions per cycle: " + str(total_transmissions / total_bcasts_for_node))
        print("\tAvg Data Queue Size: " + str(queue_size_counter / total_bcasts_for_node))
        print("\tMax Data Queue Size: " + str(max_queue_size))
        print("\tAvg CRC fails per cycle: " + str(total_crc_fails / total_bcasts_for_node))
        print("\tConnections: ")
        total_connections = sum(connections)
        for j in range(len(node_assignments)):
            print("\t\t" + str(node_assignments[j]) + ":\t" + str(connections[j]))
            if connections[j] > 0:
                print("\t\t\tAvg RSSI: " + str(connections_rssi[j] / connections[j]))
                if USE_HARDCODED_NODE_LOCS:
                    G.add_edge(node_id, node_assignments[j], weight=connections[j]/total_connections)

    if USE_HARDCODED_NODE_LOCS:
        # Plot network with parent-child connections

        # nodes
        locs_flipped = {node: (y, x) for (node, (x,y)) in HARDCODED_NODE_LOCS.items()}
        nx.draw_networkx_nodes(G, locs_flipped, node_size=NETWORK_DRAW_OPTIONS["node_size"], \
            node_color=NETWORK_DRAW_OPTIONS["node_color"], edgecolors=NETWORK_DRAW_OPTIONS["node_edge_color"],
            linewidths=NETWORK_DRAW_OPTIONS["node_width"])

        # edges
        weighted_edges = [G[u][v]['weight'] for u,v in G.edges()]
        nx.draw_networkx_edges(G, locs_flipped, width=weighted_edges, connectionstyle="angle3")

        # labels
        nx.draw_networkx_labels(G, locs_flipped, font_size=NETWORK_DRAW_OPTIONS["node_font_size"])

        ax = plt.gca()
        plt.axis("off")
        plt.tight_layout()
        plt.show()

    if PLOT_RSSI_ANALYSIS:
        print("RSSI Analysis # data points: " + str(len(analysis_rssi)))
        plt.scatter(analysis_rssi, analysis_transmissions, alpha=0.1);
        plt.show()

if __name__ == "__main__":
    main()