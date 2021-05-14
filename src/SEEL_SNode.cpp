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
            user_callback_forwarding_t user_cb_forwarding,
            uint8_t cs_pin, uint8_t int_pin, 
            uint32_t snode_id, uint32_t tdma_slot)
{
    SEEL_Node::init((snode_id == SEEL_GNODE_ID) ? generate_id() : snode_id, tdma_slot);
    rfm_param_init(cs_pin, int_pin, SEEL_RFM95_SNODE_TX, SEEL_RFM95_SNODE_CR);

    // Initialize member variables
    _ref_scheduler = ref_scheduler;
    _user_cb_load = user_cb_load;
    _user_cb_forwarding = user_cb_forwarding;
    _unique_key = random(0, UINT32_MAX);
    _snode_awake_time_secs = 0;
    _snode_sleep_time_secs = 0;
    _bcast_last_seqnum = 0;
    _sleep_time_estimate_millis = SEEL_ADJUSTED_SLEEP_INITAL_ESTIMATE_MILLIS;
    _sleep_time_offset_millis = 0;
    _id_verified = false;
    _system_sync = false;
    _acked = true;

    // Set task instances
    _task_wake.set_inst(this);
    _task_receive.set_inst(this);
    _task_enqueue_msg.set_inst(this);
    _task_user.set_inst(this);
    _task_sleep.set_inst(this);

    _ref_scheduler->add_task(&_task_wake);
}

void SEEL_SNode::SEEL_Task_SNode_Wake::run()
{
    SEEL_Print::println(F("Wake"));
    _inst->_cb_info.wtb_millis = millis();
    _inst->_msg_send_delay = 0;
    _inst->_unack_msgs = 0;
    _inst->_bcast_received = false;
    _inst->_parent_sync = false;
    _inst->_cb_info.first_callback = true; // Allows ability to only send 1 message per cycle
    _inst->_bcast_avail = false;
    _inst->_bcast_sent = false;

    _inst->_hop_count = UINT32_MAX;
    _inst->_path_rssi = INT8_MIN;

    // Clear ack queue
    _inst->_ack_queue.clear();

    _inst->_ref_scheduler->set_user_task_enable(false); // Disables user tasks from running until critical LoRa tasks are done

    // Add cycle tasks to scheduler
    _inst->_ref_scheduler->add_task(&_inst->_task_receive);
    // TODO: Add max time awake check, so SNODE can sleep even if it misses the bcast
}

void SEEL_SNode::bcast_setup(SEEL_Message &msg)
{
    // Calculate wakeup-to-bcast time before time shift
    _cb_info.wtb_millis = millis() - _cb_info.wtb_millis;

    // Update system with values from bcast, values stored in Big Endian (MSB in lower address)
    // Update local time
    uint32_t millis_update = 0;
    millis_update += (uint32_t)msg.data[SEEL_MSG_DATA_TIME_SYNC_INDEX] << 24;
    millis_update += (uint32_t)msg.data[SEEL_MSG_DATA_TIME_SYNC_INDEX + 1] << 16;
    millis_update += (uint32_t)msg.data[SEEL_MSG_DATA_TIME_SYNC_INDEX + 2] << 8;
    millis_update += (uint32_t)msg.data[SEEL_MSG_DATA_TIME_SYNC_INDEX + 3];
    uint32_t millis_old = millis();
    _ref_scheduler->set_millis(millis_update);

    // Bcast will be modified such that sender becomes this node
    _bcast_received = true; // resets every cycle

    // Correct times in Scheduler with updated times
    // Note: Large time jumps (a difference bigger than int32_max) may cause tasks to run earlier than scheduled
    _ref_scheduler->offset_task_times((int32_t)millis_update - (int32_t)millis_old);

    // Update awake time, stored time in seconds
    uint32_t awake_time_update = 0;
    awake_time_update += (uint32_t)msg.data[SEEL_MSG_DATA_AWAKE_TIME_SECONDS_INDEX] << 8;
    awake_time_update += (uint32_t)msg.data[SEEL_MSG_DATA_AWAKE_TIME_SECONDS_INDEX + 1];
    _snode_awake_time_secs = awake_time_update;

    uint32_t sleep_time_update = 0;
    sleep_time_update += (uint32_t)msg.data[SEEL_MSG_DATA_SLEEP_TIME_SECONDS_INDEX] << 8;
    sleep_time_update += (uint32_t)msg.data[SEEL_MSG_DATA_SLEEP_TIME_SECONDS_INDEX + 1];
    _snode_sleep_time_secs = sleep_time_update;

    // Add sleep ASAP to keep time consistent, should be done AFTER local and awake times are updated
    _ref_scheduler->add_task(&_task_sleep, _snode_awake_time_secs * SEEL_SECS_TO_MILLIS); // Delay sleep task by time node should be awake
}

