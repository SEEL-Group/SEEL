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

    return &_content_ary_ptr[_q_pos];
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
        if (_q_size == SEEL_DEFAULT_QUEUE_SIZE) // Queue is full
        {
            // If queue full, move _q_pos pointer to next element and 
            // original element becomes end because of ring buffer structure
            inc_q_pos();
        }
        else
        {
            uint32_t insert_index = _q_size + _q_pos;
            insert_index = insert_index % SEEL_DEFAULT_QUEUE_SIZE;
            _content_ary_ptr[insert_index] = _content_ary_ptr[_q_pos];
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
        uint32_t current_index = (_q_pos + i) % SEEL_DEFAULT_QUEUE_SIZE;
        if (_content_ary_ptr[current_index + move_inc] == val)
        {
            ++move_inc;
            continue; // So we don't move removed values
        }

        // New attempt to move present to past values instead of present to future 
        if (move_inc > 0)
        {
            uint32_t new_index = (current_index - move_inc) % SEEL_DEFAULT_QUEUE_SIZE;
            _content_ary_ptr[new_index] = _content_ary_ptr[current_index];
        }
    }
    _q_size -= move_inc;
    
    return move_inc;
}

template <class T>
bool SEEL_Queue<T>::add(const T& val, bool wrap)
{
    if ( _q_size >= SEEL_DEFAULT_QUEUE_SIZE)
    {
        SEEL_Print::print(F("QUEUE FULL, SIZE: ")); 
        SEEL_Print::println(_q_size);

        SEEL_Print::flush();
        if(wrap) {
            pop_front();
        }
    }

    else 
    {
        uint32_t insert_index = _q_size + _q_pos;
        insert_index = insert_index % SEEL_DEFAULT_QUEUE_SIZE;
        _content_ary_ptr[insert_index] = val;
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
        uint32_t current_index = (_q_pos + i) % SEEL_DEFAULT_QUEUE_SIZE;
        if (_content_ary_ptr[current_index] == val)
        {
            return &_content_ary_ptr[current_index];
        }
    }

    return NULL;
}

template<class T>

void SEEL_Queue<T>::print() {

    SEEL_Print::print(F("SIZE: "));
    SEEL_Print::print(_q_size);
    SEEL_Print::print(F(" POS: "));
    SEEL_Print::print(_q_pos);
    SEEL_Print::print(F(" [ "));

    int first_len = (_q_pos + _q_size > SEEL_DEFAULT_QUEUE_SIZE) ? SEEL_DEFAULT_QUEUE_SIZE:
         _q_pos + _q_size;

     for (uint8_t i = _q_pos; i < first_len; ++i) {
         SEEL_Print::print(_content_ary_ptr[i]);
         SEEL_Print::print(F(" "));
     }

    // wrap print
     if (_q_pos+_q_size > SEEL_DEFAULT_QUEUE_SIZE) {
            int adj_len =  _q_pos + _q_size - SEEL_DEFAULT_QUEUE_SIZE;
            for (uint8_t i = 0; i < adj_len; ++i) {
                SEEL_Print::print(_content_ary_ptr[i]);
                SEEL_Print::print(F(" "));
            }
     }

    SEEL_Print::println(F("]"));
    SEEL_Print::flush();
}



