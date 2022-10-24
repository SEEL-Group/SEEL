# Script to simulate collected SEEL data on different topologies

# Example usage in SEEL_log_parser.py

# Tested with Python 3.6.0

import statistics

# Parameters
PRINT_INCOMING_DATA = False
PRINT_ALL_TOPOLOGIES = False
PRINT_ALL_REPLAY_CYCLES = False

PARENT_USE_PATH_RSSI = True # Otherwise use immediate RSSI

class Sim_Node_Bcast_Msg:
    def __init__(self, id, rssi, unconsidered_bcast):
        self.id = id
        self.rssi = rssi
        self.unconsidered_bcast = unconsidered_bcast
        
    def __repr__(self):
        return "Id: " + str(self.id) + "\tRSSI: " + str(self.rssi) + "\tUB: " + str(self.unconsidered_bcast)
        
class Sim_Node_Other_Msg:
    def __init__(self, id, rssi, count, is_child):
        self.id = id
        self.rssi = rssi
        self.count = count
        self.is_child = is_child
        
    def __repr__(self):
        return "Id: " + str(self.id) + "\tRSSI: " + str(self.rssi) + "\tCount: " + str(self.count) + "\tChild: " + str(self.is_child)
        
class Sim_Node:
    def __init__(self, node_id, parent_id, parent_rssi, received_bcast_msg):
        self.node_id = node_id
        self.parent_id = parent_id
        self.parent_rssi = parent_rssi
        self.failed_data_trans = -1
        self.total_data_trans = -1
        self.total_any_trans = -1
        self.received_bcast_msg = {}
        self.received_other_msg = {}
        for msg in received_bcast_msg:
            self.received_bcast_msg[msg.id] = msg
    
    def set_prev_data(self, failed_data_trans, total_data_trans, total_any_trans, received_other_msg):
        self.failed_data_trans = failed_data_trans
        self.total_data_trans = total_data_trans
        self.total_any_trans = total_any_trans
        for msg in received_other_msg:
            self.received_other_msg[msg.id] = msg
                
    def print_node(self):
        print("\tParent: " + str(self.parent_id) + \
            "\tParent RSSI: " + str(self.parent_rssi) + \
            "\tFailed D. Trans: " + str(self.failed_data_trans) + \
            "\tTotal D. Trans: " + str(self.total_data_trans) + \
            "\tTotal A. Trans: " + str(self.total_any_trans))
        print("\tReceived Broadcast messages")
        for b_msg in self.received_bcast_msg.values():
            print("\t\t" + str(b_msg))
        print("\tReceived Non-Broadcast messages")
        if len(self.received_other_msg) > 0:
            for nb_msg in self.received_other_msg.values():
                print("\t\t" + str(nb_msg))
        else:
            print("\t\tNone")
    
class Sim_Cycle:
    def __init__(self):
        self.nodes = {}
        self.gnode_any_trans = -1
        
    def add_node(self, node_id, sim_node):
        self.nodes[node_id] = sim_node
        
    def set_gnode_trans(self, any_trans):
        self.gnode_any_trans = any_trans
        
class Sim_Data:        
    def __init__(self):        
        self.cycles = {}
        
    def add_cycle(self, cycle_num, sim_cycle):
        self.cycles[cycle_num] = sim_cycle
    
class Sim_Tree_Node:
    def __init__(self, node_id, hop_count, parent_id, parent_rssi):
        self.node_id = node_id
        self.hop_count = hop_count
        self.parent_id =  parent_id
        self.parent_rssi = parent_rssi
        self.children_id = []

    # Checks if "incoming_parent" is a better parent and sets as parent if it is
    # Returns previous parent id if taken new parent, -1 otherwise
    def parent_compare(self, incoming_parent, incoming_parent_rssi):
        # Only take parents with equal or smaller hop counts otherwise a loop could form
        if incoming_parent.hop_count <= self.hop_count and incoming_parent_rssi > self.parent_rssi:
            # Take new parent
            previous_parent_id = self.parent_id
            self.parent_id = incoming_parent.node_id
            self.parent_rssi = incoming_parent_rssi
            self.hop_count = incoming_parent.hop_count + 1
            incoming_parent.children.append(self.node_id)
            return previous_parent_id
        return -1
    
