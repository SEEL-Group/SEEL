/*
The SEEL repository can be found at: https://github.com/SEEL-Group/SEEL
Copyright (C) SEEL Group 2021 all rights reserved
See license file in root folder for more licensing details
See SEEL_documentation.pdf for protocol description details

File purpose:   See SEEL_Node.h
*/

#include "SEEL_Node.h"

void SEEL_Node::init(uint32_t n_id, uint32_t ts)
{
    _node_id = n_id;
    _tdma_slot = ts;
    if (_tdma_slot >= SEEL_TDMA_SLOTS && SEEL_TDMA_USE_TDMA)
    {
        SEEL_Print::println(F("Error - TDMA Slot Overflow")); // Error - TDMA SLOTS Overflow
        while(true);
    }

    _id_verified = true;
    _tranmission_ToA = SEEL_TRANSMISSION_UB_DUR_MILLIS; // Set to upperbound initially and adjust dynamically

    _task_send.set_inst(this);
}

void SEEL_Node::rfm_param_init(uint8_t cs_pin, uint8_t int_pin, uint8_t TX_power, uint8_t coding_rate)
{
    // Create and initialize RFM module
    _rf95_ptr = new RH_RF95(cs_pin, int_pin);
    _rf95_ptr->init() ? SEEL_Print::println(F("RF95 Init Success")) : SEEL_Print::println(F("RF95 Init Fail")); // Init - Success : Init - Fail

    // Set LoRa params, defined in 
    _rf95_ptr->setFrequency(SEEL_RFM95_FREQ);
    _rf95_ptr->setSpreadingFactor(SEEL_RFM95_SF);
    _rf95_ptr->setSignalBandwidth(SEEL_RFM95_BW);
    _rf95_ptr->setTxPower(TX_power);
    _rf95_ptr->setCodingRate4(coding_rate);
    SEEL_Print::println(F("Parameters:"));
    SEEL_Print::print(F("\tFq: ")); SEEL_Print::println(SEEL_RFM95_FREQ);
    SEEL_Print::print(F("\tSF: ")); SEEL_Print::println(SEEL_RFM95_SF);
    SEEL_Print::print(F("\tBW: ")); SEEL_Print::println(SEEL_RFM95_BW);
    SEEL_Print::print(F("\tTX: ")); SEEL_Print::println(TX_power);
    SEEL_Print::print(F("\tCR: ")); SEEL_Print::println(coding_rate);
    SEEL_Print::print(F("\tPayload Size (Does not include LoRa headers): ")); SEEL_Print::println(SEEL_MSG_TOTAL_SIZE);
}

void SEEL_Node::create_msg(SEEL_Message* msg, const uint8_t targ_id, 
    const uint8_t send_id, const uint8_t cmd)
{
    msg->targ_id = targ_id;
    msg->send_id = send_id;
    msg->cmd = cmd;
    msg->seq_num = _seq_num;
}

void SEEL_Node::create_msg(SEEL_Message* msg, const uint8_t targ_id, 
    const uint8_t send_id, const uint8_t cmd, uint8_t const * data)
{
    create_msg(msg, targ_id, send_id, cmd);
    memcpy(msg->data, data, SEEL_MSG_DATA_SIZE*sizeof(*data));
}

void SEEL_Node::buf_to_SEEL_msg(SEEL_Message* msg, uint8_t const * buf)
{
    memcpy(&msg->targ_id, buf+SEEL_MSG_TARG_INDEX, SEEL_MSG_TARG_SIZE*sizeof(*buf));
    memcpy(&msg->send_id, buf+SEEL_MSG_SEND_INDEX, SEEL_MSG_SEND_SIZE*sizeof(*buf));
    memcpy(&msg->cmd, buf+SEEL_MSG_CMD_INDEX, SEEL_MSG_CMD_SIZE*sizeof(*buf));
    memcpy(&msg->seq_num, buf+SEEL_MSG_SEQ_INDEX, SEEL_MSG_SEQ_SIZE*sizeof(*buf));
    memcpy(msg->data, buf+SEEL_MSG_MISC_INDEX, SEEL_MSG_DATA_SIZE*sizeof(*buf));
}

bool SEEL_Node::rfm_send_msg(SEEL_Message* msg, uint8_t seq_num, uint16_t timeout)
{
    msg->seq_num = seq_num;

    _rf95_ptr->send((uint8_t *)msg, SEEL_MSG_TOTAL_SIZE);
    uint32_t send_time_start = millis();
    if (timeout == 0)
    {
        _rf95_ptr->waitPacketSent();
    }
    else
    {
        // Returns false if timed out
        if (!_rf95_ptr->waitPacketSent(timeout))
        {
            return false; // Message not sent out
        }
    }

    // ToA should be consistent among transmissions since packet size and LoRa parameters are fixed
    _tranmission_ToA = millis() - send_time_start;
    SEEL_Print::print(F("<<S: "));
    print_msg(msg);
    SEEL_Print::print(F(", Start Time: "));
    SEEL_Print::print(send_time_start);
    SEEL_Print::print(F(", ToA: ")); // Send duration (ToA, time in TX state) estimate
    SEEL_Print::println(_tranmission_ToA);
    SEEL_Print::flush();

    return true; // Message sent out
}

