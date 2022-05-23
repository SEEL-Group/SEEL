/*
The SEEL repository can be found at: https://github.com/SEEL-Group/SEEL
Copyright (C) SEEL Group 2021 all rights reserved
See license file in root folder for more licensing details
See SEEL_documentation.pdf for protocol description details

File purpose:   See SEEL_SNode.h
*/

#include "SEEL_SNode.h"

uint32_t SEEL_SNode::generate_id()
{
    // Make sure GNode is configured to handle ID size and there are enough bits in message protocol
    uint32_t largest_id = min(pow(2, SEEL_MSG_TARG_SIZE * 8), SEEL_MAX_NODES);
    // Make sure ID is not 0, since Gnode is ID 0
    uint32_t new_id = random(1, largest_id);
    _id_verified = false;

    SEEL_Print::print(F("New NODE ID: ")); SEEL_Print::println(new_id); SEEL_Print::flush();

    return new_id;
}

void SEEL_SNode::init(  SEEL_Scheduler* ref_scheduler,
            user_callback_load_t user_cb_load, 
            user_callback_presend_t user_cb_presend,
            user_callback_forwarding_t user_cb_forwarding,
            uint8_t cs_pin, uint8_t reset_pin, uint8_t int_pin, 
            uint32_t snode_id, uint32_t tdma_slot)
{
    SEEL_Node::init((snode_id == SEEL_GNODE_ID) ? generate_id() : snode_id, tdma_slot);
    rfm_param_init(cs_pin, reset_pin, int_pin, SEEL_RFM95_SNODE_TX, SEEL_RFM95_SNODE_CR);

    // Initialize member variables
    _ref_scheduler = ref_scheduler;
    _user_cb_load = user_cb_load;
    _user_cb_forwarding = user_cb_forwarding;
    _user_cb_presend = user_cb_presend; // called in SEEL_Node.cpp
    _unique_key = random(0, UINT32_MAX);
    _snode_awake_time_secs = 0;
    _snode_sleep_time_secs = 0;
    _sleep_time_estimate_millis = SEEL_ADJUSTED_SLEEP_INITAL_ESTIMATE_MILLIS;
    _sleep_time_offset_millis = 0;
    _missed_bcasts = 0;
    _last_parent = 0;
    _id_verified = false;
    _system_sync = false;
    _acked = true;
    _WD_adjusted = false;

    // Set task instances
    _task_wake.set_inst(this);
    _task_receive.set_inst(this);
    _task_enqueue_msg.set_inst(this);
    _task_user.set_inst(this);
    _task_sleep.set_inst(this);
    _task_force_sleep.set_inst(this);

    bool added = _ref_scheduler->add_task(&_task_wake);
    SEEL_Assert::assert(added, SEEL_ASSERT_FILE_NUM_SNODE, __LINE__);
}

void SEEL_SNode::SEEL_Task_SNode_Wake::run()
{
    SEEL_Print::println(F("Wake"));
    _inst->_cb_info.wtb_millis = millis();
    _inst->_msg_send_delay = 0;
    _inst->_unack_msgs = 0;
    _inst->_data_msgs_sent = 0;
    _inst->_CRC_fails = 0;
    _inst->_bcast_received = false;
    _inst->_parent_sync = false;
    _inst->_cb_info.first_callback = true; // Allows ability to only send 1 message per cycle
    _inst->_bcast_avail = false;
    _inst->_bcast_sent = false; // Set to true in SEEL_Node.cpp when bcast msg sent out

    _inst->_cb_info.hop_count = UINT8_MAX;
    _inst->_cb_info.parent_rssi = 0;
    _inst->_path_rssi = INT8_MIN;

    // Clear ack queue
    _inst->_ack_queue.clear();

    // Disables user tasks from running until critical LoRa tasks are done
    _inst->_ref_scheduler->set_user_task_enable(false); 

    // Add cycle tasks to scheduler
    bool added = _inst->_ref_scheduler->add_task(&_inst->_task_receive);
    SEEL_Assert::assert(added, SEEL_ASSERT_FILE_NUM_SNODE, __LINE__);

    // Force sleep, so SNODE can sleep even if it misses the bcast
    // Cannot force sleep until WD timer is properly adjusted
    // After SEEL_FORCE_SLEEP_RESET_COUNT of missed bcasts, disable forced sleep
    if(_inst->_WD_adjusted && _inst->_missed_bcasts < SEEL_FORCE_SLEEP_RESET_COUNT)
    {
        added = _inst->_ref_scheduler->add_task(&_inst->_task_force_sleep,
            SEEL_FORCE_SLEEP_AWAKE_MULT * _inst->_snode_awake_time_secs * SEEL_SECS_TO_MILLIS *
            pow(SEEL_FORCE_SLEEP_AWAKE_DURATION_SCALE, _inst->_missed_bcasts + 1));
        SEEL_Assert::assert(added, SEEL_ASSERT_FILE_NUM_SNODE, __LINE__);
    }
    else
    {
        // WD_adjusted turns from true to false here if there are too many missed bcasts
        // With too many missed bcasts, the WD timer may have drifted too far, so force re-adjust
        _inst->_WD_adjusted = false;

        // Reset sleep estimate since the oversleep issue could be due to inaccurate sleep time estimate
        // Not reset the estimate may cause a over-sleep loop since estimate not adjusted normally after missed bcasts
        _inst->_sleep_time_estimate_millis = SEEL_ADJUSTED_SLEEP_INITAL_ESTIMATE_MILLIS;
    }
}

