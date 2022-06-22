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

PLOT_DISPLAY = True
PLOT_RSSI_ANALYSIS = True
PLOT_LOCS_WEIGHT_SCALAR = 1000 # Smaller for thicker lines

############################################################################
# Hardcode Section
USE_HARDCODED_NODE_JOINS = False
HARDCODED_NODE_JOINS = [
    # Format: [actual ID, assigned ID, cycle join]
]

HC_NJ_ACTUAL_ID_IDX = 0
HC_NJ_ASSIGNED_ID_IDX = 1
HC_NJ_CYCLE_JOIN_IDX = 2

USE_HARDCODED_NODE_LOCS = False
HARDCODED_NODE_LOCS = {
    # Format: actual ID: (loc_x, loc_y)
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

# INDEX_BT 0 used for the text "BT:"
INDEX_BT_TIME = 1

# INDEX_BD 0 used for the text "BD:"
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
data_info = []

############################################################################

class Bcast_Info:
    def __init__(self, bcast_num, bcast_inst, sys_time, awk_time, slp_time):
        self.bcast_num = bcast_num
        self.bcast_inst = bcast_inst
        self.sys_time = sys_time
        self.awk_time = awk_time
        self.slp_time = slp_time

    def __str__(self):
        return "Bcast num: " + str(self.bcast_num) + " Inst: " + str(self.bcast_inst) + " System time: " + str(self.sys_time) + " Awake time: " + str(self.awk_time) + " Sleep time: " + str(self.slp_time)

class Data_Info:
    def __init__(self, bcast_num, bcast_inst, wtb, prev_trans, node_id, parent_id, parent_rssi, send_count, queue_size, missed_bcasts, crc_fails,       assert_fails):
        self.bcast_num = bcast_num
        self.bcast_inst = bcast_inst
        self.wtb = wtb
        self.prev_trans = prev_trans
        self.node_id = node_id
        self.parent_id = parent_id
        self.parent_rssi = parent_rssi
        self.send_count = send_count
        self.queue_size = queue_size
        self.missed_bcasts = missed_bcasts
        self.crc_fails = crc_fails
        self.assert_fails = assert_fails

    def __str__(self):
        return "Bcast num: " + str(self.bcast_num) + "\tBcast inst: " + str(self.bcast_inst) + "\tNode ID: " + str(self.node_id) + \
        "\tParent ID: " + str(self.parent_id) + "\tSend Count: " + str(self.send_count) + "\tParent RSSI: " + str(self.parent_rssi) + \
        "\tPrevious Transmissions: " + str(self.prev_trans) + "\tWTB: " + str(self.wtb) + "\tQ Size: " + str(self.queue_size) + \
        "\tMissed Bcasts: " + str(self.missed_bcasts) + "\tCRC Fails: " + str(self.crc_fails) + "\tAssert Fails: " + str(self.assert_fails)

def node_entry(actual_id, assigned_id, bcast_join):
    print("join id: " + str(actual_id) + "\tresponse: " + str(assigned_id) + "\tB. Join: " + str(bcast_join))
    if assigned_id in node_mapping:
        print("WARNING: Assigned ID " + str(assigned_id) + " is assigned to multiple SNODEs")
    node_mapping[assigned_id] = actual_id
    if node_assignments.count(actual_id) == 0:
        node_assignments.append(actual_id)
        data_info.append([])
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
    bcast_instance = -1
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
            if line[INDEX_BD_FIRST] > 0:
                bcast_instance += 1
            bcast_info.append(Bcast_Info(line[INDEX_BD_BCAST_COUNT], bcast_instance, sys_time, awk_time, slp_time))

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
                data_info[node_assignments.index(original_node_id)].append(Data_Info(line[INDEX_DATA_BCAST_COUNT], bcast_instance, wtb, line[INDEX_DATA_PREV_TRANS], original_node_id, line[INDEX_DATA_PARENT_ID], line[INDEX_DATA_PARENT_RSSI] - 256, send_count, line[INDEX_DATA_QUEUE_SIZE], line[INDEX_DATA_MISSED_BCASTS], line[INDEX_DATA_CRC_FAILS], line[INDEX_DATA_ASSERT_FAIL]))
        current_line += 1

    # Analysis vars
    total_bcasts = len(bcast_times)
    node_assignments.append(0) # For gateway
    node_mapping[0] = 0
    
    # Plotting vars
    if PLOT_DISPLAY:
        if USE_HARDCODED_NODE_LOCS:
            G = nx.DiGraph()
        if PLOT_RSSI_ANALYSIS:
            analysis_rssi = []
            analysis_transmissions = []
            
        plot_gnode_bcast_nums = []
        plot_gnode_bcast_insts = []
        for b in bcast_info:
            plot_gnode_bcast_nums.append(b.bcast_num)
            plot_gnode_bcast_insts.append(b.bcast_inst)

    # GNODE 
    print("Total Bcasts: " + str(total_bcasts))

    # SNODE
    for i in range(len(data_info)):
        node_data_msgs = data_info[i]
        print("********************************************************")
        print("Node " + str(node_assignments[i]))
        if not node_data_msgs: # Empty
            print("No messages received")
            continue
        duplicate_msg_tracker = {}
        wtb = []
        node_id = node_data_msgs[0].node_id
        num_node_msgs = len(node_data_msgs)
        total_transmissions = 0
        total_parent_rssi = 0
        total_rssi_counts = 0
        send_count_tracker = 0
        dropped_packets = 0
        dropped_packet_tracker = []
        duplicate_msg = 0
        connection_count = 0 # How many unique (bcast) data msgs we received
        connection_count_max = 0 # Max number of unique bcasts based on highest bcast count per bcast instance
        connection_count_overflow = 0 # Handles 256 overflow in a bcast instance
        connection_count_last_overflow = 0 # Enforces overflow rates so single late msg cannot cause another overflow
        connection_inst_set = set() # Tracks current bcast number per bcast instance, make set to remove dups
        connection_inst_max = 0 # Tracks current max bcast number per bcast instance
        connections = [0] * len(node_assignments)
        connections_rssi = [0] * len(node_assignments)
        queue_size_counter = 0
        max_queue_size = 0
        total_crc_fails = 0
        first_wtb = True
        prev_bcast_num = -1
        prev_bcast_inst = -1
        total_bcasts_for_node = total_bcasts - bcast_instances[node_id] + 1

        # Plotting
        plot_snode_bcast_nums = []
        plot_snode_bcast_insts = []
        plot_snode_bcast_used = []

        if PLOT_RSSI_ANALYSIS:
            analysis_reset = True # Resets on first time or missed bcasts
            analysis_prev_data = [0, 0, 0]

        for msg in node_data_msgs:   
            dup = True
        
            # Figure out how many messages we received from the SNODE versus how many we possibly could have received
            if msg.bcast_inst != prev_bcast_inst:
                prev_bcast_inst = msg.bcast_inst
                print("Received messages in bcast instance: " + str(len(connection_inst_set)) + "/" + str(connection_inst_max), str(0 if connection_inst_max == 0 else len(connection_inst_set) / connection_inst_max))        
                connection_count += len(connection_inst_set)
                connection_count_max += connection_inst_max
                connection_count_overflow = 0
                connection_count_last_overflow = 0
                connection_inst_set = set()
                connection_inst_max = 0
                prev_bcast_num = -1
                
               
            if (len(connection_inst_set) > PARAM_COUNT_WRAP_SAFETY or msg.bcast_num < (PARAM_COUNT_WRAP_SAFETY if len(connection_inst_set) == 0 else max(            connection_inst_set)+ PARAM_COUNT_WRAP_SAFETY)): # Not from previous bcast inst
                if msg.bcast_num < prev_bcast_num and prev_bcast_num > 192 and msg.bcast_num < 64 and len(connection_inst_set) > (connection_count_last_overflow + PARAM_COUNT_WRAP_SAFETY): # Assume bcast num overflowed, values used are 3/4 of 256 and 1/4 of 256
                    #print("Debug: overflow")
                    connection_count_overflow += 256
                    connection_count_last_overflow = len(connection_inst_set)
                
                overflow_comp_bcast_num = msg.bcast_num + connection_count_overflow
                # Message from non-overflow case came late
                if msg.bcast_num > (255 - PARAM_COUNT_WRAP_SAFETY) and (len(connection_inst_set) - connection_count_last_overflow) < PARAM_COUNT_WRAP_SAFETY:
                    overflow_comp_bcast_num -= 256;
                
                if not overflow_comp_bcast_num in connection_inst_set: # Dup check
                    #print("DEBUG: add " + str(overflow_comp_bcast_num))
                    connection_inst_set.add(overflow_comp_bcast_num)
                    if overflow_comp_bcast_num > connection_inst_max:
                        connection_inst_max = overflow_comp_bcast_num
                    prev_bcast_num = msg.bcast_num
                    plot_snode_bcast_nums.append(msg.bcast_num)
                    plot_snode_bcast_insts.append(msg.bcast_inst)
                    plot_snode_bcast_used.append(False)
                    dup = False
                    print(str(msg)) 
                #else:
                    #print("Debug: dup")
            #else:
                #print(str(msg)) 
                #print("Debug: ignore")

            '''
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
            '''

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
            else: # if dup
                duplicate_msg += 1
        
        print("Received messages in bcast instance: " + str(len(connection_inst_set)) + "/" + str(connection_inst_max), str(0 if connection_inst_max == 0 else len( \
            connection_inst_set) / connection_inst_max))
        connection_count += len(connection_inst_set)
        connection_count_max += connection_inst_max
        
        # Print Analysis
        print("\tJoined Network on Bcast: " + str(bcast_instances[node_id]))
        print("\tTotal Received Messages: " + str(num_node_msgs))
        print("\tDuplicate Messages: " + str(duplicate_msg))
        print("\tDropped Packets: " + str(total_bcasts_for_node - (num_node_msgs - duplicate_msg)))
        print("\tReceived (Total) Percentage: " + str(connection_count / total_bcasts_for_node)) # Total number of GNODE Bcasts received
        print("\tReceived (Possible) Percentage: " + str(0 if connection_count_max == 0 else connection_count / connection_count_max)) # Total given times connected (until disconnect or death). This metric is a better representation of PDR
        if len(wtb) > 0:
            print("\tMean WTB: " + str(statistics.mean(wtb)))
            print("\tMedian WTB: " + str(statistics.median(wtb)))
            print("\tStd Dev. WTB: " + str(statistics.stdev(wtb)))
        print("\tAvg Transmissions per received msg: " + str(total_transmissions / num_node_msgs))
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
                    G.add_edge(node_id, node_assignments[j], weight=connections[j]/PLOT_LOCS_WEIGHT_SCALAR) #total_connections)
        print(flush=True)
        
        # Node specific plots
        if PLOT_DISPLAY:
        
            """ Bcasts Received Start """
            
            plot_snode_bcast_nums_padded = []
            """
            if len(plot_snode_bcast_nums) > 0:
                # Since SNODE may have missed GNODE bcasts, fill SNODE array with same value (graph shows horizontal line) if missed
                n_g_count = 0
                for n_s in plot_snode_bcast_nums:
                    print("Debug: looking for " + str(n_s))
                    n_g = plot_gnode_bcast_nums[n_g_count]
                    print("\tDebug: looking at " + str(n_g))
                    while n_s != n_g:
                        if len(plot_snode_bcast_nums_padded) > 0:
                            plot_snode_bcast_nums_padded.append(plot_snode_bcast_nums_padded[-1]) # Append last value
                            print("\tDebug: padding " + str(plot_snode_bcast_nums_padded[-1]))
                        else:
                            plot_snode_bcast_nums_padded.append(0)
                            print("\tDebug: padding " + str(0))
                        n_g_count += 1
                        if n_g_count >= len(plot_gnode_bcast_nums): # Out of GNODE nums
                            break
                        n_g = plot_gnode_bcast_nums[n_g_count]
                        print("\tDebug: looking at " + str(n_g))
                    if n_g_count < len(plot_snode_bcast_nums): # Out of GNODE nums
                        plot_snode_bcast_nums_padded.append(n_g)
                        n_g_count += 1
                    else:
                        break
            """
            if len(plot_snode_bcast_nums) > 0:
                # Since SNODE may have missed GNODE bcasts, fill SNODE array with same value (graph shows horizontal line) if missed
                plot_snode_bcast_nums_padded = []
                n_s_count = 0
                for n_g_idx in range(len(plot_gnode_bcast_nums)): # Start tracking when SNODE joined the network
                    n_g = plot_gnode_bcast_nums[n_g_idx]
                    n_g_inst = plot_gnode_bcast_insts[n_g_idx]
                    #print("Debug: looking for " + str(n_g))
                    window_min = max(n_s_count - PARAM_COUNT_WRAP_SAFETY, 0)
                    window_max = min(n_s_count + PARAM_COUNT_WRAP_SAFETY, len(plot_snode_bcast_nums) - 1)
                    #print("Debug: Window min idx: " + str(window_min) + " Current idx: " + str(min(n_s_count, window_max)) + " Window max idx: " + str(window_max))
                    #print("Debug: Window min: " + str(plot_snode_bcast_nums[window_min]) + " Current: " + str(plot_snode_bcast_nums[min(n_s_count, window_max)]) + " Window max: " + str(plot_snode_bcast_nums[window_max]))
                    found = -1
                    count_inc = window_min - n_s_count
                    for n_s_idx in range(window_min, window_max):
                        n_s = plot_snode_bcast_nums[n_s_idx]
                        n_s_inst = plot_snode_bcast_insts[n_s_idx]
                        #print("\tDebug: looking at " + str(n_s))
                        count_inc += 1
                        if n_s == n_g and n_s_inst == n_g_inst and not plot_snode_bcast_used[n_s_idx]:
                            found = n_s
                            plot_snode_bcast_used[n_s_idx] = True
                            break
                    if found >= 0:
                        plot_snode_bcast_nums_padded.append(n_s)
                        n_s_count += count_inc
                    else:
                        if len(plot_snode_bcast_nums_padded) > 0:
                            plot_snode_bcast_nums_padded.append(0) # Append last value
                            #print("\tDebug: padding " + str(plot_snode_bcast_nums_padded[-1]))
                        else:
                            plot_snode_bcast_nums_padded.append(0)
                            #print("\tDebug: padding " + str(0))
            """
            if len(plot_snode_bcast_nums) > 0:
                # Since SNODE may have missed GNODE bcasts, fill SNODE array with same value (graph shows horizontal line) if missed
                for n_s_idx in range(len(plot_snode_bcast_nums)):
                    n_s = plot_snode_bcast_nums[n_s_idx]
                    print("Debug: looking for " + str(n_s))
                    window_min = max(n_s_idx - PARAM_COUNT_WRAP_SAFETY, 0)
                    window_max = min(n_s_idx + PARAM_COUNT_WRAP_SAFETY, len(plot_snode_bcast_nums) - 1)
                    print("Debug: Window min idx: " + str(window_min) + " Current idx: " + str(min(n_s_idx, window_max)) + " Window max idx: " + str(window_max))
                    print("Debug: Window min: " + str(plot_snode_bcast_nums[window_min]) + " Current: " + str(plot_snode_bcast_nums[min(n_s_idx, window_max)]) + " Window max: " + str(plot_snode_bcast_nums[window_max]))
                    found = False
                    for n_s_local in plot_snode_bcast_nums[window_min:window_max]:
                        print("Debug: looking at " + str(n_s_local))
                        if n_s_local == n_s:
                            found = True
                            break;
                    if found:
                        plot_snode_bcast_nums_padded.append(n_s)
                    else: # Append last value or 0 if no values yet
                        if len(plot_snode_bcast_nums_padded) > 0:
                            plot_snode_bcast_nums_padded.append(plot_snode_bcast_nums_padded[-1]) # Append last value
                            print("\tDebug: padding " + str(plot_snode_bcast_nums_padded[-1]))
                        else:
                            plot_snode_bcast_nums_padded.append(0)
                            print("\tDebug: padding " + str(0))
            """
                            
            figure, axis = plt.subplots(2, sharex=True)
            # GNODE
            axis[0].plot(plot_gnode_bcast_nums)
            axis[0].set_title("GNODE")
            # SNODE
            axis[1].plot(plot_snode_bcast_nums_padded)
            axis[1].set_title("SNODE " + str(node_id))
            plt.show()
            """ Bcasts Received End """

    if PLOT_DISPLAY:
        # Plot network with parent-child connections
        if USE_HARDCODED_NODE_LOCS:

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