/*
The SEEL repository can be found at: https://github.com/SEEL-Group/SEEL
Copyright (C) SEEL Group 2021 all rights reserved
See license file in root folder for more licensing details
See SEEL_documentation.pdf for protocol description details

File purpose:   SEEL GNODE, serves as the sink NODE in the network. Connects network messages to application layer.
                Initiates cycles in the network.
*/

#ifndef SEEL_GNode_h
#define SEEL_GNode_h

#include "SEEL_Node.h"

class SEEL_GNode : public SEEL_Node
{
public:
    // Typedefs
    typedef void (*user_callback_broadcast_t) (uint8_t msg_data[SEEL_MSG_DATA_SIZE]);
    typedef void (*user_callback_data_t) (const uint8_t msg_data[SEEL_MSG_DATA_SIZE], const int8_t msg_rssi);

    // ***************************************************
    // Member functions

    // Getter & Setters
    void set_snode_awake_time(uint32_t new_awake_time) {_snode_awake_time_secs = new_awake_time;};
    void set_snode_sleep_time(uint32_t new_sleep_time) {_snode_sleep_time_secs = new_sleep_time;};

    void init(  SEEL_Scheduler* ref_scheduler, 
                user_callback_broadcast_t user_cb_broadcast, user_callback_data_t user_cb_data, 
                uint8_t cs_pin, uint8_t reset_pin, uint8_t int_pin, 
                uint32_t cycle_period_millis, uint32_t snode_awake_time_secs,
                uint32_t tdma_slot);

private:
    // Structs & Classes

    // Packages information for tracking ID activity
    struct SEEL_ID_INFO
    {
        // Bools take up 1 byte on Arduino, so use bit fields to be more space efficient
        uint8_t saved_bcast_count : 7;
        uint8_t used : 1;

        SEEL_ID_INFO() : saved_bcast_count(0), used(false) {}
    };

    // Packages information for _pending_bcast_ids
    struct SEEL_ID_BCAST
    {
        uint32_t unique_key;
        uint8_t id;
        uint8_t response;

        SEEL_ID_BCAST(uint8_t t_id = 0, uint8_t t_response = 0, uint32_t t_unique_key = 0)
        {
            id = t_id;
            response = t_response;
            unique_key = t_unique_key;
        }

        SEEL_ID_BCAST& operator=(const SEEL_ID_BCAST& t)
        {
            id = t.id;
            response = t.response;
            unique_key = t.unique_key;
            return *this;
        }

        bool operator== (const SEEL_ID_BCAST& t) const
        {
            // Only look for similar ID's
            return (id == t.id);
        }
    };

    // ***************************************************
    // Tasks
    class SEEL_Task_GNode : public SEEL_Task
    {
    public:
        void set_inst(SEEL_GNode* inst) {_inst = inst; _user_task = false;}
    protected:
        SEEL_Task_GNode() {} // Prevents class instantiation
        SEEL_GNode* _inst;

        friend SEEL_GNode;
    };

    // Checks for LoRa messages. Messages can be either be data messages or ID checks.
    // Also acknowledges messages
    class SEEL_Task_GNode_Receive : public SEEL_Task_GNode {virtual void run();};
    SEEL_Task_GNode_Receive _task_receive;

    // Periodically broadcasts tree-formation signal to initiate SEEL cycle
    class SEEL_Task_GNode_Bcast : public SEEL_Task_GNode {virtual void run();};
    SEEL_Task_GNode_Bcast _task_bcast;

    // ***************************************************
    // Member functions

    // Prints bcast queue
    void print_bcast_queue();

    // Helper function to check if an id is available for use (empty or timed out)
    bool id_avail(uint32_t msg_id);

    void id_check(uint32_t msg_id, uint32_t unique_key);

    // ***************************************************
    // Member variables
    SEEL_ID_INFO _id_container[SEEL_MAX_NODES];
    SEEL_Queue<SEEL_ID_BCAST> _pending_bcast_ids;
    user_callback_broadcast_t _user_cb_broadcast;
    user_callback_data_t _user_cb_data;
    uint32_t _cycle_period_secs;
    uint32_t _snode_awake_time_secs;
    uint32_t _snode_sleep_time_secs;
    uint8_t _bcast_count;
    bool _first_bcast;
};

#endif // SEEL_GNode_h