void SEEL_SNode::bcast_setup(SEEL_Message &msg, uint32_t receive_offset)
{
    // Calculate wakeup-to-bcast time before time shift
    SEEL_Assert::assert(millis() >=  _cb_info.wtb_millis, SEEL_ASSERT_FILE_NUM_SNODE, __LINE__);
    _cb_info.wtb_millis = millis() - _cb_info.wtb_millis;

    /* Update system with values from bcast, values stored in Big Endian (MSB in lower address) */
    
    // Update local time, transmission delay is accounted for on sender side
    // Account for reception time via receive offset, the time taken in the rfm receive method
    uint32_t millis_update = 0;
    millis_update += (uint32_t)msg.data[SEEL_MSG_DATA_TIME_SYNC_INDEX] << 24;
    millis_update += (uint32_t)msg.data[SEEL_MSG_DATA_TIME_SYNC_INDEX + 1] << 16;
    millis_update += (uint32_t)msg.data[SEEL_MSG_DATA_TIME_SYNC_INDEX + 2] << 8;
    millis_update += (uint32_t)msg.data[SEEL_MSG_DATA_TIME_SYNC_INDEX + 3];
    millis_update += receive_offset;
    _ref_scheduler->adjust_time(millis_update);

    // Bcast will be modified such that sender becomes this node
    _bcast_received = true; // resets every cycle

    // SEEL_MSG_DATA_FIRST_BCAST_INDEX index is 1 if first bcast, otherwise 0
    // system_sync should only be true if it was previously sync'd and the msg is NOT a first_bcast
    _system_sync &= (msg.data[SEEL_MSG_DATA_FIRST_BCAST_INDEX] != SEEL_BCAST_FB);

    // Update awake time, stored time in seconds, big Endian
    _snode_awake_time_secs = 0;
    _snode_awake_time_secs += (uint32_t)msg.data[SEEL_MSG_DATA_AWAKE_TIME_SECONDS_INDEX] << 24;
    _snode_awake_time_secs += (uint32_t)msg.data[SEEL_MSG_DATA_AWAKE_TIME_SECONDS_INDEX + 1] << 16;
    _snode_awake_time_secs += (uint32_t)msg.data[SEEL_MSG_DATA_AWAKE_TIME_SECONDS_INDEX + 2] << 8;
    _snode_awake_time_secs += (uint32_t)msg.data[SEEL_MSG_DATA_AWAKE_TIME_SECONDS_INDEX + 3];

    _snode_sleep_time_secs = 0;
    _snode_sleep_time_secs += (uint32_t)msg.data[SEEL_MSG_DATA_SLEEP_TIME_SECONDS_INDEX] << 24;
    _snode_sleep_time_secs += (uint32_t)msg.data[SEEL_MSG_DATA_SLEEP_TIME_SECONDS_INDEX + 1] << 16;
    _snode_sleep_time_secs += (uint32_t)msg.data[SEEL_MSG_DATA_SLEEP_TIME_SECONDS_INDEX + 2] << 8;
    _snode_sleep_time_secs += (uint32_t)msg.data[SEEL_MSG_DATA_SLEEP_TIME_SECONDS_INDEX + 3];

    // Add sleep ASAP to keep awake time accurate, should be done AFTER local and awake times are updated
    // Parenting off a new SNODE parent may cause delays to miss original nodes
    // so stay awake less (by diff) if WTB greater than SEEL_ADJUSTED_SLEEP_EARLY_WAKE_MILLIS
    /*
    uint32_t awake_offset = 0;
    if(_system_sync && _cb_info.missed_bcasts == 0 && _last_parent != _parent_id && 
        _cb_info.wtb_millis >= SEEL_ADJUSTED_SLEEP_EARLY_WAKE_MILLIS)
    {
        awake_offset = _cb_info.wtb_millis - SEEL_ADJUSTED_SLEEP_EARLY_WAKE_MILLIS;
    }
    awake_offset = min(awake_offset, _snode_awake_time_secs * SEEL_SECS_TO_MILLIS); // Safety to prevent uint32 wraparound
    SEEL_Print::print(F("DEBUG: AWAKE OFFSET: ")); SEEL_Print::println(awake_offset);
    */
    bool added = _ref_scheduler->add_task(&_task_sleep, _snode_awake_time_secs * SEEL_SECS_TO_MILLIS); // - awake_offset); // Delay sleep task by time node should be awake
    SEEL_Assert::assert(added, SEEL_ASSERT_FILE_NUM_SNODE, __LINE__);
}

