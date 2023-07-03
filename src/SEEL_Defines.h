/*
The SEEL repository can be found at: https://github.com/SEEL-Group/SEEL
Copyright (C) SEEL Group 2021 all rights reserved
See license file in root folder for more licensing details
See SEEL_documentation.pdf for protocol description details

File purpose:   Set up constants and structs used in the SEEL protocol
*/

#ifndef SEEL_Defines_h
#define SEEL_Defines_h

#include "SEEL_Params.h"
#include "SEEL_Queue.h"
#include "SEEL_Queue.cpp" // cpp file required for templates
#include "SEEL_Print.h"
#include "SEEL_Assert.h"

/* 
This file contains useful constants and structures used by the SEEL protocol
Values in this header file should NOT be modified; instead modify values in SEEL_Params.h
*/

/* MESSAGE COMMANDS */
const uint8_t SEEL_CMD_BCAST      = 0;
const uint8_t SEEL_CMD_ACK        = 1;
const uint8_t SEEL_CMD_DATA       = 2;
const uint8_t SEEL_CMD_ID_CHECK   = 3;

/* MESSAGE DESCRIPTION, SIZE in Bytes */
const uint8_t SEEL_MSG_TARG_INDEX   = 0;
const uint8_t SEEL_MSG_TARG_SIZE    = 1;
const uint8_t SEEL_MSG_SEND_INDEX   = 1;
const uint8_t SEEL_MSG_SEND_SIZE    = 1;
const uint8_t SEEL_MSG_CMD_INDEX    = 2;
const uint8_t SEEL_MSG_CMD_SIZE     = 1;
const uint8_t SEEL_MSG_SEQ_INDEX    = 3;
const uint8_t SEEL_MSG_SEQ_SIZE     = 1;
const uint8_t SEEL_MSG_OSEND_INDEX  = 4;
const uint8_t SEEL_MSG_OSEND_SIZE   = 1;
const uint8_t SEEL_MSG_MISC_INDEX   = 5;
// LED DEMO
const uint8_t SEEL_MSG_MISC_SIZE    = 19;
const uint8_t SEEL_MSG_USER_INDEX   = 24; // USER_SIZE defined in SEEL_Params.h
const uint8_t SEEL_MSG_DATA_SIZE = SEEL_MSG_MISC_SIZE + SEEL_MSG_USER_SIZE;
const uint8_t SEEL_MSG_TOTAL_SIZE = SEEL_MSG_TARG_SIZE + SEEL_MSG_SEND_SIZE + SEEL_MSG_CMD_SIZE
    + SEEL_MSG_SEQ_SIZE + SEEL_MSG_OSEND_SIZE + SEEL_MSG_MISC_SIZE + SEEL_MSG_USER_SIZE;

/* MESSAGE DATA DESCRIPTION, SIZE in Bytes */

// For CMD: DATA
// Filled by user

// For CMD: ACK
// Filled with acknowledgement targets

// For CMD: ID_CHECK
const uint8_t SEEL_MSG_DATA_ID_CHECK_INDEX = 0;
const uint8_t SEEL_MSG_DATA_ID_CHECK_SIZE = 1;
const uint8_t SEEL_MSG_DATA_ID_ENCRYPT_INDEX = 1;
const uint8_t SEEL_MSG_DATA_ID_ENCRYPT_SIZE = 4;

// For CMD: BCAST
const uint8_t SEEL_MSG_DATA_FIRST_BCAST_INDEX = 0;
const uint8_t SEEL_MSG_DATA_FIRST_BCAST_SIZE = 1;
const uint8_t SEEL_MSG_DATA_BCAST_COUNT_INDEX = 1;
const uint8_t SEEL_MSG_DATA_BCAST_COUNT_SIZE = 1;
const uint8_t SEEL_MSG_DATA_TIME_SYNC_INDEX = 2;
const uint8_t SEEL_MSG_DATA_TIME_SYNC_SIZE = 4;
const uint8_t SEEL_MSG_DATA_AWAKE_TIME_SECONDS_INDEX = 6;
const uint8_t SEEL_MSG_DATA_AWAKE_TIME_SECONDS_SIZE = 4;
const uint8_t SEEL_MSG_DATA_SLEEP_TIME_SECONDS_INDEX = 10;
const uint8_t SEEL_MSG_DATA_SLEEP_TIME_SECONDS_SIZE = 4;
const uint8_t SEEL_MSG_DATA_HOP_COUNT_INDEX = 14;
const uint8_t SEEL_MSG_DATA_HOP_COUNT_SIZE = 1;
const uint8_t SEEL_MSG_DATA_RSSI_INDEX = 15;
const uint8_t SEEL_MSG_DATA_RSSI_SIZE = 1;
// LED DEMO
const uint8_t SEEL_MSG_DATA_LED_PARENT_INDEX = 16;
const uint8_t SEEL_MSG_DATA_LED_PARENT_SIZE = 1;
const uint8_t SEEL_MSG_DATA_ID_FEEDBACK_INDEX = 17; // Variable size, default is two. Expand with user size
const uint8_t SEEL_MSG_DATA_ID_FEEDBACK_DEFAULT_SIZE = 2; // Actual size is 2*floor((SEEL_MSG_DATA_ID_FEEDBACK_DEFAULT_SIZE + SEEL_MSG_USER_SIZE) / 2.0)
const uint8_t SEEL_MSG_DATA_USER_INDEX = 19;  // USER_SIZE defined in SEEL_Params.h
const uint8_t SEEL_MSG_DATA_ID_FEEDBACK_TOTAL_SIZE = SEEL_MSG_DATA_ID_FEEDBACK_DEFAULT_SIZE + SEEL_MSG_USER_SIZE;

/* MSG Info and Signals */
const uint8_t SEEL_GNODE_ID = 0;
const uint8_t SEEL_ID_CHECK_ERROR = 0;
const uint8_t SEEL_BCAST_FB = 1; // Signals the system has restarted and that the bcast msg is the first one (for init purposes). Otherwise 0.

/* MISC */
const uint32_t SEEL_SECS_TO_MILLIS = 1000;

// SEEL message packed into a more readable form
struct SEEL_Message
{
    uint8_t targ_id;
    uint8_t send_id;
    uint8_t cmd;
    uint8_t seq_num;
    uint8_t orig_send_id;
    uint8_t data[SEEL_MSG_DATA_SIZE];

    SEEL_Message() {}

    // Copy Constructor
    SEEL_Message(const SEEL_Message& msg)
    {
        this->targ_id = msg.targ_id;
        this->send_id = msg.send_id;
        this->cmd = msg.cmd;
        this->seq_num = msg.seq_num;
        this->orig_send_id = msg.orig_send_id;
        memcpy(this->data, msg.data, SEEL_MSG_DATA_SIZE * sizeof(*data));
    }

    // Assignment overload
    SEEL_Message& operator=(const SEEL_Message& msg)
    {
        this->targ_id = msg.targ_id;
        this->send_id = msg.send_id;
        this->cmd = msg.cmd;
        this->seq_num = msg.seq_num;
        this->orig_send_id = msg.orig_send_id;
        memcpy(this->data, msg.data, SEEL_MSG_DATA_SIZE * sizeof(*data));

        return *this;
    }

    operator String () const {
        return (String)this->cmd;
    }

};

#endif // SEEL_Defines