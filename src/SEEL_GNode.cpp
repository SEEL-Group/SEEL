/*
The SEEL repository can be found at: https://github.com/SEEL-Group/SEEL
Copyright (C) SEEL Group 2021 all rights reserved
See license file in root folder for more licensing details
See SEEL_documentation.pdf for protocol description details

File purpose:   See SEEL_GNode.h
*/

#include "SEEL_GNode.h"

void SEEL_GNode::init(  SEEL_Scheduler* ref_scheduler, 
            user_callback_broadcast_t user_cb_broadcast, user_callback_data_t user_cb_data, 
            uint8_t cs_pin, uint8_t reset_pin, uint8_t int_pin, 
            uint32_t cycle_period_secs, uint32_t snode_awake_time_secs, 
            uint32_t tdma_slot)
{
    SEEL_Node::init(SEEL_GNODE_ID, tdma_slot);
    rfm_param_init(cs_pin, reset_pin, int_pin, SEEL_RFM95_GNODE_TX, SEEL_RFM95_GNODE_CR);

    // Initialize member variables
    _ref_scheduler = ref_scheduler;
    _user_cb_broadcast = user_cb_broadcast;
    _user_cb_data = user_cb_data;
    _snode_awake_time_secs = snode_awake_time_secs;
    _snode_sleep_time_secs = cycle_period_secs - snode_awake_time_secs;
    _cycle_period_secs = cycle_period_secs;
    _bcast_count = 0;
    _cb_info.hop_count = 0;
    _path_rssi = 0;
    _first_bcast = true;

    // Initialize tasks with this inst
    _task_bcast.set_inst(this);
    _task_receive.set_inst(this);

    _ref_scheduler->add_task(&_task_bcast);
    _ref_scheduler->add_task(&_task_receive);
    _ref_scheduler->add_task(&_task_send); // inst set in SEEL_Node.cpp
}

void SEEL_GNode::print_bcast_queue()
{
    SEEL_Print::print(F("Bcast queue: ["));
    for (uint32_t i = 0; i < _pending_bcast_ids.size(); ++i)
    {
        SEEL_Print::print(_pending_bcast_ids.front()->id);
        SEEL_Print::print(F("->"));
        SEEL_Print::print(_pending_bcast_ids.front()->response);
        SEEL_Print::print(F(" "));
        _pending_bcast_ids.recycle_front();
    }
    SEEL_Print::println(F("]"));
}

bool SEEL_GNode::id_avail(uint32_t msg_id)
{
    bool id_free = true;

    if (_id_container[msg_id].used)
    {
        // ID taken, check if expired
        uint32_t missed_counts = (uint8_t)((_bcast_count & 0x7F) - (_id_container[msg_id].saved_bcast_count & 0x7F)) & 0x7F;
        if (missed_counts < SEEL_MAX_CYCLE_MISSES)
        {
            id_free = false;
        }
    }

    return id_free;
}

void SEEL_GNode::id_check(uint32_t msg_id, uint32_t unique_key)
{
    uint32_t id_response = SEEL_ID_CHECK_ERROR;

    // Create temp response and check send queue to make sure ID does not already
    // exist. If it does, that means two nodes selected the same ID, which then we should
    // send a response of 0 which means both nodes re-select their ID's
    SEEL_ID_BCAST id_info(msg_id, id_response, unique_key);

    // Check if there's already an ID bcast with the same ID in the pending queue
    SEEL_ID_BCAST* found_ID = _pending_bcast_ids.find(id_info);
    if (found_ID) // Found
    {
        // Check encrypt codes
        // If they have the same encrypt code, high chances that this msg is from the same node, so ignore this request
        // If different encrypt codes, assume multiple nodes have generated the same ID and flag to all nodes involved
        if (found_ID->unique_key != id_info.unique_key)
        {
            found_ID->response = SEEL_ID_CHECK_ERROR;
            _id_container[found_ID->id].used = false; // Was set to true earlier, need to correct
        }
        return;
    }

    bool found;
    if (!id_avail(msg_id))
    {
        // Re-assign ID to available, counting from the back since users are more likely
        // to assign low ID values (prevents confusion)
        found = false;
        uint32_t id_assign_start = min(pow(2, SEEL_MSG_TARG_SIZE * 8), SEEL_MAX_NODES) - 1;
        for (uint32_t i = id_assign_start; i > SEEL_GNODE_ID && !found; --i)
        {
            if (id_avail(i))
            {
                found = true;

                id_info.response = i;

                _id_container[i].used = true;
                _id_container[i].saved_bcast_count = (_bcast_count & 0x7F);
            }
        }

        // If no avail ID was found, then we add error msg to the send queue
    }
    else // ID available
    {
        // Okay the same ID
        id_info.response = msg_id;
        _id_container[msg_id].used = true;
        _id_container[msg_id].saved_bcast_count = (_bcast_count & 0x7F);
    }

    // Add response message to send queue
    _pending_bcast_ids.add(id_info);
}

