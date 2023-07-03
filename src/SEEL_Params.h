/*
The SEEL repository can be found at: https://github.com/SEEL-Group/SEEL
Copyright (C) SEEL Group 2021 all rights reserved
See license file in root folder for more licensing details
See SEEL_documentation.pdf for protocol description details

File purpose:   Contains adjustable parameters (along with recommended defaults) for the SEEL protocol.
*/

#ifndef SEEL_Params_h
#define SEEL_Params_h

#include <LowPower.h> // To access LowPower.h constexprants

/*
    Note: Any changes to the width of these constexprants will require similar changes to 
    their assigned and compared against variables in the rest of the code.
*/

#define TRUE 1
#define FALSE 0

// ***************************************************
/* SEEL Debug */

#define SEEL_ASSERT_ENABLE TRUE
#define SEEL_ASSERT_ENABLE_NVM FALSE // If true, assert uses NVM writes
constexpr bool SEEL_ASSERT_NVM_PRINT_BLOCK = true; // If true, on print_nvm(), print entire NVM block
constexpr uint8_t SEEL_ASSERT_NVM_PRINT_BLOCK_WIDTH = 32; // How many NVM cells to print per line in the block
// NVM partitions are associated with these parameters; so if these parameters are changed,
// then previous NVM writes are no longer valid
// NVM entry storage system (designed for wear leveling):
// [(1bit, cell used); (7bits, (uint8)(File_num >> 8))] [(8bits, (uint8)File_num)] [(8bits, (uint8)(Line_num >> 8))] [(8bits, (uint8)Line_num)]
constexpr uint8_t SEEL_ASSERT_NVM_CELLS_PER_ENTRY = 4; // Number of NVM cells for a single entry
// Max file and line num to partition assert writes to EEPROM
constexpr uint16_t SEEL_ASSERT_NVM_MAX_FILE_NUM = 32767; // 15 bits for file
constexpr uint16_t SEEL_ASSERT_NVM_MAX_LINE_NUM = 65535; // 16 bits for line

// ***************************************************
/* SEEL Queue */

constexpr uint8_t SEEL_DEFAULT_QUEUE_SIZE = 10; // Allocation size of ALL queues used in SEEL
constexpr uint8_t SEEL_SNODE_MSG_QUEUE_SIZE = 7; // Optimize for message size in buffers
constexpr uint8_t SEEL_SCHED_QUEUE_SIZE = 10; // Must maintain minimum size (7) for scheduler to function

// ***************************************************
/* SEEL LoRa Params */

constexpr uint32_t SEEL_RFM95_FREQ = 915E6;
constexpr int8_t SEEL_RFM95_SF = 7;
constexpr uint32_t SEEL_RFM95_BW = 500E3;
constexpr int8_t SEEL_RFM95_GNODE_TX = 2; // 2 to 20 for PA Boost
constexpr int8_t SEEL_RFM95_GNODE_CR = 5; // 5 to 8
constexpr int8_t SEEL_RFM95_SNODE_TX = 2; // 2 to 20 for PA Boost
constexpr int8_t SEEL_RFM95_SNODE_CR = 5; // 5 to 8

// SEEL Message size
// Additional bytes allocated for the message packet
// In addition to the default SEEL_MSG_MISC_SIZE in SEEL_Defines.h
// Influences the ToA of a message, which may require changing TDMA parameters (if using TDMA)
// More allocated bytes lets users send more data at a time, allows more SNODEs to join the network per cycle,
// and increases the number of NODEs that can be ACK'd per ACK message
<<<<<<< Updated upstream
constexpr uint32_t SEEL_MSG_USER_SIZE = 4;
=======
constexpr uint32_t SEEL_MSG_USER_SIZE = 0;
>>>>>>> Stashed changes

// Duplicate msg holder
// How many messages to hold when checking for duplicates
constexpr uint8_t SEEL_DUP_MSG_SIZE = 3;

// ***************************************************
/* SEEL_GNode */

// Maximum of 2^(targ_id/send_id size)
// HIGH impact on device memory for GNODE
constexpr uint16_t SEEL_MAX_NODES = 128;

// Node kick-out
// Note: A node may take multiple cycles to reach GNode due to collisions
// so 5 cycle misses may not be the same as the node not responding 5 times
// MAX 127 because of there are only 7 bits being used for representation in the SEEL_ID_INFO bitfield
constexpr uint8_t SEEL_MAX_CYCLE_MISSES = 25;

// ***************************************************
/* SEEL_SNode */

// Upperbound transmission duration used to create TDMA slot widths. Also used as initial estimate 
// to correct for transmission delay when time sychronizing; value will be updated with measured msg send ToA
// SEEL_Print'ed in RFM send method
constexpr uint32_t SEEL_TRANSMISSION_UB_DUR_MILLIS = 100;

// How long Arduino watchdog timer can sleep at a time
// Only select values can be used, check Arduino WD specs (SLEEP_8S is maximum duration per sleep instance)
// Changing this value may require changes to SEEL_ADJUSTED_SLEEP_INITAL_ESTIMATE_MILLIS
constexpr period_t SEEL_WD_TIMER_DUR = SLEEP_1S; // From LowPower.h