class Sim_Link_Condition:
    def __init__(self, immediate_rssi, rssi_heuristic, packet_percentage):
        self.immediate_rssi = immediate_rssi
        self.rssi_heuristic = rssi_heuristic
        self.packet_percentage = packet_percentage
        
    def __repr__(self):
        if PARENT_USE_PATH_RSSI:
            return "Immediate RSSI: " + str(self.immediate_rssi) + "\tPath RSSI: " + str(self.rssi_heuristic) + "\tPacket Percentage: " + str(self.packet_percentage)
        else:
            return "Immediate RSSI: " + str(self.immediate_rssi) + "\tPacket Percentage: " + str(self.packet_percentage)
    
# Search msgs for "id"
# Returns RSSI if found, 0 otherwise
def search_msgs_rssi(bcast_msgs, other_msgs, id):
    for msg in bcast_msgs:
        if msg.id == id:
            return msg.rssi  
    for msg in other_msgs:
        if msg.id == id:
            return msg.rssi
    return 0
    
# Search msgs for "count"
# Returns count
def search_msgs_received_count(bcast_msgs, other_msgs, id):
    count = 0
    for msg in bcast_msgs:
        if msg.id == id:
            count += 1         
    for msg in other_msgs:
        if msg.id == id:
            count += msg.count
    return count   
    
