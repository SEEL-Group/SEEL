/*
The SEEL repository can be found at: https://github.com/SEEL-Group/SEEL
Copyright (C) SEEL Group 2021 all rights reserved
See license file in root folder for more licensing details
See SEEL_documentation.pdf for protocol description details

File purpose:   Base class for GNODE and SNODE
*/

#ifndef SEEL_Node_h
#define SEEL_Node_h

// LoRa library by Sandeep Mistry. https://github.com/sandeepmistry/arduino-LoRa.
#include <LoRa.h>

#include "SEEL_Defines.h"
#include "SEEL_Scheduler.h"

class SEEL_Node
{
public:
    // Structs & Classes

    // Contains information that helps debugging the network after deployment.
    // Combining everything into struct allows for easy variable sizes to be passed
    // through functions.
    struct SEEL_CB_Info
    {
        uint32_t wtb_millis; // Time between waking up and this NODE receiving the broadcast message
        uint8_t prev_data_transmissions; // Number of data/id_check/fwd tranmissions in the PREVIOUS cycle
        uint8_t hop_count;
        uint8_t missed_bcasts;
        uint8_t data_queue_size;
        bool first_callback; // Whether this callback call is the first one this cycle (allows for initialization)

        SEEL_CB_Info() : wtb_millis(0), prev_data_transmissions(0), hop_count(0), missed_bcasts(0), 
        data_queue_size(0), first_callback(false) {}
    };

    // ***************************************************
    // Member functions

    // Getters & Setters
    uint32_t get_node_id() {return _node_id;}
    uint32_t get_parent_id() {return _parent_id;}
    uint8_t get_seq_num() {return _seq_num;}

    // MODIFIES: "msg"
    // Note: seq_num is left out since it is incremented in the send method
    void create_msg(SEEL_Message* msg, const uint8_t targ_id, const uint8_t send_id, const uint8_t cmd);
    void create_msg(SEEL_Message* msg, const uint8_t targ_id, const uint8_t send_id, const uint8_t cmd, uint8_t const * data);

    // Converts raw msg buffer from RFM95 to SEEL msg format
    void buf_to_SEEL_msg(SEEL_Message* msg, uint8_t const * buf);
    // ***************************************************
    // Destructor
    virtual ~SEEL_Node() {_LoRaPHY_ptr->end();}
protected:
    // Tasks
    class SEEL_Task_Node : public SEEL_Task
    {
    public:
        void set_inst(SEEL_Node* inst) {_inst = inst; _user_task = false;}
    protected:
        SEEL_Task_Node() {} // Prevents class instantiation
        SEEL_Node* _inst;

        friend SEEL_Node;
    };
	
	// Periodically broadcasts tree-formation signal to initiate SEEL cycle
    class SEEL_Task_Node_Send : public SEEL_Task_Node {virtual void run();};
    SEEL_Task_Node_Send _task_send;

    // ***************************************************
    // Member functions

    // Constructor
    SEEL_Node() {} // Having a protected constructor prevents instantiation of class

    void init(uint32_t n_id, uint32_t ts);

    void rfm_param_init(uint8_t cs_pin, uint8_t reset_pin, uint8_t int_pin, uint8_t TX_power, uint8_t coding_rate);

    // Returns if message was sent out
    bool rfm_send_msg(SEEL_Message* rfm_send_msg, uint8_t seq_num);

    bool rfm_receive_msg(SEEL_Message* rec_msg, int8_t& rssi);

    void print_msg(SEEL_Message* msg);

    void enqueue_ack(SEEL_Message* prev_msg);

    bool try_send(SEEL_Message* to_send_ptr, bool seq_inc);

    // ***************************************************
    // Member variables
    SEEL_Scheduler* _ref_scheduler;
    LoRaClass* _LoRaPHY_ptr; // Transceiver library pointer

    SEEL_Queue<SEEL_Message> _data_queue; // includes ID_CHECK and FWD msgs
    SEEL_Queue<uint8_t> _ack_queue;
    SEEL_Message _bcast_msg;
    SEEL_CB_Info _cb_info;

    uint32_t _last_msg_sent_time; // Exponential Backoff (EB), how long ago last msg was sent
    uint32_t _msg_send_delay; // EB, how long to delay until next transmission attempt
    uint32_t _unack_msgs; // EB, number of unacked msgs so far, reset to 0 on msg ack
    uint32_t _data_msgs_sent; // includes ID_CHECK and FWD msgs, tracks how many data msgs we sent
    uint32_t _tranmission_ToA; // estimate on ToA based on last measured transmission. Should be consistent since transmission parameters are consistent
    uint8_t _node_id;
    uint8_t _parent_id;
    uint8_t _tdma_slot; // TDMA
    uint8_t _seq_num; // Note: Will overflow after 255, but overflow does not affect functionality since seq_num serves to differentiate msgs
    int8_t _path_rssi; // changes based on parent selection mode
    bool _id_verified;
    bool _bcast_avail; // bcast msg is ready to be sent out
    bool _bcast_sent; // bcast msg has been sent out this cycle

private:
    // Structs & Classes
    struct SEEL_Dup_Msg
    {
        uint8_t send_id;
        uint8_t seq_num;
        uint8_t cmd;
        bool active;

        SEEL_Dup_Msg() : active(false) {}
    };

    // ***************************************************
    // Member functions

    // Returns true if msg is a duplicate msg (should be ignored)
    bool dup_msg_check(SEEL_Message* msg);

    // ***************************************************
    // Member variables
    SEEL_Dup_Msg _dup_msgs[SEEL_DUP_MSG_SIZE];
    uint8_t _oldest_dup_index = 0;
};

#endif // SEEL_Node_h