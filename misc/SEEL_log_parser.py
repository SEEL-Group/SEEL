# Sample script on how to parse SEEL logs generated by "SEEL_gateway_node.ino"

# This script assumes NODEs are run uninterrupted. If NODES are unplugged, paused, and then re-plugged,
# the script will state that NODE missed BCASTS during the duration of the pause.Quick power cycles
# are okay.

# Tested with Python 3.5.2

# To run: python3 <path_to_this_file>/SEEL_log_parser.py <path_to_data_file>/<data_file>

# Expected input format (All <> is 1 byte):
# Broadcast time: "BT: <time>"
# Broadcast data: "BD: <INDEX BCAST 0> <INDEX BCAST 1> ....
# Node data: <INDEX DATA 0> <INDEX DATA 1> ....

import sys
import math
import matplotlib.pyplot as plt
import statistics

# Indexes for msgs
INDEX_HEADER = 0

INDEX_BT_TIME = 1

INDEX_BD_FIRST = 1
INDEX_BD_SYS_TIME_0 = 2
INDEX_BD_SYS_TIME_1 = 3
INDEX_BD_SYS_TIME_2 = 4
INDEX_BD_SYS_TIME_3 = 5
INDEX_BD_SNODE_AWAKE_TIME_0 = 6
INDEX_BD_SNODE_AWAKE_TIME_1 = 7
INDEX_BD_SNODE_AWAKE_TIME_2 = 8
INDEX_BD_SNODE_AWAKE_TIME_3 = 9
INDEX_BD_SNODE_SLEEP_TIME_0 = 10
INDEX_BD_SNODE_SLEEP_TIME_1 = 11
INDEX_BD_SNODE_SLEEP_TIME_2 = 12
INDEX_BD_SNODE_SLEEP_TIME_3 = 13
INDEX_BD_PATH_HC = 14
INDEX_BD_PATH_RSSI = 15
INDEX_BD_SNODE_JOIN_ID = 16 # Repeated
INDEX_BD_SNODE_JOIN_RESPONSE = 17 # Repeated

INDEX_DATA_ORIGINAL_ID = 0
INDEX_DATA_ASSIGNED_ID = 1
INDEX_DATA_PARENT_ID = 2
INDEX_DATA_PARENT_RSSI = 3
INDEX_DATA_WTB_0 = 4
INDEX_DATA_WTB_1 = 5
INDEX_DATA_WTB_2 = 6
INDEX_DATA_WTB_3 = 7
INDEX_DATA_PREV_TRANS = 8
INDEX_DATA_MISSED_BCASTS = 9
INDEX_DATA_QUEUE_SIZE = 10
INDEX_DATA_SEND_COUNT_0 = 11
INDEX_DATA_SEND_COUNT_1 = 12

# Misc Params
PARAM_COUNT_WRAP_SAFETY = 10 # Send count will not have wrapped within this many counts, keep it lower to account for node restarts too

class Bcast_Info:
    def __init__(self, sys_time, awk_time, slp_time):
        self.sys_time = sys_time
        self.awk_time = awk_time
        self.slp_time = slp_time

    def __str__(self):
        return "System time: " + str(self.sys_time) + " Awake time: " + str(self.awk_time) + " Sleep time: " + str(self.slp_time)

class Node_Info:
    def __init__(self, bcast_num, wtb, prev_trans, node_id, parent_id, parent_rssi, send_count, queue_size, missed_bcasts):
        self.bcast_num = bcast_num
        self.wtb = wtb
        self.prev_trans = prev_trans
        self.node_id = node_id
        self.parent_id = parent_id
        self.parent_rssi = parent_rssi
        self.send_count = send_count
        self.queue_size = queue_size
        self.missed_bcasts = missed_bcasts

    def __str__(self):
        return "Node ID: " + str(self.node_id) + "\tParent ID: " + str(self.parent_id) + "\tSend Count: " + \
            str(self.send_count) + "\tParent RSSI: " + str(self.parent_rssi) + "\tPrevious Transmissions: " + \
            str(self.prev_trans) + "\tWTB: " + str(self.wtb) + "\tQ Size: " + str(self.queue_size) + "\tMissed Bcasts: " + str(self.missed_bcasts)

