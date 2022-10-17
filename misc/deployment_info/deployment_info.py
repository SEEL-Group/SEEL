class Parameters:
    ############################################################################
    # General Parameters
    PRINT_ALL_MSGS = True
    PRINT_ALL_MSGS_EXTENDED = True

    PLOT_DISPLAY = True
    PLOT_NODE_SPECIFIC_BCASTS = True
    PLOT_NODE_SPECIFIC_CONNECTIONS = True
    PLOT_NODE_SPECIFIC_MAPS = True
    PLOT_NODE_PARENT_CYCLE_RSSI = True
    
    PLOT_RSSI_ANALYSIS = True
    PLOT_LOCS_WEIGHT_SCALAR = 1000 # Smaller for thicker lines
    PLOT_LOCS_WEIGHT_SCALAR_SPECIFIC = 500 # Smaller for thicker lines

    PARAM_COUNT_WRAP_SAFETY = 10 # Send count will not have wrapped within this many counts, keep it lower to account for node restarts too

    ############################################################################
    # Hardcode Section
    HC_NJ_ORIGINAL_ID_IDX = 0
    HC_NJ_ASSIGNED_ID_IDX = 1
    HC_NJ_CYCLE_JOIN_IDX = 2
    HARDCODED_NODE_JOINS = [
        # Format: [actual ID, assigned ID, cycle join]
    ]


    HARDCODED_NODE_LOCS = {
        # Format: actual ID: (loc_x, loc_y)
    }
    
    # Node TDMA slots
    HARDCODED_NODE_TDMA = {
        # Format -> node ID: TDMA slot
    }
    
    # Excludes nodes from correlation plots
    # Useful for any outliers that may skew regressions
    HARDCODED_PLOT_EXCLUDE = {
        # Format -> node_id
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
    INDEX_DATA_PREV_DATA_TRANS = 11
    INDEX_DATA_MISSED_MSGS = 12
    INDEX_DATA_MAX_QUEUE_SIZE = 13
    INDEX_DATA_CRC_FAILS = 14
    INDEX_DATA_FLAGS = 15
    INDEX_DATA_HC_DOWNSTREAM = 16
    INDEX_DATA_HC_UPSTREAM = 17
    INDEX_DATA_DROPPED_MSGS_SELF = 18
    INDEX_DATA_DROPPED_MSGS_OTHERS = 19
    INDEX_DATA_FAILED_TRANS = 20
    INDEX_DATA_PREV_TRANS_BCAST = 21
    INDEX_DATA_PREV_TRANS_DATA = 22
    INDEX_DATA_PREV_TRANS_ID_CHECK = 23
    INDEX_DATA_PREV_TRANS_ACK = 24
    INDEX_DATA_PREV_TRANS_FWD = 25
    ###
    # VEC RECEIVED_BCASTS
    INDEX_DATA_VEC_RECEIVED_BCASTS_IND = 26
    INDEX_DATA_VEC_RECEIVED_BCASTS_SIZE = 8
    INDEX_DATA_VEC_RECEIVED_BCASTS_NUM_V = 2
    # V1: ID, 1st bit (MSB) denotes if the incoming bcast was received after this node already sent a bcast, remaining bit for node ID
    # V2: RSSI
    ###
    # VEC RECEIVED MSGS
    INDEX_DATA_VEC_RECEIVED_MSGS_IND = 42
    INDEX_DATA_VEC_RECEIVED_MSGS_SIZE = 8
    INDEX_DATA_VEC_RECEIVED_MSGS_NUM_V = 3
    # V1 = 0 # ID
    # V2 = 0 # RSSI, latest sender
    # V3 = 0 # Misc, 1st bit (MSB) denotes if sender was child, remaining bits is send count
    ###

    ############################################################################
    # SEEL Parameters
    SEEL_CYCLE_AWAKE_TIME_MILLIS = 6000
    SEEL_CYCLE_SLEEP_TIME_MILLIS = 6000
    
    SEEL_FORCE_SLEEP_AWAKE_MULT = 1.0
    SEEL_FORCE_SLEEP_AWAKE_DURATION_SCALE = 1.5
    SEEL_FORCE_SLEEP_RESET_COUNT = 3