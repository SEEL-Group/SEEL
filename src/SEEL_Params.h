/*
The SEEL repository can be found at: https://github.com/SEEL-Group/SEEL
Copyright (C) SEEL Group 2021 all rights reserved
See license file in root folder for more licensing details
See SEEL_documentation.pdf for protocol description details

File purpose:   Contains adjustable parameters (along with recommended defaults) for the SEEL protocol.
*/

#ifndef SEEL_Params_h
#define SEEL_Params_h

#include <LowPower.h> // To access LowPower.h constants

/*
    Note: Any changes to the width of these constants will require similar changes to 
    their assigned and compared against variables in the rest of the code.
*/

// ***************************************************
/* SEEL Debug */

const bool SEEL_ASSERT_ENABLE = true;
const bool SEEL_ASSERT_ENABLE_NVM = true; // If true, assert uses NVM
const bool SEEL_ASSERT_NVM_PRINT_BLOCK = true; // If true, on print_nvm(), print entire NVM block
const uint8_t SEEL_ASSERT_NVM_PRINT_BLOCK_WIDTH = 32; // How many NVM cells to print per line in the block
// NVM partitions are associated with these parameters; so if these parameters are changed,
// then previous NVM writes are no longer valid
// NVM entry storage system (designed for wear leveling):
// [(1bit, cell used); (7bits, (uint8)(File_num >> 8))] [(8bits, (uint8)File_num)] [(8bits, (uint8)(Line_num >> 8))] [(8bits, (uint8)Line_num)]
const uint8_t SEEL_ASSERT_NVM_CELLS_PER_ENTRY = 4; // Number of NVM cells for a single entry
// Max file and line num to partition assert writes to EEPROM
const uint16_t SEEL_ASSERT_NVM_MAX_FILE_NUM = 32767; // 15 bits for file
const uint16_t SEEL_ASSERT_NVM_MAX_LINE_NUM = 65535; // 16 bits for line

// ***************************************************
/* SEEL Queue */

const uint8_t SEEL_QUEUE_ALLOCATION_SIZE = 7; // Allocation size of ALL queues used in SEEL

// ***************************************************
/* SEEL LoRa Params */

const uint32_t SEEL_RFM95_FREQ = 915E6;
const int8_t SEEL_RFM95_SF = 12;
const uint32_t SEEL_RFM95_BW = 250E3;
const int8_t SEEL_RFM95_GNODE_TX = 20; // 2 to 20 for PA Boost
const int8_t SEEL_RFM95_GNODE_CR = 5; // 5 to 8
const int8_t SEEL_RFM95_SNODE_TX = 20; // 2 to 20 for PA Boost
const int8_t SEEL_RFM95_SNODE_CR = 5; // 5 to 8

// SEEL Message size
// Additional bytes allocated for the message packet
// In addition to the default SEEL_MSG_MISC_SIZE in SEEL_Defines.h
// Influences the ToA of a message, which may require changing TDMA parameters (if using TDMA)
// More allocated bytes lets users send more data at a time, allows more SNODEs to join the network per cycle,
// and increases the number of NODEs that can be ACK'd per ACK message
const uint32_t SEEL_MSG_USER_SIZE = 2;

// Duplicate msg holder
// How many messages to hold when checking for duplicates
const uint8_t SEEL_DUP_MSG_SIZE = 3;

// ***************************************************
/* SEEL_GNode */

// Maximum of 2^(targ_id/send_id size)
// HIGH impact on device memory for GNODE
const uint16_t SEEL_MAX_NODES = 128;

// Node kick-out
// Note: A node may take multiple cycles to reach GNode due to collisions
// so 5 cycle misses may not be the same as the node not responding 5 times
// MAX 127 because of there are only 7 bits being used for representation in the SEEL_ID_INFO bitfield
const uint8_t SEEL_MAX_CYCLE_MISSES = 10;

// ***************************************************
/* SEEL_SNode */

// Upperbound transmission duration used to create TDMA slot widths. Also used as initial estimate 
// to correct for transmission delay when time sychronizing; value will be updated with measured msg send ToA
// SEEL_Print'ed in RFM send method
const uint32_t SEEL_TRANSMISSION_UB_DUR_MILLIS = 700;

// How long Arduino watchdog timer can sleep at a time
// Only select values can be used, check Arduino WD specs (SLEEP_8S is maximum duration per sleep instance)
// Changing this value may require changes to SEEL_ADJUSTED_SLEEP_INITAL_ESTIMATE_MILLIS
const period_t SEEL_WD_TIMER_DUR = SLEEP_8S; // From LowPower.h

