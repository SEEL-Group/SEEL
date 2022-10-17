# Script to simulate collected SEEL data on different topologies

# Example usage in SEEL_log_parser.py

# Tested with Python 3.6.0

# Parameters
PRINT_INCOMING_DATA = True

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
    def __init__(self, parent, parent_rssi, failed_data_trans, total_data_trans, total_any_trans, received_bcast_msg, received_other_msg):
        self.parent = parent
        self.parent_rssi = parent_rssi
        self.failed_data_trans = failed_data_trans
        self.total_data_trans = total_data_trans
        self.total_any_trans = total_any_trans
        self.received_bcast_msg = received_bcast_msg
        self.received_other_msg = received_other_msg
    
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
    
def run_sim(sim_data):
    if PRINT_INCOMING_DATA:
        for cycle_num in sim_data.cycles:
            print("Cycle " + str(cycle_num) + "***************************************")
            cycle_data = sim_data.cycles[cycle_num]
            for node_id in cycle_data.nodes:
                node_data = cycle_data.nodes[node_id]
                print("Node " + str(node_id))
                print("\tParent: " + str(node_data.parent) + \
                    "\tParent RSSI: " + str(node_data.parent_rssi) + \
                    "\tFailed D. Trans: " + str(node_data.failed_data_trans) + \
                    "\tTotal D. Trans: " + str(node_data.total_data_trans) + \
                    "\tTotal A. Trans: " + str(node_data.total_any_trans))
                print("\tReceived Broadcast messages")
                for b_msg in node_data.received_bcast_msg:
                    print("\t\t" + str(b_msg))
                print("\tReceived Non-Broadcast messages")
                if node_data.received_other_msg != None:
                    for nb_msg in node_data.received_other_msg:
                        print("\t\t" + str(nb_msg))
                else:
                    print("\t\tNone")