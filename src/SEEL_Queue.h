/*
The SEEL repository can be found at: https://github.com/SEEL-Group/SEEL
Copyright (C) SEEL Group 2021 all rights reserved
See license file in root folder for more licensing details
See SEEL_documentation.pdf for protocol description details

File purpose:   Queue implementation based on ring buffered arrays
*/

#ifndef SEEL_Queue_h
#define SEEL_Queue_h

#include "SEEL_Params.h"
#include "SEEL_Print.h"

template <class T>
class SEEL_Queue
{
public:
    // Getters & Setters
    bool empty() { return _q_size == 0; }
    uint8_t size() { return _q_size; }
    uint8_t max_size() {return Q_MAX_SIZE; }

    // ***************************************************
    // Member functions
    // Resets all class member variables which clears queue
    void clear();

    // Returns a pointer to the next element in queue (front of queue)
    // This allows for NULL checks
    T* front();

    // Removes element at the front of the queue
    void pop_front();

    // Sends element at front of queue to the back of queue
    void recycle_front();

    // Removes elements from queue that "==" val.
    // Returns number of removed elements.
    uint8_t remove(const T& val);

    // Adds element to queue. Returns true if add was successful. If wrap is true, replaces oldest element in queue
    bool add(const T& val, bool wrap = false);

    // Searches for an element in the queue. Returns a reference to the found first element, otherwise returns NULL
    T* find(const T& val);

    void print();

protected:
    // Constructor, prevent instatiation
    SEEL_Queue();

    // ***************************************************
    // Member functions
    void inc_q_pos() 
    {
        ++_q_pos;
        if (_q_pos >= Q_MAX_SIZE)
        {
            _q_pos -= Q_MAX_SIZE;
        }
    }

    // ***************************************************
    // Member variables
    uint32_t _q_pos;
    uint32_t _q_size;
    T* _content_ary_ptr;
    uint32_t Q_MAX_SIZE;
};

template <class T> 
class SEEL_Default_Queue : public SEEL_Queue<T> {
private:
    T content_ary[SEEL_DEFAULT_QUEUE_SIZE];
public:
    SEEL_Default_Queue() {
        this->_content_ary_ptr = content_ary;
        this->Q_MAX_SIZE = SEEL_DEFAULT_QUEUE_SIZE;
    }
};

template <class T> 
class SEEL_SNode_Msg_Queue : public SEEL_Queue<T> {
private:
    T content_ary[SEEL_SNODE_MSG_QUEUE_SIZE];
public:
    SEEL_SNode_Msg_Queue() {
        this->_content_ary_ptr = content_ary;
        this->Q_MAX_SIZE = SEEL_SNODE_MSG_QUEUE_SIZE;
    }
};

template <class T> 
class SEEL_GNode_Msg_Queue : public SEEL_Queue<T> {
private:
    T content_ary[SEEL_GNODE_MSG_QUEUE_SIZE];
public:
    SEEL_GNode_Msg_Queue() {
        this->_content_ary_ptr = content_ary;
        this->Q_MAX_SIZE = SEEL_GNODE_MSG_QUEUE_SIZE;
    }
};

template <class T> 
class SEEL_Sched_Queue : public SEEL_Queue<T> {
private:
    T content_ary[SEEL_SCHED_QUEUE_SIZE];
public:
    SEEL_Sched_Queue() {
        this->_content_ary_ptr = content_ary;
        this->Q_MAX_SIZE = SEEL_SCHED_QUEUE_SIZE;
    }
};

template <class T> 
class SEEL_Extended_Packet_Queue : public SEEL_Queue<T> {
private:
    T content_ary[SEEL_EXTENDED_PACKET_MAX_NODES_EXPECTED];
public:
    SEEL_Sched_Queue() {
        this->_content_ary_ptr = content_ary;
        this->Q_MAX_SIZE = SEEL_EXTENDED_PACKET_MAX_NODES_EXPECTED;
    }
};

#endif // SEEL_Queue_h