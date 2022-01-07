/*
The SEEL repository can be found at: https://github.com/SEEL-Group/SEEL
Copyright (C) SEEL Group 2021 all rights reserved
See license file in root folder for more licensing details
See SEEL_documentation.pdf for protocol description details

File purpose:   See SEEL_Scheduler.h
*/

#include "SEEL_Scheduler.h"

bool SEEL_Scheduler::add_task(SEEL_Task* tf, uint32_t task_delay)
{
    uint32_t tid = assign_task_id();
    uint32_t current_millis = millis();
    uint32_t time_to_run = current_millis + task_delay;
    
    if (time_to_run < current_millis)
    {
        // Overflow will occur
        zero_millis_timer();
        time_to_run = current_millis + task_delay;
    }
    
    SEEL_Sched_Unit u(tf, time_to_run, task_delay, tid);

    return _scheduler_queue.add(u);
}

bool SEEL_Scheduler::get_task_info(uint32_t* ret_task_start_time, uint32_t* ret_task_delay, uint32_t* ret_task_id)
{
    if (_current_task_ptr == NULL)
    {
        return false;
    }

    *ret_task_start_time = _current_task_ptr->time_to_run;
    *ret_task_delay = _current_task_ptr->delay_millis;
    *ret_task_id = _current_task_ptr->task_id;
    return true;
}

void SEEL_Scheduler::offset_task_times(int32_t offset_millis)
{
    uint32_t current_millis = millis();
    for (uint32_t i = 0; i < _scheduler_queue.size(); ++i)
    {
        uint32_t ttr = _scheduler_queue.front()->time_to_run + offset_millis;
        
        if (offset_millis > 0 && ttr < current_millis)
        {
            // Overflow has occurred
            zero_millis_timer();
            _scheduler_queue.front()->time_to_run += offset_millis;
        }
        else
        {
            _scheduler_queue.front()->time_to_run = ttr;
        }            
        
        _scheduler_queue.recycle_front();
    }
}

// Need a function to reset the millis() timer since we do uint comparisons
// System will likely get un-sync'd on overflow until next cycle
void SEEL_Scheduler::zero_millis_timer()
{   
    uint32_t original_millis = millis();
    set_millis(0);

    for (uint32_t i = 0; i < _scheduler_queue.size(); ++i)
    {
        if (_scheduler_queue.front()->time_to_run < original_millis)
        {
            // Task is overdue to run
            _scheduler_queue.front()->time_to_run = 0;
        }
        else
        {
            _scheduler_queue.front()->time_to_run -= original_millis;
        }
        _scheduler_queue.recycle_front();
    }
}

void SEEL_Scheduler::adjust_time(uint32_t new_millis)
{
    uint32_t old_millis = millis();

    set_millis(new_millis);

    // Correct times in Scheduler with updated times
    // Note: Large time jumps (a difference bigger than int32_max) may cause tasks to run earlier than scheduled
    SEEL_Assert::assert(((int64_t)new_millis - (int64_t)old_millis) >= INT32_MIN &&
        ((int64_t)new_millis - (int64_t)old_millis) <= INT32_MAX, SEEL_ASSERT_FILE_NUM_SCHEDULER, __LINE__);
    offset_task_times((int32_t)new_millis - (int32_t)old_millis);
}

void SEEL_Scheduler::set_millis(uint32_t new_millis)
{
    // disables interrupts so no reads/writes can occur in the middle of writing to timer0
    cli();
 
    timer0_millis = new_millis;
 
    // re-enable interrupts
    sei();
}

void SEEL_Scheduler::run()
{
    // Main loop for SEEL (instead of in Arduino's loop() function)
    while (true)
    {
        uint32_t current_time = millis();

        // Check if it's time to run the front task
        _current_task_ptr = _scheduler_queue.front();
        if (_current_task_ptr != NULL)
        {
            // Run the task if 1) Task is a system task or 2) Task is a user task and user tasks are enabled
            bool run = (!_current_task_ptr->ref_task->get_user_status() || _user_task_enable);
            if (_current_task_ptr->time_to_run <= current_time && run)
            {
                _scheduler_queue.pop_front();
                _current_task_ptr->ref_task->run();
            }
            else // Not time to run yet
            {
                // Recycle the front task, sending it to the back efficiently
                _scheduler_queue.recycle_front();
            }
        }
        _current_task_ptr = NULL;
    }
}