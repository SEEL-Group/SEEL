# Script to simulate collected SEEL data on different topologies

# Example usage in SEEL_log_parser.py

# Tested with Python 3.6.0

# Parameters
PRINT_INCOMING_DATA = False
PRINT_ALL_TOPOLOGIES = False

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
    def __init__(self, node_id, parent, parent_rssi, received_bcast_msg):
        self.node_id = node_id
        self.parent = parent
        self.parent_rssi = parent_rssi
        self.failed_data_trans = -1
        self.total_data_trans = -1
        self.total_any_trans = -1
        self.received_bcast_msg = {}
        self.received_other_msg = {}
        for msg in received_bcast_msg:
            self.received_bcast_msg[msg.id] = msg
    
    def set_prev_data(self, failed_data_trans, total_data_trans, total_any_trans, received_other_msg):
        self.failed_data_trans = -1
        self.total_data_trans = -1
        self.total_any_trans = -1
        for msg in received_other_msg:
            self.received_other_msg[msg.id] = msg
                
    def print_node(self):
        print("\tParent: " + str(self.parent) + \
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
        
    def add_node(self, node_id, sim_node):
        self.nodes[node_id] = sim_node
        
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
        # Only take parents with equal or higher hop counts otherwise a loop could form
        if incoming_parent.hop_count >= self.hop_count and incoming_parent_rssi > self.parent_rssi:
            # Take new parent
            previous_parent_id = self.parent_id
            self.parent_id = incoming_parent.node_id
            self.parent_rssi = incoming_parent_rssi
            self.hop_count = incoming_parent.hop_count + 1
            incoming_parent.children.append(self.node_id)
            return previous_parent_id
        return -1
    
def run_sim(sim_data):
    # Determine unique nodes
    unique_nodes = set()
    for cycle_num in sim_data.cycles:
        if PRINT_INCOMING_DATA:
            print("Cycle " + str(cycle_num) + "***************************************")
        cycle_data = sim_data.cycles[cycle_num]
        for node_id in cycle_data.nodes:
            unique_nodes.add(node_id)
            if PRINT_INCOMING_DATA:
                node_data = cycle_data.nodes[node_id]
                print("Node " + str(node_id))
                node_data.print_node()        
    print("Unique nodes: " + str(unique_nodes))
    
    # Form a static topology, note the current algorithm will not find the best static topology, just a working one
    static_topology = {}
    suitable_topology_cycle = -1
    for cycle_num in sim_data.cycles:
        cycle_data = sim_data.cycles[cycle_num]
        static_topology = {}
        node_id_queue = [0]
        static_topology[0] = Sim_Tree_Node(node_id = 0, hop_count = 0, parent_id = -1, parent_rssi = -256)
        added_nodes = 0
        while len(node_id_queue) > 0:
            parent_node = static_topology[node_id_queue.pop(0)]
            # Find all nodes that received bcast from "parent_node" this cycle_data
            for node_id in cycle_data.nodes:
                node_data = cycle_data.nodes[node_id]
                parent_id = parent_node.node_id
                # Check for "unconsidered_bcast" which means bcast was not used that cycle since it was received too late
                if parent_id in node_data.received_bcast_msg and node_data.received_bcast_msg[parent_id].unconsidered_bcast == 0:
                    # Determine RSSI metric based on parenting heuristic
                    direct_rssi_to_parent = node_data.received_bcast_msg[parent_id].rssi
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
            if added_nodes == len(unique_nodes):
                print("Found suitable topology on Cycle " + str(cycle_num) + ":")
                for node_id in static_topology:
                    node_data = static_topology[node_id]
                    print("Node " + str(node_id) + ", Parent " + str(node_data.parent_id))
            else:
                print("Unable to find suitable topology on Cycle " + str(cycle_num) + ", added nodes = " + str(added_nodes))
        else:
            suitable_topology_cycle = cycle_num
            break
    if not PRINT_ALL_TOPOLOGIES:
        if suitable_topology_cycle < 0:
            print("Unable to find suitable topology on Cycle " + str(cycle_num) + ", added nodes = " + str(added_nodes))
        else:
            print("Found suitable topology on Cycle " + str(suitable_topology_cycle) + ":")
            for node_id in static_topology:
                node_data = static_topology[node_id]
                print("Node " + str(node_id) + ", Parent " + str(node_data.parent_id))
            '''
            print("DEBUG: Cycle " + str(suitable_topology_cycle) + "***************************************")
            cycle_data = sim_data.cycles[suitable_topology_cycle]
            for node_id in cycle_data.nodes:
                unique_nodes.add(node_id)
                node_data = cycle_data.nodes[node_id]
                print("DEBUG: Node " + str(node_id))
                node_data.print_node()              
            '''