void SEEL_SNode::SEEL_Task_SNode_Receive::run()
{
    SEEL_Message msg;
    int8_t msg_rssi;
    uint32_t receive_offset;
    // Check if there is a message available
    if(!_inst->rfm_receive_msg(&msg, msg_rssi, receive_offset)) // Stores the returned msg in msg
    {
        // No message is available
        bool added = _inst->_ref_scheduler->add_task(&_inst->_task_receive);
        SEEL_Assert::assert(added, SEEL_ASSERT_FILE_NUM_SNODE, __LINE__);
        return;
    }

    // Message is available

    /* There are three types of received available msgs:
        1) Bcast message, check this for information and time sync
        2) Acknowledgement messages, can pop most recent message from the msg queue
        3) Messages intended for another node, forward these
    */

    // Prioritize bcast check over everything else
    // Possible to receive bcast msgs from multiple nodes; action depends on parent selection mode
    // Only respond to bcast msgs while own bcast msg has not been sent yet
    if(msg.cmd == SEEL_CMD_BCAST && !_inst->_bcast_sent)
    {
        // "_acked" is only false here if the node never slept last cycle. Used to check if we never slept and received another bcast (missed a cycle)
        // acked may get set to false when node receives multiple bcasts in the same cycle (from diff nodes due to the blacklist system),
        // thus use the bcast's SEEL_MSG_DATA_BCAST_COUNT field to differentiate between different cycle bcasts
        uint8_t bcast_count = msg.data[SEEL_MSG_DATA_BCAST_COUNT_INDEX];
        if(!_inst->_acked && _inst->_cb_info.bcast_count != bcast_count)
        {
            // If no bcast was received, clear the blacklist to try previously blacklisted nodes
            SEEL_Print::println(F("Blacklist clear")); // Blacklist: clear
            _inst->_bcast_blacklist.clear();
        }
        if(!_inst->_parent_sync)
        {
            _inst->_acked = false;
        }
        _inst->_cb_info.bcast_count = bcast_count;

        // Check to make sure sender is not in the broadcast blacklist. A blacklist is needed because
        // node sends (typically GNODE to SNODE) is sometimes asymmetrical in transmission distance.
        // So the sender could reach this node, but this node may not reach the sender. Using a blacklist
        // ensures previous parents that could not be reached are not tried again. The blacklist is cleared
        // if no one was able to reach this node; thus, a parent is not permanently blocked if it's
        // the only possible solution
        if(_inst->_bcast_blacklist.find(msg.send_id) == NULL)
        {
            // Check if this bcast node should become new parent
            uint32_t incoming_hop_count = msg.data[SEEL_MSG_DATA_HOP_COUNT_INDEX] + 1;
            bool new_parent = false;

            // PSEL == FIRST_BROADCAST will only take the first parent
            if(SEEL_PSEL_MODE == SEEL_PSEL_FIRST_BROADCAST)
            {
                if(!_inst->_parent_sync)
                {
                    _inst->_parent_id = msg.send_id;
                    _inst->_path_rssi = msg_rssi;
                    _inst->_cb_info.hop_count = incoming_hop_count;
                    new_parent = true;
                }
            } 
            else // Other modes will replace the current parent for (heuristically) better parents, within an interval (SEEL_PSEL_DURATION_MILLIS)
            {
                int8_t rssi_mode_value = 0;
                if(SEEL_PSEL_MODE == SEEL_PSEL_IMMEDIATE_RSSI)
                {
                    rssi_mode_value = msg_rssi;
                } 
                else if(SEEL_PSEL_MODE == SEEL_PSEL_PATH_RSSI)
                {
                    int8_t incoming_rssi = msg.data[SEEL_MSG_DATA_RSSI_INDEX];
                    rssi_mode_value = min(msg_rssi, incoming_rssi);
                }

                // A new parent is considered better if it has a higher RSSI metric than the
                // previous parent, with ties broken by hop count
                if(!_inst->_parent_sync ||
                    (rssi_mode_value > _inst->_path_rssi) || 
                    (rssi_mode_value == _inst->_path_rssi && incoming_hop_count < _inst->_cb_info.hop_count))
                {
                    _inst->_parent_id = msg.send_id;
                    _inst->_cb_info.hop_count = incoming_hop_count;
                    _inst->_path_rssi = rssi_mode_value;
                    new_parent = true;
                }
            }

            if(new_parent)
            {
                _inst->_acked = false;
                _inst->_bcast_msg = msg;
                _inst->_bcast_avail = true;
                _inst->_cb_info.parent_rssi = _inst->_path_rssi;
                SEEL_Print::print(F("Parent: ")); // Parent
                SEEL_Print::print(_inst->_parent_id);
                SEEL_Print::print(F(", RSSI metric: ")); // RSSI
                SEEL_Print::print(_inst->_path_rssi);
                SEEL_Print::print(F(", Hop Count: "));
                SEEL_Print::println(_inst->_cb_info.hop_count); // Hop Count
            }

            // Only do the following tasks on the first parent connected
            if(!_inst->_parent_sync)
            {
                _inst->_cb_info.missed_bcasts = _inst->_missed_bcasts;
                _inst->_missed_bcasts = 0;

                // Save the previous here since sleep time may get changed in bcast_setup
                uint32_t prev_sleep_time_secs = _inst->_snode_sleep_time_secs;
                
                // bcast_setup may have already run from a blacklisted node, so check to
                // make sure it only runs once
                if(!_inst->_bcast_received)
                {
                    // bcast_setup returns true if the bcast is the first bcast from a GNODE (network restarted)
                    _inst->bcast_setup(msg, receive_offset);
                }
                
                SEEL_Print::print(F("WTB: ")); SEEL_Print::println(_inst->_cb_info.wtb_millis);

                // Adjusting sleep-time, only adjust if we already have wtb data
                // Dont adjust sleep if previous bcast was missed, since we do not know how long we actually slept for;
                // there is no bcast reference to measure against
                // Make sure we have the same parent, otherwise WTB could be confounded by TDMA bcast delay
                if(_inst->_system_sync && _inst->_cb_info.missed_bcasts == 0 && _inst->_last_parent == _inst->_parent_id)
                {
                    SEEL_Assert::assert((uint64_t)(_inst->_snode_awake_time_secs + _inst->_snode_sleep_time_secs) * (uint64_t)SEEL_SECS_TO_MILLIS <= UINT32_MAX, SEEL_ASSERT_FILE_NUM_SNODE, __LINE__);
                    uint32_t cycle_time_millis = (_inst->_snode_awake_time_secs + _inst->_snode_sleep_time_secs) * SEEL_SECS_TO_MILLIS;
                    uint32_t prev_sleep_counts = 0;
                    uint32_t prev_sleep_time_millis = prev_sleep_time_secs * SEEL_SECS_TO_MILLIS;
                    SEEL_Assert::assert(prev_sleep_time_millis >= SEEL_ADJUSTED_SLEEP_EARLY_WAKE_MILLIS, SEEL_ASSERT_FILE_NUM_SNODE, __LINE__);
                    if (prev_sleep_time_millis > (SEEL_ADJUSTED_SLEEP_EARLY_WAKE_MILLIS + _inst->_sleep_time_offset_millis))
                    {
                        prev_sleep_counts = (prev_sleep_time_millis - SEEL_ADJUSTED_SLEEP_EARLY_WAKE_MILLIS - 
                            _inst->_sleep_time_offset_millis) / _inst->_sleep_time_estimate_millis;
                    }
                    
                    // Trims WTB in case a bcast was missed. Later check if missed bcast was due to sleeping too long
                    uint32_t wtb_trimmed_millis = _inst->_cb_info.wtb_millis % cycle_time_millis;
                    uint32_t actual_sleep_time_millis = prev_sleep_time_millis - wtb_trimmed_millis;
                    
                    // SNODE woke up too late if trimmed WTB is longer than specified sleep time
                    // Does not activate if SNODE is sleeping early
                    if(wtb_trimmed_millis > prev_sleep_time_millis)
                    {
                        // Adjust the sleep time offset to prevent oversleeping again
                        SEEL_Assert::assert(cycle_time_millis >= wtb_trimmed_millis, SEEL_ASSERT_FILE_NUM_SNODE, __LINE__);
                        // Check we are not going negative with offset
                        _inst->_sleep_time_offset_millis = min(cycle_time_millis - wtb_trimmed_millis, (prev_sleep_time_millis - SEEL_ADJUSTED_SLEEP_EARLY_WAKE_MILLIS));
                        actual_sleep_time_millis = prev_sleep_time_millis + _inst->_sleep_time_offset_millis;
                        SEEL_Print::print(F("New sleep offset: ")); SEEL_Print::println(_inst->_sleep_time_offset_millis);
                    }
                    else if(_inst->_sleep_time_offset_millis > 0 && wtb_trimmed_millis > _inst->_sleep_time_offset_millis)
                    {
                        _inst->_sleep_time_offset_millis = 0;
                        SEEL_Print::print(F("New sleep offset: ")); SEEL_Print::println(_inst->_sleep_time_offset_millis);
                    }

                    _inst->_sleep_time_estimate_millis = (prev_sleep_counts > 0) ? 
                        actual_sleep_time_millis / prev_sleep_counts : _inst->_sleep_time_estimate_millis;
                    SEEL_Print::print(F("Watchdog estimate: ")); SEEL_Print::println(_inst->_sleep_time_estimate_millis);
                    _inst->_WD_adjusted = true;
                }
                else if(!_inst->_system_sync)// First configuration or GNODE was refreshed
                {
                    // Keep the previous estimate of WD duration
                    _inst->_sleep_time_offset_millis = 0;
                }

                bool prev_system_sync = _inst->_system_sync;
                _inst->_parent_sync = true;
                _inst->_system_sync = true; // resets on system restart
                
                // Check if bcast msg just verified
                // Ignore bcast msg if this was the first bcast msg received; any node ID msgs in bcast msg is not
                // meant for this node
                if(!_inst->_id_verified)
                {
                    _inst->_id_verified = (prev_system_sync && _inst->bcast_id_check(&msg));
                }

                // If the Parent Selection mode is FIRST_BROADCAST then no broadcast collection delay is needed.
                bool added = _inst->_ref_scheduler->add_task(&_inst->_task_enqueue_msg, (SEEL_PSEL_MODE == SEEL_PSEL_FIRST_BROADCAST) ? 0 : SEEL_PSEL_DURATION_MILLIS);
                SEEL_Assert::assert(added, SEEL_ASSERT_FILE_NUM_SNODE, __LINE__);
                added = _inst->_ref_scheduler->add_task(&_inst->_task_send); // Only start sending messages when broadcast is received and processed
                SEEL_Assert::assert(added, SEEL_ASSERT_FILE_NUM_SNODE, __LINE__);
            }
        }
        else if(!_inst->_bcast_received) // Received bcast from blacklist node, but can still take time sync and sleep info
        {
            SEEL_Print::println(F("Blacklisted Node Bcast"));
            // Logic described above with the other call to bcast_setup
            _inst->bcast_setup(msg, receive_offset);
        }
    } // End Bcast Msg block
    else if(msg.cmd == SEEL_CMD_ACK && _inst->_unack_msgs > 0) // Only ack if ack is needed, acks have no target
    {
        // Check if ACK involves this node
        bool found = false;
        for (uint32_t i = 0; i < SEEL_MSG_DATA_SIZE && !found; ++i)
        {
            found = (msg.data[i] == _inst->_node_id);
        }

        if(found)
        {
            // Ack msg, decrement _data_queue
            _inst->_data_queue.pop_front();
            _inst->_msg_send_delay = 0;
            _inst->_unack_msgs = 0;
            _inst->_acked = true; // Gets set to true until cycle ends. This is to see if the parent ever ack'd messages. If not, add parent to blacklist.

            SEEL_Print::println(F("ACK received"));
        }
    }
    else if(msg.targ_id == _inst->_node_id && (msg.cmd == SEEL_CMD_DATA || msg.cmd == SEEL_CMD_ID_CHECK)) // Other msg intended for this node must be from a child, forward msg
    {
        // Node cannot be the recipient of another node
        // Continue to forward msg
        if(_inst->enqueue_forwarding_msg(&msg))
        {
            // Only acknowledge the msg if msg was added to the send queue (failure results if send queue is full)
            _inst->enqueue_ack(&msg);
        }
    }
    else if(msg.targ_id == _inst->_node_id)// Illegal msg
    {
        // Should never be here
        SEEL_Print::print(F("Error - Illegal Message")); // Error-Message
        _inst->print_msg(&msg);
        SEEL_Print::println(F(""));
        SEEL_Assert::assert(false, SEEL_ASSERT_FILE_NUM_SNODE, __LINE__);
    }
    else
    {
        SEEL_Print::println(F("Ignored message")); // Ignore this message
    }

    bool added = _inst->_ref_scheduler->add_task(&_inst->_task_receive);
    SEEL_Assert::assert(added, SEEL_ASSERT_FILE_NUM_SNODE, __LINE__);
}

