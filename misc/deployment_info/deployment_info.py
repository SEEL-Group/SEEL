class Parameters:
############################################################################
    # General Parameters
    PRINT_ALL_MSGS = True

    PLOT_DISPLAY = True
    PLOT_NODE_SPECIFIC_BCASTS = True
    PLOT_NODE_SPECIFIC_CONNECTIONS = True
    PLOT_NODE_SPECIFIC_MAPS = True
    PLOT_RSSI_ANALYSIS = True
    PLOT_LOCS_WEIGHT_SCALAR = 1000 # Smaller for thicker lines
    PLOT_LOCS_WEIGHT_SCALAR_SPECIFIC = 500 # Smaller for thicker lines

    PARAM_COUNT_WRAP_SAFETY = 10 # Send count will not have wrapped within this many counts, keep it lower to account for node restarts too

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
    INDEX_DATA_MAX_QUEUE_SIZE = 13
    INDEX_DATA_CRC_FAILS = 14
    INDEX_DATA_FLAGS = 15

############################################################################