def main():
    bcast_times = []
    bcast_info = []
    bcast_instances = {} # per node, since nodes may join at different times

    node_mapping = {}
    node_assignments = []
    node_info = []

    if len(sys.argv) <= 1:
        print("Unspecified data file")
        exit()

    df_name = sys.argv[1]

    df = open(df_name)
    df_read = df.readlines()
    df_length = len(df_read)

    current_line = 0
    while current_line < df_length:
        line = df_read[current_line].split()
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
                        print("join id: " + str(join_id) + " response: " + str(response) + " B. Join: " + str(len(bcast_times)))
                        node_mapping[response] = join_id
                        if node_assignments.count(join_id) == 0:
                            node_assignments.append(join_id)
                            node_info.append([])
                            # tracks earliest time "join_id" node couldve started sending data msgs
                            bcast_instances[join_id] = len(bcast_times) 
        else: # Node Data
            wtb = 0
            send_count = 0
            wtb += line[INDEX_DATA_WTB_0] << 24
            wtb += line[INDEX_DATA_WTB_1] << 16
            wtb += line[INDEX_DATA_WTB_2] << 8
            wtb += line[INDEX_DATA_WTB_3]
            send_count += line[INDEX_DATA_SEND_COUNT_0] << 8
            send_count += line[INDEX_DATA_SEND_COUNT_1]
            original_node_id = node_mapping[line[INDEX_DATA_ASSIGNED_ID]]
            node_info[node_assignments.index(original_node_id)].append(Node_Info(len(bcast_times), wtb, line[INDEX_DATA_PREV_TRANS], original_node_id, 
                line[INDEX_DATA_PARENT_ID], line[INDEX_DATA_PARENT_RSSI] - 256, send_count, line[INDEX_DATA_QUEUE_SIZE], line[INDEX_DATA_MISSED_BCASTS]))
        current_line += 1

    # Analysis
    total_bcasts = len(bcast_times)
    node_assignments.append(0) # For gateway
    node_mapping[0] = 0

    for node in node_info:
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
        first_wtb = True

        total_bcasts_for_node = total_bcasts - bcast_instances[node_id] + 1

        print("Node " + str(node_id))
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

                if msg.parent_rssi != -256: # Impossible value, used to flag RSSI unavailable
                    total_parent_rssi += msg.parent_rssi
                connections[node_assignments.index(node_mapping[msg.parent_id])] += 1
                connections_rssi[node_assignments.index(node_mapping[msg.parent_id])] += msg.parent_rssi

                queue_size_counter += msg.queue_size
                if msg.queue_size > max_queue_size:
                    max_queue_size = msg.queue_size

        print("\tJoined Network on Bcast: " + str(bcast_instances[node_id]))
        print("\tTotal Received Messages: " + str(node_msgs))
        print("\tDuplicate Messages: " + str(duplicate_msg))
        #print("\tDropped Packets: " + str(dropped_packets))
        print("\tDropped Packets: " + str(total_bcasts_for_node - (node_msgs - duplicate_msg)))
        print("\tConnection Percentage: " + str(connection_count / total_bcasts_for_node))
        print("\tReceived Percentage: " + str((node_msgs - duplicate_msg) / total_bcasts_for_node))
        print("\tMean WTB: " + str(statistics.mean(wtb)))
        print("\tMedian WTB: " + str(statistics.median(wtb)))
        print("\tStd Dev. WTB: " + str(statistics.stdev(wtb)))
        print("\tAvg Transmissions per received msg: " + str(total_transmissions / node_msgs))
        print("\tAvg Transmissions per cycle: " + str(total_transmissions / total_bcasts_for_node))
        print("\tAvg Data Queue Size: " + str(queue_size_counter / total_bcasts_for_node))
        print("\tMax Data Queue Size: " + str(max_queue_size))
        print("\tConnections: ")
        for i in range(len(node_assignments)):
            print("\t\t" + str(node_assignments[i]) + ":\t" + str(connections[i]))
            if connections[i] > 0:
                print("\t\t\tAvg RSSI: " + str(connections_rssi[i] / connections[i]))

if __name__ == "__main__":
    main()