def run_sim(sim_data):
    # Determine unique nodes
    unique_nodes = set()
    for cycle_num in sorted(sim_data.cycles.keys()):
        cycle_data = sim_data.cycles[cycle_num]
        if PRINT_INCOMING_DATA:
            print("Cycle " + str(cycle_num) + ", GNODE Trans: " + str(cycle_data.gnode_any_trans) +"***************************************")
        for node_id in cycle_data.nodes:
            unique_nodes.add(node_id)
            if PRINT_INCOMING_DATA:
                node = cycle_data.nodes[node_id]
                print("Node " + str(node_id))
                node.print_node()        
    print("Unique nodes: " + str(unique_nodes))
    
    # Form a static topology, note the current algorithm will not find the best static topology, just a working one
    best_static_topology = {}
    suitable_topology_cycle = -1
    best_added_nodes = 0
    for cycle_num in sorted(sim_data.cycles.keys()):
        cycle_data = sim_data.cycles[cycle_num]
        static_topology = {}
        node_id_queue = [0]
        static_topology[0] = Sim_Tree_Node(node_id = 0, hop_count = 0, parent_id = -1, parent_rssi = -256)
        added_nodes = 0
        while len(node_id_queue) > 0:
            parent_node = static_topology[node_id_queue.pop(0)]
            # Find all nodes that received bcast from "parent_node" this cycle_data
            for node_id in cycle_data.nodes:
                node = cycle_data.nodes[node_id]
                parent_id = parent_node.node_id
                # Check for "unconsidered_bcast" which means bcast was not used that cycle since it was received too late
                if parent_id in node.received_bcast_msg and node.received_bcast_msg[parent_id].unconsidered_bcast == 0:
                    # Determine RSSI metric based on parenting heuristic
                    direct_rssi_to_parent = node.received_bcast_msg[parent_id].rssi
                    if PARENT_USE_PATH_RSSI:
                        parent_rssi = min(direct_rssi_to_parent, parent_node.parent_rssi)
                    else: # Immediate RSSI
                        parent_rssi = direct_rssi_to_parent
                    
                    # Check if this node has a parent
                    if node_id not in static_topology: # If no parent set "parent_node" as parent
                        static_topology[node_id] = Sim_Tree_Node(   node_id = node_id, hop_count = (parent_node.hop_count + 1), \
                                                                    parent_id = parent_node.node_id, parent_rssi = parent_rssi)
                        parent_node.children_id.append(node_id)
                        added_nodes += 1
                        # Since this node hasn't been explored yet, we need to add it to the node_id_queue
                        node_id_queue.append(node_id)
                    else: # If this node already has a parent, compare possible parent with exsiting parent
                        previous_parent_id = static_topology[node_id].parent_compare(parent_node, parent_rssi)
                        # "previous_parent_id" >= 0 means new parent was taken, need to eliminate child from old parent
                        if previous_parent_id >= 0:
                            static_topology[previous_parent_id].children_id.remove(node_id)
                            node_id_queue.append(node_id)
        
        # Check if the generated static topology has as many nodes as unique nodes, so no nodes are left out
        if PRINT_ALL_TOPOLOGIES:
            if added_nodes > 0:
                if added_nodes == len(unique_nodes):
                    print("Found full topology on Cycle " + str(cycle_num) + ":")
                else:
                    print("Found partial topology on Cycle " + str(cycle_num) + ":")
                    
                for node_id in static_topology:
                    node = static_topology[node_id]
                    print("Node " + str(node_id) + ", Parent " + str(node.parent_id))
            else:
                print("Unable to find suitable any topology on Cycle " + str(cycle_num) + ", added nodes = " + str(added_nodes))

        if added_nodes > best_added_nodes:
            suitable_topology_cycle = cycle_num
            best_static_topology = static_topology
            best_added_nodes = added_nodes
            if added_nodes == len(unique_nodes):
                break

    if suitable_topology_cycle >= 0:
        if best_added_nodes == len(unique_nodes):
            print("Found full topology on Cycle " + str(suitable_topology_cycle) + ":")
        else:
            print("Found partial topology on Cycle " + str(suitable_topology_cycle) + ":")
        
        for node_id in static_topology:
                node = static_topology[node_id]
                print("Node " + str(node_id) + ", Parent " + str(node.parent_id))
    else:
        print("ERROR: Unable to find any suitable topology")
    '''
    print("DEBUG: Cycle " + str(suitable_topology_cycle) + "***************************************")
    cycle_data = sim_data.cycles[suitable_topology_cycle]
    for node_id in cycle_data.nodes:
        unique_nodes.add(node_id)
        node = cycle_data.nodes[node_id]
        print("DEBUG: Node " + str(node_id))
        node.print_node()              
    '''
            
    # Evaluate topology compared to existing data, currently only has Downstream metrics
    # TODO: Add in Upstream metrics
    cycle_link_condition_real_data = {} # From actual data
    cycle_link_condition_static = {}
    cycle_detected_cycles = set()
    for cycle_num in sorted(sim_data.cycles.keys()):
        if PRINT_ALL_REPLAY_CYCLES:
            print("Cycle " + str(cycle_num) + "***************************************", flush=True)
        cycle_data = sim_data.cycles[cycle_num]
        link_condition_static = {}
        link_condition_real_data = {}
        cycle_link_condition_real_data[cycle_num] = link_condition_real_data
        cycle_link_condition_static[cycle_num] = link_condition_static
        for node_id in cycle_data.nodes:
            if PRINT_ALL_REPLAY_CYCLES:
                print("---------------------")
            node = cycle_data.nodes[node_id]
            # Real Data Calculation
            real_data_parent_id = node.parent_id
            if PRINT_ALL_REPLAY_CYCLES:
                print("Real Data: Node " + str(node_id) + " Parent " + str(real_data_parent_id))
            ir = node.parent_rssi
            rh = ir
            if PARENT_USE_PATH_RSSI:
                cycle_detection = set()
                rh = 0 # Set high at first
                current_id = node_id
                cycle_detection.add(current_id)
                while current_id != 0:
                    if current_id in cycle_data.nodes:
                        current_node = cycle_data.nodes[current_id]
                    else:
                        # Parent does not exist in this cycle
                        if PRINT_ALL_REPLAY_CYCLES:
                            print("\tPath RSSI: Incomplete path")
                        rh = 0
                        break
                    current_parent_id = current_node.parent_id
                    current_parent_rssi = current_node.parent_rssi
                    rh = min(rh, current_parent_rssi)
                    if current_parent_id in cycle_detection:
                        if PRINT_ALL_REPLAY_CYCLES:
                            print("\tCycle detected")
                        cycle_detected_cycles.add(cycle_num)
                        rh = 1
                        break
                    else:
                        current_id = current_parent_id
                        cycle_detection.add(current_id)
            pp = -1
            if real_data_parent_id == 0 or real_data_parent_id in cycle_data.nodes:
                if real_data_parent_id == 0:
                    packets_sent = cycle_data.gnode_any_trans
                else:
                    packets_sent = cycle_data.nodes[real_data_parent_id].total_any_trans
                if packets_sent >= 0: # Less than zero means no data was recorded
                    packets_received = search_msgs_received_count(node.received_bcast_msg.values(), node.received_other_msg.values(), node.parent_id)
                    pp = (packets_received / packets_sent) if packets_received > 0 else 0
                    #print("\tDEBUG ->Parent: " + str(static_parent_id) + ", PR: " + str(packets_received) + ", PS: " + str(packets_sent))
                elif PRINT_ALL_REPLAY_CYCLES:
                    print("\tParent PREV data missing")
            elif PRINT_ALL_REPLAY_CYCLES:
                print("\tParent missing")
            link_condition_real_data[node_id] = Sim_Link_Condition(immediate_rssi = ir, rssi_heuristic = rh, packet_percentage = pp)
            if PRINT_ALL_REPLAY_CYCLES:
                print("\t" + str(link_condition_real_data[node_id]))
            
            if suitable_topology_cycle < 0:
                if PRINT_ALL_REPLAY_CYCLES:
                    print("Static: No data available")
            else:
                # Static Calculation
                static_parent_id = static_topology[node_id].parent_id
                if PRINT_ALL_REPLAY_CYCLES:
                    print("Static: Node " + str(node_id) + " Parent " + str(static_parent_id))
                # Search if node received any messages from parent
                ir = search_msgs_rssi(node.received_bcast_msg.values(), node.received_other_msg.values(), static_parent_id)
                rh = ir
                if PARENT_USE_PATH_RSSI:
                    rh = 0 # Set high at first
                    current_id = node_id
                    while current_id != 0:
                        current_node = cycle_data.nodes[current_id]
                        current_parent_id = static_topology[current_id].parent_id
                        current_parent_rssi = search_msgs_rssi(current_node.received_bcast_msg.values(), current_node.received_other_msg.values(), current_parent_id)
                        if current_parent_rssi != 0:
                            rh = min(rh, current_parent_rssi)
                            current_id = current_parent_id
                        else:
                            if PRINT_ALL_REPLAY_CYCLES:
                                print("\tPath RSSI: Incomplete path")
                            rh = 0
                            break
                pp = -1 # Unavailable data
                if static_parent_id == 0 or static_parent_id in cycle_data.nodes:
                    if static_parent_id == 0:
                        packets_sent = cycle_data.gnode_any_trans
                    else:
                        packets_sent = cycle_data.nodes[static_parent_id].total_any_trans
                    if packets_sent >= 0: # Less than zero means no data was recorded
                        packets_received = search_msgs_received_count(node.received_bcast_msg.values(), node.received_other_msg.values(), static_parent_id)
                        pp = (packets_received / packets_sent) if packets_received > 0 else 0
                        #print("\tDEBUG ->Parent: " + str(static_parent_id) + ", PR: " + str(packets_received) + ", PS: " + str(packets_sent))
                    elif PRINT_ALL_REPLAY_CYCLES:
                        print("\tParent PREV data missing")
                elif PRINT_ALL_REPLAY_CYCLES:
                    print("\tParent missing")
                link_condition_static[node_id] = Sim_Link_Condition(immediate_rssi = ir, rssi_heuristic = rh, packet_percentage = pp)
                if PRINT_ALL_REPLAY_CYCLES:
                    print("\t" + str(link_condition_static[node_id]))
    
    print("Analysis***************************************")
    
    if len(cycle_detected_cycles) > 0:
        print("Found " + str(len(cycle_detected_cycles)) + " cycles: " + str(cycle_detected_cycles))
    else:
        print("No cycles detected")
        
    for node_id in unique_nodes:
        print("Node " + str(node_id))
        stats_downstream_immediate_rssi_real_data = []
        stats_downstream_immediate_rssi_static = []
        stats_downstream_parent_heuristic_real_data = []
        stats_downstream_parent_heuristic_static = []
        stats_downstream_packet_percentage_real_data = []
        stats_downstream_packet_percentage_static = []
        for cycle_num in sorted(sim_data.cycles.keys()):
            link_condition_real_data = cycle_link_condition_real_data[cycle_num]
            link_condition_static = cycle_link_condition_static[cycle_num]
            if node_id in link_condition_real_data:
                if link_condition_real_data[node_id].immediate_rssi != 0:
                    stats_downstream_immediate_rssi_real_data.append(link_condition_real_data[node_id].immediate_rssi)
                if link_condition_real_data[node_id].rssi_heuristic != 0:
                    stats_downstream_parent_heuristic_real_data.append(link_condition_real_data[node_id].rssi_heuristic)
                if link_condition_real_data[node_id].packet_percentage >= 0:
                    stats_downstream_packet_percentage_real_data.append(link_condition_real_data[node_id].packet_percentage)
            if node_id in link_condition_static:
                if link_condition_static[node_id].immediate_rssi != 0:
                    stats_downstream_immediate_rssi_static.append(link_condition_static[node_id].immediate_rssi)
                if link_condition_static[node_id].rssi_heuristic != 0:
                    stats_downstream_parent_heuristic_static.append(link_condition_static[node_id].rssi_heuristic)
                if link_condition_static[node_id].packet_percentage >= 0:
                    stats_downstream_packet_percentage_static.append(link_condition_static[node_id].packet_percentage)
        print("\tImmediate RSSI")
        print("\t\tDownstream Real Data Mean:\t" + str(statistics.mean(stats_downstream_immediate_rssi_real_data)) + ", Valid Data Count: " + str(len(stats_downstream_immediate_rssi_real_data)))
        print("\t\tDownstream Static Mean:\t\t" + str(statistics.mean(stats_downstream_immediate_rssi_static)) + ", Valid Data Count: " + str(len(stats_downstream_immediate_rssi_static)))
        print("\tParent Heuristic RSSI")
        print("\t\tDownstream Real Data Mean:\t" + str(statistics.mean(stats_downstream_parent_heuristic_real_data)) + ", Valid Data Count: " + str(len(stats_downstream_parent_heuristic_real_data)))
        print("\t\tDownstream Static Mean:\t\t" + str(statistics.mean(stats_downstream_parent_heuristic_static)) + ", Valid Data Count: " + str(len(stats_downstream_parent_heuristic_static)))
        print("\tImmediate RSSI")
        print("\t\tDownstream Real Data Mean:\t" + str(statistics.mean(stats_downstream_packet_percentage_real_data)) + ", Valid Data Count: " + str(len(stats_downstream_packet_percentage_real_data)))
        print("\t\tDownstream Static Mean:\t\t" + str(statistics.mean(stats_downstream_packet_percentage_static)) + ", Valid Data Count: " + str(len(stats_downstream_packet_percentage_static)))