void SEEL_GNode::SEEL_Task_GNode_Receive::run()
{
    SEEL_Message msg;
    int8_t msg_rssi;
    uint32_t receive_offset;
    // Check if there is a message available
    if (!_inst->rfm_receive_msg(&msg, msg_rssi, receive_offset))
    {
        // No message is available
        _inst->_ref_scheduler->add_task(&_inst->_task_receive);
        return;
    }

    if (msg.targ_id != SEEL_GNODE_ID)
    {
        // Message not intended for GNode
        _inst->_ref_scheduler->add_task(&_inst->_task_receive);
        return;
    }

    // Check message type:
    // 1. Data msg - Give to user receive function to handle
    // 2. ID check - Check ID database for ID acceptability
    if (msg.cmd == SEEL_CMD_DATA)
    {
        // Re-affirm node is still in network by updating its saved_bcast_count
        // Set container status to active in case gateway had to restart. so we don't lose comms with already-initialized
        // nodes. However, possible security risk of intruder nodes
        _inst->_id_container[msg.send_id].used = true;
        _inst->_id_container[msg.send_id].saved_bcast_count = (_inst->_bcast_count & 0x7F);
        // Provide msg to user callback
        _inst->_user_cb_data(msg.data, msg_rssi);
        _inst->enqueue_ack(&msg);
    }
    else if (msg.cmd == SEEL_CMD_ID_CHECK)
    {
        // Protocol: When verifying node id's, the node id is placed
        // in the first element of the data slot so the Gnode should check this slot for ID addition
        uint32_t unique_key = 0;
        unique_key += (uint32_t)msg.data[SEEL_MSG_DATA_ID_ENCRYPT_INDEX] << 24;
        unique_key += (uint32_t)msg.data[SEEL_MSG_DATA_ID_ENCRYPT_INDEX + 1] << 16;
        unique_key += (uint32_t)msg.data[SEEL_MSG_DATA_ID_ENCRYPT_INDEX + 2] << 8;
        unique_key += (uint32_t)msg.data[SEEL_MSG_DATA_ID_ENCRYPT_INDEX + 3];

        _inst->id_check(msg.data[SEEL_MSG_DATA_ID_CHECK_INDEX], unique_key);
        _inst->enqueue_ack(&msg);
    }

    _inst->_ref_scheduler->add_task(&_inst->_task_receive);
}