// Initial estimate for the duration of a single WD sleep duration in milliseconds
// This value should be an overestimate of the duration (max of the deviation range)
const uint32_t SEEL_ADJUSTED_SLEEP_INITAL_ESTIMATE_MILLIS = 10000;
// How early to wake up SNODE to prepare for bcast
// Consider making this value bigger as the sleep time increases to have a safer margin of error from WD's deviation
const uint32_t SEEL_ADJUSTED_SLEEP_EARLY_WAKE_MILLIS = 10000;

// Enabling force sleep makes SNODEs go to sleep after being awake for the maximum awake time
// Note awake time for force sleep starts taking awake time since wake up, not when the bcast msg was received
// Thus (max awake) = (specified awake time * SEEL_FORCE_SLEEP_AWAKE_MULT) + (WTB time)
// Note SEEL_FORCE_SLEEP_AWAKE_MULT should be greater than one (to account for WTB time) and the total max awake time should be less than the specified sleep time
// Since (if set correctly) force sleep activating means the broadcast msg was missed, the SNODE may use old specified sleep values
const float SEEL_FORCE_SLEEP_AWAKE_MULT = 1.0f;
// Adjust awake duration multiplicatively by SEEL_FORCE_SLEEP_AWAKE_DURATION_SCALE since force sleep
// wakes up SNODE earlier and earlier each bcast miss due to WTB
// awake duration = (specified awake duration) * (SEEL_FORCE_SLEEP_AWAKE_DURATION_SCALE ^ (missed bcasts))
const float SEEL_FORCE_SLEEP_AWAKE_DURATION_SCALE = 1.5f;
// After SEEL_FORCE_SLEEP_RESET_COUNT bcasts are missed, force sleep is disabled and the SNODE stays awake
// until receiving the next bcast
// On receiving a bcast, the missed bcast counter is set to 0
// Set this value to 0 to disable force sleep (always stay on waiting for bcast)
const uint32_t SEEL_FORCE_SLEEP_RESET_COUNT = 3;

// RSSI-based Parent Selection
// Selection Modes
// Ties in RSSI modes broken by lowest hop count
enum SEEL_PARENT_SELECTION_MODE
{
    SEEL_PSEL_FIRST_BROADCAST, // Selects parent based off of sender of first received broadcast message
    SEEL_PSEL_IMMEDIATE_RSSI,  // Selects parent based off of the immediate sender with the largest RSSI among received broadcast messages
    SEEL_PSEL_PATH_RSSI // Selects parent with the best path RSSI, where path RSSI is determined by the worst RSSI along the path
};
// If enabled, collects broadcasts for a duration of SEEL_SMART_PARENT_DURATION_MILLIS and chooses node with highest RSSI to be parent. 
// If SEEL_SMART_PARENT_DURATION_MILLIS=0 then SNode will collect broadcasts until it's time to send broadcast.
const SEEL_PARENT_SELECTION_MODE SEEL_PSEL_MODE = SEEL_PSEL_PATH_RSSI;
const uint32_t SEEL_PSEL_DURATION_MILLIS = 0; // Should be much less than awake time of SNODE, Special Case if zero (refer to above).

// Collision avoidance scheme 1: TDMA
// Time for all slots to send = transmission_duration * slots + buffer * (slots - 1)
// Pros: Shorter wait window, more predictable performance
// Cons: Requires user setup and calculation unique to each deployment
const bool SEEL_TDMA_USE_TDMA = true; // Otherwise uses Exponential backoff
const uint8_t SEEL_TDMA_SLOTS = 10; // Maximum group of nodes, first slot begins at 0
const uint32_t SEEL_TDMA_BUFFER_MILLIS = 500; // Buffer time between scheduled TMDA transmissions, factors in receive buffer copy delay (SEEL_Print'ed in RFM receive method)
const uint32_t SEEL_TDMA_SLOT_WAIT_MILLIS = SEEL_TRANSMISSION_UB_DUR_MILLIS + SEEL_TDMA_BUFFER_MILLIS;
const uint32_t SEEL_TDMA_CYCLE_TIME_MILLIS = SEEL_TDMA_SLOT_WAIT_MILLIS * SEEL_TDMA_SLOTS;

// Collision avoidance scheme 2: Exponential backoff 
// Pros: Less user parameter tuning
// Cons: Longer wait window
const uint32_t SEEL_EB_INIT_MILLIS = 10000; // How long first backoff max is
const uint32_t SEEL_EB_MIN_MILLIS = 0;
const float SEEL_EB_EXP_SCALE = 2.0f;

#endif // SEEL_Params