bool SEEL_Node::dup_msg_check(SEEL_Message* msg)
{
    // Check if there are any matches in dup array
    // The most important fields in the message are send_id and seq_num
    // using those two fields allows differentiation among unique messages
    // the active field is used initially in case we receive a message from 
    // id 0 that has a seq num of 0
    bool message_duplicate = false;
    for (uint32_t i = 0; i < SEEL_DUP_MSG_SIZE && !message_duplicate; ++i)
    {
        if (_dup_msgs[i].active &&
            _dup_msgs[i].seq_num == msg->seq_num &&
            _dup_msgs[i].send_id == msg->send_id &&
            _dup_msgs[i].cmd == msg->cmd)
        {
            message_duplicate = true;
        }
    }

    // Check if necessary to store this message into dup array
    if (!message_duplicate && SEEL_DUP_MSG_SIZE > 0)
    {
        // If this message is not a duplicate then replace the 
        // oldest element in the duplicate array with this message
        _dup_msgs[_oldest_dup_index].active = true;
        _dup_msgs[_oldest_dup_index].send_id = msg->send_id;
        _dup_msgs[_oldest_dup_index].seq_num = msg->seq_num;
        _dup_msgs[_oldest_dup_index].cmd = msg->cmd;

        ++_oldest_dup_index;
        if (_oldest_dup_index >= SEEL_DUP_MSG_SIZE)
        {
            _oldest_dup_index -= SEEL_DUP_MSG_SIZE;
        }
    }
    return message_duplicate;
}

bool SEEL_Node::rfm_receive_msg(SEEL_Message* msg, int8_t& rssi)
{
    bool valid_msg = false;

    // Message is available
    if (_rf95_ptr->available())
    { 
        uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
        uint8_t buf_len = sizeof(buf);

        // Load message into buffer
        if (_rf95_ptr->recv(buf, &buf_len))
        {
            SEEL_Print::print(F(">>R: "));
            for (uint32_t i = 0; i < buf_len; ++i)
            {
                SEEL_Print::print(buf[i]); SEEL_Print::print(F(" "));
            }
            rssi = _rf95_ptr->lastRssi();
            SEEL_Print::print(F("RSSI: ")); SEEL_Print::println(rssi);

            if (buf_len == SEEL_MSG_TOTAL_SIZE) // Potential SEEL message
            {
                // Converts raw msg buffer to SEEL_Message
                buf_to_SEEL_msg(msg, buf);

                // Check if the message has already been seen, to prevent a loop
                if (!dup_msg_check(msg))
                {
                    valid_msg = true;
                }
                else
                {
                    SEEL_Print::println(F("Duplicate message")); 
                }
            }
            SEEL_Print::flush();
        }
    }

    // No valid msg was received
    return valid_msg;
}

void SEEL_Node::enqueue_ack(SEEL_Message* prev_msg)
{
    // ACK messages have no target. Instead, the node IDs that have been ack'd are written in
    // the data section of the message. Which is filled out before sending.
    if (_ack_queue.find(prev_msg->send_id) == NULL)
    {
        _ack_queue.add(prev_msg->send_id);
    }
}

void SEEL_Node::print_msg(SEEL_Message* msg)
{
    for (uint32_t i = 0; i < SEEL_MSG_TOTAL_SIZE; ++i)
    {
        SEEL_Print::print(((uint8_t*)msg)[i]); SEEL_Print::print(F(" "));
    }
}

bool SEEL_Node::try_send(SEEL_Message* to_send_ptr, bool seq_inc)
{
    uint8_t seq_num = to_send_ptr->seq_num;
    // Increment sequence numbers for messages sent by this node
    // Do not modify for bcast msg
    if (seq_inc)
    {
        seq_num = _seq_num;
        ++_seq_num;
    }

    // Not waiting for ack or wait has timed out
    // If timed out, re-send the same message because it has not been popped
    if (rfm_send_msg(to_send_ptr, seq_num, SEEL_SEND_TIMEOUT_MILLIS))
    {
        // Code reaches here if msg was sent out
        
        if (!SEEL_TDMA_USE_TDMA)
        {
            _last_msg_sent_time = millis();
            _msg_send_delay = random(SEEL_EB_MIN_MILLIS, SEEL_EB_INIT_MILLIS * pow(SEEL_EB_EXP_SCALE, _unack_msgs));
        }
        return true;
    }

    // Message failed to send
    return false;
}