void SEEL_GNode::SEEL_Task_GNode_Bcast::run()
{
    // Don't use SEEL_Node's send system to bypass collision avoidance; BCAST on GNODE must be sent without delay
    SEEL_Message to_send;

    // Print out any pending ID's
    _inst->print_bcast_queue();

    // Clear ack queue
    _inst->_ack_queue.clear();

    // Check if there are any new ID's that need to be added to gateway signal
    for (uint32_t i = 0; i < SEEL_MSG_DATA_ID_FEEDBACK_TOTAL_SIZE; i += 2)
    {
        uint8_t id;
        uint8_t response;
        if (_inst->_pending_bcast_ids.empty())
        {
            // If no more items to add, fill with zeros
            id = 0;
            response = 0;
        }
        else
        {
            id = _inst->_pending_bcast_ids.front()->id;
            response = _inst->_pending_bcast_ids.front()->response;
            _inst->_pending_bcast_ids.pop_front();
        }
        
        if (i < SEEL_MSG_DATA_ID_FEEDBACK_DEFAULT_SIZE)
        {
            to_send.data[SEEL_MSG_DATA_ID_FEEDBACK_INDEX + i] = id;
            to_send.data[SEEL_MSG_DATA_ID_FEEDBACK_INDEX + i + 1] = response;
        }
        else // User space
        {
            to_send.data[SEEL_MSG_DATA_USER_INDEX + (i - SEEL_MSG_DATA_ID_FEEDBACK_DEFAULT_SIZE)] = id;
            to_send.data[SEEL_MSG_DATA_USER_INDEX + (i - SEEL_MSG_DATA_ID_FEEDBACK_DEFAULT_SIZE) + 1] = response;
        }
    }

    // Note whether this bcast is the first bcast for the network, used for initialization
    to_send.data[SEEL_MSG_DATA_FIRST_BCAST_INDEX] = _inst->_first_bcast ? SEEL_BCAST_FB : 0;

    // Update cycle information, information stored big endian
    to_send.data[SEEL_MSG_DATA_AWAKE_TIME_SECONDS_INDEX] = (uint8_t) (_inst->_snode_awake_time_secs >> 24);
    to_send.data[SEEL_MSG_DATA_AWAKE_TIME_SECONDS_INDEX + 1] = (uint8_t) (_inst->_snode_awake_time_secs >> 16);
    to_send.data[SEEL_MSG_DATA_AWAKE_TIME_SECONDS_INDEX + 2] = (uint8_t) (_inst->_snode_awake_time_secs >> 8);
    to_send.data[SEEL_MSG_DATA_AWAKE_TIME_SECONDS_INDEX + 3] = (uint8_t) (_inst->_snode_awake_time_secs);

    to_send.data[SEEL_MSG_DATA_SLEEP_TIME_SECONDS_INDEX] = (uint8_t) (_inst->_snode_sleep_time_secs >> 24);
    to_send.data[SEEL_MSG_DATA_SLEEP_TIME_SECONDS_INDEX + 1] = (uint8_t) (_inst->_snode_sleep_time_secs >> 16);
    to_send.data[SEEL_MSG_DATA_SLEEP_TIME_SECONDS_INDEX + 2] = (uint8_t) (_inst->_snode_sleep_time_secs >> 8);
    to_send.data[SEEL_MSG_DATA_SLEEP_TIME_SECONDS_INDEX + 3] = (uint8_t) (_inst->_snode_sleep_time_secs);

    // Parent selection info
    to_send.data[SEEL_MSG_DATA_HOP_COUNT_INDEX] = (uint8_t) (_inst->_cb_info.hop_count);
    to_send.data[SEEL_MSG_DATA_RSSI_INDEX] = (uint8_t) (0); // Filled out later by SNODEs

    // Downside to sys time of 0 is that system time keeps resetting so system cannot schedule tasks longer than cycle duration
    //_inst->_ref_scheduler->zero_millis_timer(); // Use a system time of 0 for better determinism and less TDMA collision chances
    uint32_t system_time = millis();
    system_time += _inst->_tranmission_ToA; // account for transmission delay beforehand
    to_send.data[SEEL_MSG_DATA_TIME_SYNC_INDEX] = (uint8_t) (system_time >> 24);
    to_send.data[SEEL_MSG_DATA_TIME_SYNC_INDEX + 1] = (uint8_t) (system_time >> 16);
    to_send.data[SEEL_MSG_DATA_TIME_SYNC_INDEX + 2] = (uint8_t) (system_time >> 8);
    to_send.data[SEEL_MSG_DATA_TIME_SYNC_INDEX + 3] = (uint8_t) (system_time);

    // Send out gateway msg
    _inst->create_msg(&to_send, SEEL_GNODE_ID, SEEL_GNODE_ID, SEEL_CMD_BCAST);
    _inst->rfm_send_msg(&to_send, _inst->_bcast_count);
    _inst->_user_cb_broadcast(to_send.data);

    ++_inst->_bcast_count;
    _inst->_first_bcast = false;

    // Re-add bcast task, with cycle delay
    _inst->_ref_scheduler->add_task(&_inst->_task_bcast, _inst->_cycle_period_secs * SEEL_SECS_TO_MILLIS);
}