void SEEL_SNode::SEEL_Task_SNode_Enqueue::run()
{
    if(_inst->_bcast_sent)
    {
        if(_inst->_id_verified)
        {
            // Enable scheduling user tasks
            _inst->_ref_scheduler->set_user_task_enable(true);
            bool added = _inst->_ref_scheduler->add_task(&_inst->_task_user);
            SEEL_Assert::assert(added, SEEL_ASSERT_FILE_NUM_SNODE, __LINE__);
        }
        else // Otherwise enqueue node id verification msg
        {
            _inst->enqueue_node_id();
        }
    }
    else
    {
        bool added = _inst->_ref_scheduler->add_task(&_inst->_task_enqueue_msg);
        SEEL_Assert::assert(added, SEEL_ASSERT_FILE_NUM_SNODE, __LINE__);
    }
}


void SEEL_SNode::SEEL_Task_SNode_User::run()
{
    // Make sure we receive bcast_msg before enqueuing user msgs
    if(_inst->_bcast_received)
    {
        _inst->enqueue_data();
    }

    bool added = _inst->_ref_scheduler->add_task(&_inst->_task_user);
    SEEL_Assert::assert(added, SEEL_ASSERT_FILE_NUM_SNODE, __LINE__);
}

void SEEL_SNode::SEEL_Task_SNode_Sleep::run()
{
    // Store any info messages
    _inst->_cb_info.prev_data_transmissions = _inst->_data_msgs_sent;
    _inst->_cb_info.prev_CRC_fails = _inst->_CRC_fails;
    _inst->_last_parent = _inst->_parent_id;

    // A parent was selected and a (ack-needed) msg was sent to parent, but parent never responded back
    if(_inst->_parent_sync && !_inst->_acked && _inst->_data_msgs_sent > 0) 
    {
        // Blacklist the parent
        SEEL_Print::print(F("Blacklisted NODE: ")); SEEL_Print::println(_inst->_parent_id); // Blacklist: parent ID
        _inst->_bcast_blacklist.add(_inst->_parent_id, true);
        _inst->_acked = true;
    }

    // Clear any remaining tasks and prepare for wakeup
    _inst->_ref_scheduler->clear_tasks();

    // Add in wakeup now
    bool added = _inst->_ref_scheduler->add_task(&_inst->_task_wake);
    SEEL_Assert::assert(added, SEEL_ASSERT_FILE_NUM_SNODE, __LINE__);

    // Call LoRa sleep
    _inst->_LoRaPHY_ptr->sleep();

    // Call user sleep, order matters because code flow blocks in sleep()
    _inst->sleep();
}

