/*
The SEEL repository can be found at: https://github.com/SEEL-Group/SEEL
Copyright (C) SEEL Group 2021 all rights reserved
See license file in root folder for more licensing details
See SEEL_documentation.pdf for protocol description details

File purpose:   See SEEL_Queue.h
*/

#include "SEEL_Queue.h"

template <class T>
SEEL_Queue<T>::SEEL_Queue()
{
    clear();
}

template <class T>
void SEEL_Queue<T>::clear()
{
    _q_size = 0;
    _q_pos = 0;
}

template <class T>
T* SEEL_Queue<T>::front()
{
    if (_q_size == 0)
    {
        return NULL;
    }

    return &content_ary[_q_pos];
}

template <class T>
void SEEL_Queue<T>::pop_front()
{
    if (!empty())
    {
        --_q_size;
        inc_q_pos();
    }
}

template <class T>
void SEEL_Queue<T>::recycle_front()
{
    if (_q_size > 1) // If _q_size 1 or less, no change required
    {
        if (_q_size == SEEL_QUEUE_ALLOCATION_SIZE) // Queue is full
        {
            // If queue full, move _q_pos pointer to next element and 
            // original element becomes end because of ring buffer structure
            inc_q_pos();
        }
        else
        {
            uint32_t insert_index = _q_size + _q_pos;
            insert_index = insert_index % SEEL_QUEUE_ALLOCATION_SIZE;
            content_ary[insert_index] = content_ary[_q_pos];
            inc_q_pos();
        }
    }
}

template <class T>
uint8_t SEEL_Queue<T>::remove(const T& val)
{
    uint32_t move_inc = 0;
    // Remove values by shifting post-values up
    for (uint32_t i = 0; i < _q_size; ++i)
    {
        uint32_t current_index = (_q_pos + i) % SEEL_QUEUE_ALLOCATION_SIZE;
        if (content_ary[current_index + move_inc] == val)
        {
            ++move_inc;
            continue; // So we don't move removed values
        }

        // New attempt to move present to past values instead of present to future 
        if (move_inc > 0)
        {
            uint32_t new_index = (current_index - move_inc) % SEEL_QUEUE_ALLOCATION_SIZE;
            content_ary[new_index] = content_ary[current_index];
        }
    }
    _q_size -= move_inc;
    
    return move_inc;
}

template <class T>
bool SEEL_Queue<T>::add(const T& val, bool wrap)
{
    if (wrap && _q_size >= SEEL_QUEUE_ALLOCATION_SIZE)
    {
        SEEL_Print::println(F("QUEUE FULL")); 
        SEEL_Print::flush();
        pop_front();
    }

    if (_q_size < SEEL_QUEUE_ALLOCATION_SIZE)
    {
        uint32_t insert_index = _q_size + _q_pos;
        insert_index = insert_index % SEEL_QUEUE_ALLOCATION_SIZE;
        content_ary[insert_index] = val;
        ++_q_size;
        return true;
    }

    return false;
}

template <class T>
T* SEEL_Queue<T>::find(const T& val)
{
    for (uint32_t i = 0; i < _q_size; ++i)
    {
        uint32_t current_index = (_q_pos + i) % SEEL_QUEUE_ALLOCATION_SIZE;
        if (content_ary[current_index] == val)
        {
            return &content_ary[current_index];
        }
    }

    return NULL;
}