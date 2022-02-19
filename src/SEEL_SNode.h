/*
The SEEL repository can be found at: https://github.com/SEEL-Group/SEEL
Copyright (C) SEEL Group 2021 all rights reserved
See license file in root folder for more licensing details
See SEEL_documentation.pdf for protocol description details

File purpose:   SEEL SNODE, serves as source and forwarding NODE in the network
*/

#ifndef SEEL_SNode_h
#define SEEL_SNode_h

#include <LowPower.h> // From https://github.com/rocketscream/Low-Power

#include "SEEL_Node.h"

class SEEL_SNode: public SEEL_Node
{
public:
    // Typedefs
    typedef bool (*user_callback_load_t)(uint8_t msg_data[SEEL_MSG_DATA_SIZE], const SEEL_CB_Info* info);
    typedef void (*user_callback_forwarding_t)(uint8_t msg_data[SEEL_MSG_DATA_SIZE], const SEEL_CB_Info* info);
    // user_callback_presend_t defined in SEEL_Node.h
    
    // ***************************************************
    // Member functions
    void init(  SEEL_Scheduler* ref_scheduler,
                user_callback_load_t user_cb_load, 
                user_callback_presend_t user_cb_presend,
                user_callback_forwarding_t user_cb_forwarding,
                uint8_t cs_pin, uint8_t reset_pin, uint8_t int_pin, 
                uint32_t snode_id, uint32_t tdma_slot);
    
    void blacklist_add(uint8_t node_id)
    {
        _bcast_blacklist.add(node_id);
    }
private:
    // Tasks
    class SEEL_Task_SNode : public SEEL_Task
    {
    public:
        void set_inst(SEEL_SNode* inst) {_inst = inst; _user_task = false;}
    protected:
        SEEL_Task_SNode() {} // Prevents class instantiation
        SEEL_SNode* _inst;

        friend SEEL_SNode;
    };

    // First task to run after waking up. Resets variables.
    class SEEL_Task_SNode_Wake : public SEEL_Task_SNode {virtual void run();};
    SEEL_Task_SNode_Wake _task_wake;

    // Checks for LoRa messages and handles appropriately. Receives broadcast.
    class SEEL_Task_SNode_Receive : public SEEL_Task_SNode {virtual void run();};
    SEEL_Task_SNode_Receive _task_receive;
	
    // Ends waiting for broadcasts (join phase), transitions into data phase
    class SEEL_Task_SNode_Enqueue : public SEEL_Task_SNode {virtual void run();};
    SEEL_Task_SNode_Enqueue _task_enqueue_msg;

	// Checks for user callback function for data messages to enqueue.
    class SEEL_Task_SNode_User : public SEEL_Task_SNode {virtual void run();};
    SEEL_Task_SNode_User _task_user;

    // Sends Arduino into low-power sleep mode for specified duration.
    // Duration should be multiple of Arduino Watchdog's capabilities. Default 8 seconds.
    // Non-multiples will be divided by watchdog_time: floor(sleep_duration / watchdog_timer)*watchdog_timer
    class SEEL_Task_SNode_Sleep : public SEEL_Task_SNode {virtual void run();};
    SEEL_Task_SNode_Sleep _task_sleep;

    class SEEL_Task_SNode_Force_Sleep : public SEEL_Task_SNode {virtual void run();};
    SEEL_Task_SNode_Force_Sleep _task_force_sleep;

    // ***************************************************
    // Member functions

    // Generates random ID, excluding 0 (reserved for gateway)
    uint32_t generate_id();

    void bcast_setup(SEEL_Message &msg, uint32_t receive_offset);

    bool bcast_id_check(SEEL_Message* msg);

    void sleep();

    // Enqueue messages: These are messages that can be sent with delay and need to be ack'd 
    // Returns true if the message was added to send queue
    bool enqueue_forwarding_msg(SEEL_Message* prev_msg);

    bool enqueue_node_id();

    bool enqueue_data();
    
    // ***************************************************
    // Member variables
    SEEL_Queue<uint8_t> _bcast_blacklist;
    user_callback_load_t _user_cb_load;
    user_callback_forwarding_t _user_cb_forwarding;
    uint32_t _snode_awake_time_secs; // How long node should be awake for, set with bcast
    uint32_t _snode_sleep_time_secs; // How long node should sleep for, set with bcast
    uint32_t _unique_key;
    uint32_t _sleep_time_estimate_millis; // Time estimate for single watch-dog sleep
    uint32_t _sleep_time_offset_millis; // Offset used for future sleeps when SNODE misses broadcast
    uint8_t _missed_bcasts;
    uint8_t _last_parent;
    bool _bcast_received; // Set to false on wake-up, set to true on first broadcast received
    bool _parent_sync;  // Set to false on wake-up, set to true on first non-blacklist broadcast received
    bool _system_sync; // Set to false on Snode start-up, set to true on first non-blacklist broadcast received
    bool _acked;
    bool _WD_adjusted; // Set to true after WD timer gets corrected (more accurate sleep times)
};

#endif // SEEL_SNode_h