// Initial estimate for the duration of a single WD sleep duration in milliseconds
// This value should be an overestimate of the duration (max of the deviation range)
constexpr uint32_t SEEL_ADJUSTED_SLEEP_INITAL_ESTIMATE_MILLIS = 1000;
// How early to wake up SNODE to prepare for bcast
// Consider making this value bigger as the sleep time increases to have a safer margin of error from WD's deviation
constexpr uint32_t SEEL_ADJUSTED_SLEEP_EARLY_WAKE_MILLIS = 0;

// Enabling force sleep makes SNODEs go to sleep after being awake for the maximum awake time
// Note awake time for force sleep starts taking awake time since wake up, not when the bcast msg was received
// Thus (max awake) = (specified awake time * SEEL_FORCE_SLEEP_AWAKE_MULT) + (WTB time)
// Note SEEL_FORCE_SLEEP_AWAKE_MULT should be greater than one (to account for WTB time) and the total max awake time should be less than the specified sleep time
// Since (if set correctly) force sleep activating means the broadcast msg was missed, the SNODE may use old specified sleep values
constexpr float SEEL_FORCE_SLEEP_AWAKE_MULT = 1.0f;
// Adjust awake duration multiplicatively by SEEL_FORCE_SLEEP_AWAKE_DURATION_SCALE since force sleep
// wakes up SNODE earlier and earlier each bcast miss due to WTB
// awake duration = (specified awake duration) * (SEEL_FORCE_SLEEP_AWAKE_DURATION_SCALE ^ (missed bcasts))
constexpr float SEEL_FORCE_SLEEP_AWAKE_DURATION_SCALE = 1.5f;
// After SEEL_FORCE_SLEEP_RESET_COUNT bcasts are missed, force sleep is disabled and the SNODE stays awake
// until receiving the next bcast
// On receiving a bcast, the missed bcast counter is set to 0
// Set this value to 0 to disable force sleep (always stay on waiting for bcast)
constexpr uint32_t SEEL_FORCE_SLEEP_RESET_COUNT = 3;

// Collision avoidance scheme 1: TDMA
// Time for all slots to send = transmission_duration * slots + buffer * (slots - 1)
// Pros: Shorter wait window, more predictable performance
// Cons: Requires user setup and calculation unique to each deployment
// [*SEEL_TDMA_PRE_BUFFER_MILLIS*|******SEEL_TRANSMISSION_UB_DUR_MILLIS******|*SEEL_TDMA_POST_BUFFER_MILLIS*]
// Prebuffer determines how many repeat messages can fit in a slot
// Postbuffer allows widening the TDMA slot for misc processing delays
constexpr bool SEEL_TDMA_USE_TDMA = true; // Otherwise uses Exponential backoff
constexpr bool SEEL_TDMA_SINGLE_SEND = true; // Only sends 1 message per TDMA slot, otherwise sends as many as possible
constexpr uint8_t SEEL_TDMA_SLOTS = 10; // Maximum group of nodes, first slot begins at 0
constexpr uint32_t SEEL_TDMA_BUFFER_MILLIS = 100; // Buffer time between scheduled TMDA transmissions, factors in receive buffer copy delay (SEEL_Print'ed in RFM receive method)
constexpr uint32_t SEEL_TDMA_SLOT_WAIT_MILLIS = SEEL_TRANSMISSION_UB_DUR_MILLIS + SEEL_TDMA_BUFFER_MILLIS;
constexpr uint32_t SEEL_TDMA_CYCLE_TIME_MILLIS = SEEL_TDMA_SLOT_WAIT_MILLIS * SEEL_TDMA_SLOTS;

// Collision avoidance scheme 2: Exponential backoff 
// Pros: Less user parameter tuning
// Cons: Longer wait window
constexpr uint32_t SEEL_EB_INIT_MILLIS = 10000; // How long first backoff max is
constexpr uint32_t SEEL_EB_MIN_MILLIS = 0;
constexpr float SEEL_EB_EXP_SCALE = 2.0f;

// RSSI-based Parent Selection
// Selection Modes
// Ties in RSSI modes broken by lowest hop count
enum SEEL_PARENT_SELECTION_MODE
{
    SEEL_PSEL_FIRST_BROADCAST, // Selects parent based off of sender of first received broadcast message
    SEEL_PSEL_IMMEDIATE_RSSI,  // Selects parent based off of the immediate sender with the largest RSSI among received broadcast messages
    SEEL_PSEL_PATH_RSSI // Selects parent with the best path RSSI, where path RSSI is determined by the worst RSSI along the path
};
// If enabled, collects broadcasts for a duration of SEEL_SMART_PARENT_DURATION_MILLIS after receiving a bcast and update parent if better
constexpr SEEL_PARENT_SELECTION_MODE SEEL_PSEL_MODE = SEEL_PSEL_PATH_RSSI;
constexpr uint32_t SEEL_PSEL_DURATION_MILLIS = SEEL_TDMA_CYCLE_TIME_MILLIS;

#endif // SEEL_Params