void SEEL_SNode::SEEL_Task_SNode_Force_Sleep::run()
{
    // If bcast received, then no need to force sleep
    if(_inst->_bcast_received)
    {
        return;
    }

    SEEL_Print::println(F("Force Sleep, clearing blacklist"));
    ++_inst->_missed_bcasts;
    _inst->_bcast_blacklist.clear();
    // Force sleep is necessary, run regular sleep function
    _inst->_ref_scheduler->clear_tasks(); // Guarentee sleep to be next task
    bool added = _inst->_ref_scheduler->add_task(&_inst->_task_sleep);
    SEEL_Assert::assert(added, SEEL_ASSERT_FILE_NUM_SNODE, __LINE__);
}

bool SEEL_SNode::bcast_id_check(SEEL_Message* msg)
{
    // Check if Gnode has okay'd this node's ID
    // Gnode (in the data section) of the msg has ID pairs
    // Data Format: [Time (4 Bytes):Sleep Count (2 Bytes):Awake Time Seconds (2 Bytes):
    //              ..ID in question (1 Byte), ID suggested (1 Byte); ID in question (1 Byte),ect.....]
    // The suggested ID is the ID assigned by the Gnode
    // If the suggested ID is zero, that means an error occured (for example two new ID's are the same)
    // If an error occured, the node should re-generate an ID
    
    // Since ID's are paired, must have even number of slots (truncate odd number of slots)
    // ID_FEEDBACK has two parts: [System Allocated Bytes] & [User Allocated Bytes], thus use two loops to handle each byte allocations
    // First value in pair is intended recipient
    // Second value in pair is suggested_id

    uint32_t suggested_id;
    bool found = false;
    for (uint32_t i = 0; (i + 1) < SEEL_MSG_DATA_ID_FEEDBACK_DEFAULT_SIZE && !found; i += 2) // Make sure both slots exist
    {
        if(msg->data[SEEL_MSG_DATA_ID_FEEDBACK_INDEX + i] == _node_id)
        {
            suggested_id = msg->data[SEEL_MSG_DATA_ID_FEEDBACK_INDEX + i + 1];
            found = true;
        }
    }

    for (uint32_t i = 0; (i + 1) < SEEL_MSG_USER_SIZE && !found; i += 2) // Make sure both slots exist
    {
        if((SEEL_MSG_DATA_USER_INDEX + i + 1) >= sizeof(msg->data)) // msg->data contains bytes, so sizeof by itself works
        {
            // Safety check, error can occur when snode and gnode are not updated at the same time and
            // snode has more user slots than gnode. Leads to array access in unallocated memory.
            SEEL_Print::println(F("Error - Slot Mismatch")); // Error - Slot mismatch
            SEEL_Assert::assert(false, SEEL_ASSERT_FILE_NUM_SNODE, __LINE__);
            break;
        }

        if(msg->data[SEEL_MSG_DATA_USER_INDEX + i] == _node_id)
        {
            suggested_id = msg->data[SEEL_MSG_DATA_USER_INDEX + i + 1];
            found = true;
        }
    }

    if(found)
    {
        // Gnode has msg for this Snode
        if(suggested_id == SEEL_GNODE_ID)
        {
            // Error, regenerate _node_id
            _node_id = generate_id();
        }
        else
        {
            // Take Gnode's suggestion, which may the original ID
            _node_id = suggested_id;
            _id_verified = true;
        }
    }

    return _id_verified;
}