void SEEL_SNode::SEEL_Task_SNode_Receive::run()
{
    SEEL_Message msg;
    int8_t msg_rssi;
    // Check if there is a message available
    if (!_inst->rfm_receive_msg(&msg, msg_rssi)) // Stores the returned msg in msg
    {
        // No message is available
        _inst->_ref_scheduler->add_task(&_inst->_task_receive);
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
    if (msg.cmd == SEEL_CMD_BCAST && !_inst->_bcast_sent)
    {
        // "_acked" is only false here if the node never slept last cycle. Used to check if we never slept and received another bcast (missed a cycle)
        // acked may get set to false when node receives multiple bcasts in the same cycle (from diff nodes due to the blacklist system),
        // thus use the bcasts seq_num to differentiate between different cycle bcasts
        if (!_inst->_acked && _inst->_bcast_last_seqnum != msg.seq_num)
        {
            // If no bcast was received, clear the blacklist to try previously blacklisted nodes
            SEEL_Print::println(F("Blacklist clear")); // Blacklist: clear
            _inst->_bcast_blacklist.clear();
        }
        _inst->_acked = false; 
        _inst->_bcast_last_seqnum = msg.seq_num;

        // Check to make sure sender is not in the broadcast blacklist. A blacklist is needed because
        // node sends (typically GNODE to SNODE) is sometimes asymmetrical in transmission distance.
        // So the sender could reach this node, but this node may not reach the sender. Using a blacklist
        // ensures previous parents that could not be reached are not tried again. The blacklist is cleared
        // if no one was able to reach this node; thus, a parent is not permanently blocked if it's
        // the only possible solution.

        if(_inst->_bcast_blacklist.find(msg.send_id) == NULL)
        {
            uint32_t incoming_hop_count = msg.data[SEEL_MSG_DATA_HOP_COUNT_INDEX] + 1;
            bool new_parent = false;
            if(SEEL_PSEL_MODE == SEEL_PSEL_FIRST_BROADCAST)
            {
                if(!_inst->_parent_sync)
                {
                    _inst->_parent_id = msg.send_id;
                    _inst->_path_rssi = msg_rssi;
                    _inst->_hop_count = incoming_hop_count;
                    new_parent = true;
                }
            } 
            else // Other modes will look for (heuristically) better parents within an interval
            {
                int8_t rssi_mode_value = 0;
                if(SEEL_PSEL_MODE == SEEL_PSEL_IMMEDIATE_RSSI)
                {
                    rssi_mode_value = msg_rssi;
                } 
                else if (SEEL_PSEL_MODE == SEEL_PSEL_PATH_RSSI) 
                {
                    int8_t incoming_rssi = msg.data[SEEL_MSG_DATA_RSSI_INDEX];
                    rssi_mode_value = min(msg_rssi, incoming_rssi);
                }

                // A parent is considered better if it has a lower hop count to the GNODE, 
                // with ties broken by RSSI (RSSI value used depends on the PSEL mode)
                if(incoming_hop_count < _inst->_hop_count || 
                    (_inst->_hop_count == incoming_hop_count && rssi_mode_value > _inst->_path_rssi))
                {
                    _inst->_parent_id = msg.send_id;
                    _inst->_hop_count = incoming_hop_count;
                    _inst->_path_rssi = rssi_mode_value;
                    new_parent = true;
                }
            }

            if (new_parent)
            {
                SEEL_Print::print(F("Parent: ")); // Parent
                SEEL_Print::print(_inst->_parent_id);
                SEEL_Print::print(F(", RSSI: ")); // RSSI
                SEEL_Print::print(_inst->_path_rssi);
                SEEL_Print::print(F(", Hop Count: "));
                SEEL_Print::println(_inst->_hop_count); // Hop Count
            }

            _inst->_bcast_msg = msg;
            _inst->_bcast_avail = true;

            if(!_inst->_parent_sync)
            {
                uint32_t prev_sleep_time_secs = _inst->_snode_sleep_time_secs;
                _inst->bcast_setup(msg); //efficiency runs twice on valid-first bcast
                
                SEEL_Print::print(F("WTB: ")); SEEL_Print::println(_inst->_cb_info.wtb_millis);

                //Adjusting sleep-time
                if(_inst->_system_sync == true)
                {
                    if (_inst->_snode_sleep_time_secs == prev_sleep_time_secs)
                    {
                        uint32_t cycle_time_millis = (_inst->_snode_awake_time_secs + _inst->_snode_sleep_time_secs) * SEEL_SECS_TO_MILLIS;
                        uint32_t prev_sleep_counts = ((prev_sleep_time_secs * SEEL_SECS_TO_MILLIS) - SEEL_ADJUSTED_SLEEP_EARLY_WAKE_MILLIS - _inst->_sleep_time_offset_millis) / _inst->_sleep_time_estimate_millis;
                        
                        // Trims WTB in case a bcast was missed. Later check if missed bcast was due to sleeping too long
                        uint32_t wtb_trimmed_millis = _inst->_cb_info.wtb_millis % cycle_time_millis;
                        
                        uint32_t actual_sleep_time_millis = ((prev_sleep_time_secs * SEEL_SECS_TO_MILLIS) - wtb_trimmed_millis);
                        
                        // SNODE woke up too late if trimmed WTB is longer than specified sleep time
                        if (wtb_trimmed_millis > prev_sleep_time_secs * SEEL_SECS_TO_MILLIS)
                        {
                            // Adjust the sleep time offset to prevent oversleeping again
                            _inst->_sleep_time_offset_millis = cycle_time_millis - wtb_trimmed_millis;
                            actual_sleep_time_millis = (prev_sleep_time_secs * SEEL_SECS_TO_MILLIS) + _inst->_sleep_time_offset_millis;
                            SEEL_Print::print(F("New sleep offset: ")); SEEL_Print::println(_inst->_sleep_time_offset_millis);
                        }
                        else if (_inst->_sleep_time_offset_millis > 0 && wtb_trimmed_millis > _inst->_sleep_time_offset_millis)
                        {
                            _inst->_sleep_time_offset_millis = 0;
                            SEEL_Print::print(F("New sleep offset: ")); SEEL_Print::println(_inst->_sleep_time_offset_millis);
                        }

                        _inst->_sleep_time_estimate_millis = actual_sleep_time_millis / prev_sleep_counts;
                        SEEL_Print::print(F("Watchdog estimate: ")); SEEL_Print::println(_inst->_sleep_time_estimate_millis);
                    }
                    else // Sleep time was just changed in this bcast
                    {
                        // Keep the previous estimate of WD duration
                        _inst->_sleep_time_offset_millis = 0;
                    }
                }

                bool prev_system_sync = _inst->_system_sync;
                _inst->_parent_sync = true;
                _inst->_system_sync = true; // resets on system restart
                
                // Check if bcast msg just verified
                // Ignore bcast msg if this was the first bcast msg received; any node ID msgs in bcast msg is not
                // meant for this node
                if(!_inst->_id_verified){_inst->_id_verified = (prev_system_sync && _inst->bcast_id_check(&msg));}

                // If the Parent Selection mode is FIRST_BROADCAST then no broadcast collection delay is needed.
                _inst->_ref_scheduler->add_task(&_inst->_task_enqueue_msg, (SEEL_PSEL_MODE == SEEL_PSEL_FIRST_BROADCAST) ? 0 : SEEL_PSEL_DURATION_MILLIS);
                _inst->_ref_scheduler->add_task(&_inst->_task_send); // Only start sending messages when broadcast is received and processed
            }
        }
        else if(!_inst->_bcast_received)
        {
            _inst->bcast_setup(msg);
        }

    } // end message is bcast
    else if (msg.cmd == SEEL_CMD_ACK && _inst->_unack_msgs > 0) // Only ack if ack is needed, acks have no target
    {
        // Check if ACK involves this node
        bool found = false;
        for (uint32_t i = 0; i < SEEL_MSG_DATA_SIZE && !found; ++i)
        {
            found = (msg.data[i] == _inst->_node_id);
        }

        if (found)
        {
            // Ack msg, decrement _data_queue
            _inst->_data_queue.pop_front();
            _inst->_msg_send_delay = 0;
            _inst->_unack_msgs = 0;
            _inst->_acked = true; // Gets set to true until cycle ends. This is to see if the parent ever ack'd messages. If not, add parent to blacklist.

            SEEL_Print::println(F("ACK received"));
        }
    }
    else if (msg.targ_id == _inst->_node_id && (msg.cmd == SEEL_CMD_DATA || msg.cmd == SEEL_CMD_ID_CHECK)) // Other msg intended for this node must be from a child, forward msg
    {
        // Node cannot be the recipient of another node
        // Continue to forward msg
        if (_inst->enqueue_forwarding_msg(&msg, msg_rssi))
        {
            // Only acknowledge the msg if msg was added to the send queue (failure results if send queue is full)
            _inst->enqueue_ack(&msg);
        }
    }
    else if (msg.targ_id == _inst->_node_id)// Illegal msg
    {
        // Should never be here
        SEEL_Print::print(F("Error - Illegal Message")); // Error-Message
        _inst->print_msg(&msg);
        SEEL_Print::println(F(""));
    }
    else
    {
        SEEL_Print::println(F("Ignored message")); // Ignore this message
    }

    _inst->_ref_scheduler->add_task(&_inst->_task_receive);
}

void SEEL_SNode::SEEL_Task_SNode_Enqueue::run()
{
    if(_inst->_bcast_sent)
    {
        if (_inst->_id_verified)
        {
            // Enable scheduling user tasks
            _inst->_ref_scheduler->set_user_task_enable(true);
            _inst->_ref_scheduler->add_task(&_inst->_task_user);
        }
        else // Otherwise enqueue node id verification msg
        {
            _inst->enqueue_node_id();
        }
    }
    else
    {
        _inst->_ref_scheduler->add_task(&_inst->_task_enqueue_msg); 
    }
}


void SEEL_SNode::SEEL_Task_SNode_User::run()
{
    // Make sure we receive bcast_msg before enqueuing user msgs
    if (_inst->_bcast_received)
    {
        _inst->enqueue_data();
	}
	
	_inst->_ref_scheduler->add_task(&_inst->_task_user);
}

void SEEL_SNode::SEEL_Task_SNode_Sleep::run()
{
    // Cannot clear blacklist here since this task is never reached if no bcast is received

    // Store any info messages
    _inst->_cb_info.prev_data_transmissions = _inst->_data_msgs_sent;

    // A parent was selected and a (ack-needed) msg was sent to parent, but parent never responded back
    if (!_inst->_acked && _inst->_parent_sync && _inst->_data_msgs_sent > 0) 
    {
        // Blacklist the parent
        SEEL_Print::print(F("Blacklisted NODE: ")); SEEL_Print::println(_inst->_parent_id); // Blacklist: parent ID
        _inst->_bcast_blacklist.add(_inst->_parent_id, true);
        _inst->_acked = true;
    }

    // Clear buffer and prepare for wakeup
    _inst->_ref_scheduler->clear_tasks();

    // Add in wakeup now
    _inst->_ref_scheduler->add_task(&_inst->_task_wake);

    // Call LoRa sleep
    _inst->_rf95_ptr->sleep();

    // Call user sleep, order matters because code flow blocks in sleep()
    _inst->sleep();
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
        if (msg->data[SEEL_MSG_DATA_ID_FEEDBACK_INDEX + i] == _node_id)
        {
            suggested_id = msg->data[SEEL_MSG_DATA_ID_FEEDBACK_INDEX + i + 1];
            found = true;
        }
    }

    for (uint32_t i = 0; (i + 1) < SEEL_MSG_USER_SIZE && !found; i += 2) // Make sure both slots exist
    {
        if ((SEEL_MSG_DATA_USER_INDEX + i + 1) >= sizeof(msg->data)) // msg->data contains bytes, so sizeof by itself works
        {
            // Safety check, error can occur when snode and gnode are not updated at the same time and
            // snode has more user slots than gnode. Leads to array access in unallocated memory.
            SEEL_Print::println(F("Error - Slot Mismatch")); // Error - Slot mismatch
            break;
        }

        if (msg->data[SEEL_MSG_DATA_USER_INDEX + i] == _node_id)
        {
            suggested_id = msg->data[SEEL_MSG_DATA_USER_INDEX + i + 1];
            found = true;
        }
    }

    if (found)
    {
        // Gnode has msg for this Snode
        if (suggested_id == SEEL_GNODE_ID)
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


bool SEEL_SNode::enqueue_forwarding_msg(SEEL_Message* prev_msg, int8_t msg_rssi)
{
    bool added = false;
    // We know ID/Data msgs ultimately end up at the Gnode, so we can use relative sender and target id's
    // to faciliate acknowledgements and message travel. To send original sender ID, use data section 
    uint32_t original_target = prev_msg->targ_id;
    uint32_t original_sender = prev_msg->send_id;
    prev_msg->targ_id = _parent_id; // Gets corrected later at msg SEND time
    prev_msg->send_id = _node_id;
    if (prev_msg->cmd == SEEL_CMD_DATA)
    {
        _user_cb_forwarding(prev_msg->data, msg_rssi); // May modify "prev_msg->data"
    }
    added = _data_queue.add(*prev_msg);

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
    
    create_msg(&msg, _parent_id, _node_id, SEEL_CMD_ID_CHECK, msg_data);
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
    if (_id_verified)
    {
        bool enqueue_user_message = _user_cb_load(msg_data, &_cb_info, _data_queue.size() >= _data_queue.max_size());
        _cb_info.first_callback = false;

        if (enqueue_user_message)
        {
            create_msg(&msg, _parent_id, _node_id, SEEL_CMD_DATA, msg_data);
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
    uint32_t sleep_counts = ((_snode_sleep_time_secs * SEEL_SECS_TO_MILLIS) - SEEL_ADJUSTED_SLEEP_EARLY_WAKE_MILLIS - _sleep_time_offset_millis) / _sleep_time_estimate_millis;

    SEEL_Print::print(F("Sleeping for ")); SEEL_Print::print(sleep_counts); SEEL_Print::println(F(" counts"));
    SEEL_Print::flush();
    for (uint32_t i = 0; i < sleep_counts; ++i)
    {
        LowPower.powerDown(SEEL_WD_TIMER_DUR, ADC_OFF, BOD_OFF);
    }
}