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
    // Structs, Classes & Enums
    
    // Contains flags for logging per-cycle information
    enum SEEL_Flags {
        FLAG_DUP_MSG = 0,
        FLAG_ADD_MAX_DATA_QUEUE = 1,
        FLAG_UNREC_MSG = 2,
        FLAG_ASSERT_FIRED = 3
    };

    // Extended Packet Structs
    class SEEL_Received_Broadcast
    {
    public:
        uint8_t sender_id; // 1st bit (MSB) denotes if the incoming bcast was received after this node already sent a bcast, remaining bit for node ID
        int8_t sender_rssi;

        SEEL_Received_Broadcast() : sender_id(0), sender_rssi(0) {}
        SEEL_Received_Broadcast(uint8_t id, int8_t rssi) : sender_id(id), sender_rssi(rssi) {}

        bool operator== (const SEEL_Received_Broadcast &rhs)
        {
            return sender_id == rhs.sender_id;
        }
    };
    class SEEL_Received_Message // Non-broadcast
    {
    public:
        uint8_t sender_id;
        int8_t sender_rssi; // latest sender
        uint8_t sender_misc;  // 1st bit (MSB) denotes if sender was child, remaining bits is send count

        SEEL_Received_Message() : sender_id(0), sender_rssi(0), sender_misc(0) {}
        SEEL_Received_Message(uint8_t id, int8_t rssi, uint8_t misc) : sender_id(id), sender_rssi(rssi), sender_misc(misc) {}

        bool operator== (const SEEL_Received_Message &rhs)
        {
            return sender_id == rhs.sender_id;
        }
    };
    class SEEL_Transmissions
    {
    public:
        uint8_t bcast;
        uint8_t data;
        uint8_t id_check;
        uint8_t ack;
        uint8_t fwd;
        
        SEEL_Transmissions()
        {
            clear();
        }
        
        void clear()
        {
            bcast = 0;
            data = 0;
            id_check = 0;
            ack = 0;
            fwd = 0;
        }
    };

    // Contains information that helps debugging the network after deployment.
    // Combining everything into struct allows for easy variable sizes to be passed
    // through functions.
    struct SEEL_CB_Info
    {
        // Extended Packet Variables
        SEEL_Extended_Packet_Queue<SEEL_Received_Broadcast> received_bcasts;
        SEEL_Extended_Packet_Queue<SEEL_Received_Message> prev_received_msgs;
        SEEL_Transmissions prev_transmissions;
        uint8_t prev_queue_dropped_msgs_self;
        uint8_t prev_queue_dropped_msgs_others;
        uint8_t prev_failed_transmissions;
        
        uint32_t wtb_millis; // Time between waking up and this NODE receiving the broadcast message
        uint8_t prev_CRC_fails;
        uint8_t hop_count;
        uint8_t missed_bcasts;
        uint8_t missed_msgs;
        uint8_t prev_max_data_queue_size; // prev cycle max data queue size during a cycle 
        uint8_t bcast_count;
        uint8_t prev_flags; // prev cycle logging flags; see above enum for more information
        int8_t parent_rssi; // RSSI value of the bcast msg received from the parent, initialized to 0
        bool first_callback; // Whether this callback call is the first one this cycle (allows for initialization)

        SEEL_CB_Info() : wtb_millis(0), prev_CRC_fails(0), hop_count(0), missed_bcasts(0), 
        missed_msgs(0), bcast_count(0), prev_flags(0), first_callback(false) {}
    };

    // ***************************************************
    // Typedefs
    typedef void (*user_callback_presend_t)(uint8_t msg_data[SEEL_MSG_DATA_SIZE], const SEEL_CB_Info* info);

    // ***************************************************
    // Member functions

    // Getters & Setters
    uint32_t get_node_id() {return _node_id;}
    uint32_t get_parent_id() {return _parent_id;}
    uint8_t get_seq_num() {return _seq_num;}

    // MODIFIES: "msg"
    // Note: seq_num is left out since it is incremented in the send method
    void create_msg(SEEL_Message* msg, const uint8_t targ_id, const uint8_t cmd);
    void create_msg(SEEL_Message* msg, const uint8_t targ_id, const uint8_t cmd, uint8_t const * data);

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

    bool rfm_receive_msg(SEEL_Message* rec_msg, int8_t& rssi, uint32_t& method_time);

    void print_msg(SEEL_Message* msg);

    void enqueue_ack(SEEL_Message* prev_msg);

    bool try_send(SEEL_Message* to_send_ptr, bool seq_inc);

    void set_flag(SEEL_Flags flag);

    void clear_flags();

    // ***************************************************
    // Member variables
    SEEL_Scheduler* _ref_scheduler;
    LoRaClass* _LoRaPHY_ptr; // Transceiver library pointer
    user_callback_presend_t _user_cb_presend;

    SEEL_Default_Queue<uint8_t> _ack_queue;
    SEEL_Queue<SEEL_Message>* _data_queue_ptr; // includes ID_CHECK and FWD msgs
    SEEL_Message _bcast_msg;
    SEEL_CB_Info _cb_info;

    uint32_t _last_msg_sent_time; // Exponential Backoff (EB), how long ago last msg was sent
    uint32_t _msg_send_delay; // EB, how long to delay until next transmission attempt
    uint32_t _unack_msgs; // EB, number of unacked msgs so far, reset to 0 on msg ack
    uint32_t _tranmission_ToA; // estimate on ToA based on last measured transmission. Should be consistent since transmission parameters are consistent
    uint8_t _node_id;
    uint8_t _parent_id;
    uint8_t _tdma_slot; // TDMA
    uint8_t _seq_num; // Note: Will overflow after 255, but overflow does not affect functionality since seq_num serves to differentiate msgs
    uint8_t _CRC_fails;
    uint8_t _max_data_queue_size;
    uint8_t _queue_dropped_msgs_self;
    uint8_t _queue_dropped_msgs_others;
    uint8_t _failed_transmissions;
    uint8_t _flags;
    int8_t _path_rssi; // changes based on parent selection mode
    bool _id_verified;
    bool _bcast_avail; // bcast msg is ready to be sent out
    bool _bcast_sent; // bcast msg has been sent out this cycle
    
    // Extended Packet Variables
    SEEL_Extended_Packet_Queue<SEEL_Received_Message> _received_msgs;
    SEEL_Transmissions _cycle_transmissions;

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