bool SEEL_SNode::enqueue_forwarding_msg(SEEL_Message* prev_msg)
{
    bool added = false;
    // We know ID/Data msgs ultimately end up at the Gnode, so we can use relative sender and target id's
    // to faciliate acknowledgements and message travel. To send original sender ID, use data section 
    uint32_t original_target = prev_msg->targ_id;
    uint32_t original_sender = prev_msg->send_id;
    prev_msg->targ_id = _parent_id;
    prev_msg->send_id = _node_id;
    bool forward_msg = true;
    if(prev_msg->cmd == SEEL_CMD_DATA && _user_cb_forwarding != NULL)
    {
        forward_msg = _user_cb_forwarding(prev_msg->data, &_cb_info); // May modify "prev_msg->data"
    }

    if (forward_msg)
    {
        added = _data_queue.add(*prev_msg);
    }

    prev_msg->targ_id = original_target;
    prev_msg->send_id = original_sender;
    return added;
}

bool SEEL_SNode::enqueue_node_id()
{
    SEEL_Message msg;
    uint8_t msg_data[SEEL_MSG_DATA_SIZE];
    memset(msg_data, 0, sizeof(msg_data[0]) * SEEL_MSG_DATA_SIZE);
    
    // Protocol: When verifying node id's, the node id is placed
    // in the first element of the data slot so the Gnode should check this slot for ID addition
    msg_data[SEEL_MSG_DATA_ID_CHECK_INDEX] = _node_id;
    msg_data[SEEL_MSG_DATA_ID_ENCRYPT_INDEX] = (uint8_t) (_unique_key >> 24);
    msg_data[SEEL_MSG_DATA_ID_ENCRYPT_INDEX + 1] = (uint8_t) (_unique_key >> 16);
    msg_data[SEEL_MSG_DATA_ID_ENCRYPT_INDEX + 2] = (uint8_t) (_unique_key >> 8);
    msg_data[SEEL_MSG_DATA_ID_ENCRYPT_INDEX + 3] = (uint8_t) (_unique_key);
    
    create_msg(&msg, _parent_id, SEEL_CMD_ID_CHECK, msg_data);
    return _data_queue.add(msg);
}