void SEEL_Node::SEEL_Task_Node_Send::run()
{	
    bool can_send = true;
    uint32_t time_millis = millis();

    // Select which collision avoidance strategy to use
    if (SEEL_TDMA_USE_TDMA)
    {
        uint32_t current_slot = (time_millis % SEEL_TDMA_CYCLE_TIME_MILLIS) / SEEL_TDMA_SLOT_WAIT_MILLIS;

        // Compare with buffer because NODE should not send message if the msg is expected to finish after the slot
        can_send = (current_slot == _inst->_tdma_slot) && ((time_millis % SEEL_TDMA_SLOT_WAIT_MILLIS) < SEEL_TDMA_BUFFER_MILLIS);
    }
    else // Exponential Backoff
    {
        can_send = (time_millis - _inst->_last_msg_sent_time) > _inst->_msg_send_delay;
    }

    // If cannot send or nothing to send, return
    if (!can_send || (!_inst->_bcast_avail && _inst->_ack_queue.empty() && _inst->_data_queue.empty()))
    {
        _inst->_ref_scheduler->add_task(&_inst->_task_send);
        return;
    }
    // If the code reaches here, a message can be sent out
    SEEL_Message to_send;
    SEEL_Message* to_send_ptr = &to_send;

    // Prioritize bcast msgs, then ack msgs, then data/id_check msgs
    // Note messages in _data_queue may be from previous cycles
    // Send only one bcast msg per cycle to avoid pollution
    if (_inst->_bcast_avail && !_inst->_bcast_sent)
    {
        to_send_ptr = &_inst->_bcast_msg;

        to_send_ptr->send_id = _inst->_node_id;

        to_send_ptr->data[SEEL_MSG_DATA_HOP_COUNT_INDEX] = _inst->_cb_info.hop_count;
        to_send_ptr->data[SEEL_MSG_DATA_RSSI_INDEX] = _inst->_path_rssi;

        // Update time info right before the send
        uint32_t time_millis = millis();
        time_millis += _inst->_tranmission_ToA; // account for transmission delay beforehand
        to_send_ptr->data[SEEL_MSG_DATA_TIME_SYNC_INDEX] = (uint8_t) (time_millis >> 24);
        to_send_ptr->data[SEEL_MSG_DATA_TIME_SYNC_INDEX + 1] = (uint8_t) (time_millis >> 16);
        to_send_ptr->data[SEEL_MSG_DATA_TIME_SYNC_INDEX + 2] = (uint8_t) (time_millis >> 8);
        to_send_ptr->data[SEEL_MSG_DATA_TIME_SYNC_INDEX + 3] = (uint8_t) (time_millis);

        if (_inst->try_send(to_send_ptr, false))
        {
            _inst->_bcast_avail = false;
            _inst->_bcast_sent = true;
        }
    }
    else if (!_inst->_ack_queue.empty())
    {
        memset(to_send_ptr->data, 0, sizeof(to_send_ptr->data[0]) * SEEL_MSG_DATA_SIZE);
        // Fill message with as many pending ACK's as possible
        for (uint32_t i = 0; i < SEEL_MSG_DATA_SIZE && !_inst->_ack_queue.empty(); ++i)
        {
            to_send_ptr->data[i] = *_inst->_ack_queue.front();
            _inst->_ack_queue.pop_front();
        }
        _inst->create_msg(to_send_ptr, SEEL_GNODE_ID, _inst->_node_id, SEEL_CMD_ACK);

        _inst->try_send(to_send_ptr, true);
    }
    else if (!_inst->_data_queue.empty())// DATA or ID_CHECK or FORWARDED message
    {
        to_send_ptr = _inst->_data_queue.front();
        uint32_t msg_cmd = to_send_ptr->cmd;

        // A verified node may still have an join requests in the message queue
        // If this node is already verified, then do not send and pop the join request from send queue
        if (msg_cmd == SEEL_CMD_ID_CHECK && _inst->_id_verified)
        {
            _inst->_data_queue.pop_front();
            _inst->_ref_scheduler->add_task(&_inst->_task_send);
            return;
        }

        // Any ID_CHECK or DATA msgs need to be sent to this node's parent at THIS cycle,
        // but queue'd messages might have different parents. So correct the parent at SEND time.
        // Same with self ID, node may take a suggested ID, so sender should also be corrected
        to_send_ptr->send_id = _inst->_node_id;
        to_send_ptr->targ_id = _inst->_parent_id;

        if (_inst->try_send(to_send_ptr, true))
        {
            ++(_inst->_unack_msgs);
            ++(_inst->_data_msgs_sent);
        }
        // Do not pop msg from queue until msg is ack'd
    }

    _inst->_ref_scheduler->add_task(&_inst->_task_send);
}