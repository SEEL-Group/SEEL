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

template <class T>
class SEEL_Queue
{
public:
    // Member functions

    // Constructor
    SEEL_Queue();

    // Getters & Setters
    bool empty() { return _q_size == 0; }
    uint8_t size() { return _q_size; }
    uint8_t max_size() {return SEEL_QUEUE_ALLOCATION_SIZE;}

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

private:
    // Member functions

    void inc_q_pos() 
    {
        ++_q_pos;
        if (_q_pos >= SEEL_QUEUE_ALLOCATION_SIZE)
        {
            _q_pos -= SEEL_QUEUE_ALLOCATION_SIZE;
        }
    }

    // ***************************************************
    // Member variables
    T content_ary[SEEL_QUEUE_ALLOCATION_SIZE];
    uint32_t _q_pos;
    uint32_t _q_size;
};

#endif // SEEL_Queue_h