bool SEEL_SNode::enqueue_data()
{
    SEEL_Message msg;
    uint8_t msg_data[SEEL_MSG_DATA_SIZE];
    
    // _id_verified:
    // Only send data messages when the node's ID is verified by GNODE
    // _user_cb_load:
    // Calls user callback function for loading data
    // User function MODIFIES msg_data
    // Returns (true/false) whether the message should be sent out
    if(_id_verified)
    {
        _cb_info.data_queue_size = _data_queue.size();
        bool enqueue_user_message = false;
        if (_user_cb_load != NULL)
        {
            enqueue_user_message = _user_cb_load(msg_data, &_cb_info);
        }
        
        _cb_info.first_callback = false;

        if(enqueue_user_message)
        {
            create_msg(&msg, _parent_id, SEEL_CMD_DATA, msg_data);
            return _data_queue.add(msg);
        }
    }

    return false; // No message to be added
}

void SEEL_SNode::sleep()
{
    // Puts Arduino into low power state
    // Uses watchdog timer for wakeup checks, SLEEP_8S is the longest
    // period the watchdog time can sleep for
    uint32_t sleep_counts = 0;
    uint32_t snode_sleep_time_millis = _snode_sleep_time_secs * SEEL_SECS_TO_MILLIS;
    if(snode_sleep_time_millis > (SEEL_ADJUSTED_SLEEP_EARLY_WAKE_MILLIS + _sleep_time_offset_millis))
    {
        SEEL_Assert::assert(snode_sleep_time_millis >= (SEEL_ADJUSTED_SLEEP_EARLY_WAKE_MILLIS + _sleep_time_offset_millis), SEEL_ASSERT_FILE_NUM_SNODE, __LINE__);
        sleep_counts = (snode_sleep_time_millis
            - SEEL_ADJUSTED_SLEEP_EARLY_WAKE_MILLIS
            - _sleep_time_offset_millis) / _sleep_time_estimate_millis;
    }

    if(_missed_bcasts > 0)
    {
        // Make signed int since awake duration could be smaller than specified, then sleep longer
        SEEL_Assert::assert((((uint64_t)SEEL_FORCE_SLEEP_AWAKE_MULT * 
            pow(SEEL_FORCE_SLEEP_AWAKE_DURATION_SCALE, _missed_bcasts) - 1) * 
            _snode_awake_time_secs * SEEL_SECS_TO_MILLIS) <= INT32_MAX, SEEL_ASSERT_FILE_NUM_SNODE, __LINE__);
        int32_t extra_awake_time_millis = (SEEL_FORCE_SLEEP_AWAKE_MULT * 
            pow(SEEL_FORCE_SLEEP_AWAKE_DURATION_SCALE, _missed_bcasts) - 1) * 
            _snode_awake_time_secs * SEEL_SECS_TO_MILLIS;
        // Sleep needs to be calculated a different way since SNODE stayed awake longer than specified
        if(snode_sleep_time_millis > (extra_awake_time_millis +
            SEEL_ADJUSTED_SLEEP_EARLY_WAKE_MILLIS + _sleep_time_offset_millis))
        {
            SEEL_Assert::assert(snode_sleep_time_millis >= (extra_awake_time_millis + SEEL_ADJUSTED_SLEEP_EARLY_WAKE_MILLIS + _sleep_time_offset_millis), SEEL_ASSERT_FILE_NUM_SNODE, __LINE__);
            sleep_counts = (snode_sleep_time_millis - (extra_awake_time_millis + 
                SEEL_ADJUSTED_SLEEP_EARLY_WAKE_MILLIS + _sleep_time_offset_millis)) / 
                _sleep_time_estimate_millis;
        }
        else
        {
            sleep_counts = 0;
        }
    }

    SEEL_Print::print(F("Sleeping for ")); SEEL_Print::print(sleep_counts); SEEL_Print::println(F(" counts"));
    SEEL_Print::flush();
    for (uint32_t i = 0; i < sleep_counts; ++i)
    {
        LowPower.powerDown(SEEL_WD_TIMER_DUR, ADC_OFF, BOD_OFF);
    }
}