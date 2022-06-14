/*
The SEEL repository can be found at: https://github.com/SEEL-Group/SEEL
Copyright (C) SEEL Group 2021 all rights reserved
See license file in root folder for more licensing details
See SEEL_documentation.pdf for protocol description details

File purpose:   Task scheduler for user and SEEL tasks. Allows users to insert their own tasks
                without interfering with the SEEL system. Contains the main loop() used in SEEL
*/

#ifndef SEEL_Scheduler_h
#define SEEL_Scheduler_h

#include "SEEL_Defines.h"
#include "SEEL_Task.h"

// For setting system time, affecting the millis() function
extern volatile uint32_t timer0_millis;

class SEEL_Scheduler
{
public:
    // ***************************************************
    // Member functions

    // Constructor
    SEEL_Scheduler() : _task_counter(0), _user_task_enable(false) {}

    // Returns true if there is a task running and the task is a user task
    // Gives task info via argument pointers if there is a task running and it is a user task
    bool get_task_info(uint32_t* ret_task_start_time, uint32_t* ret_task_delay, uint32_t* ret_task_id);

    // Adds one-shot task to the scheduler
    // Calculates the time for the task to run
    // Returns true if successfully added
    bool add_task(SEEL_Task* tf, uint32_t task_delay = 0);

    // Starts infinite scheduler loop
    void run();

    // USERS should not call the below public methods unless they know what they're doing

    // Clears all queued tasks in scheduler
    void clear_tasks() {_scheduler_queue.clear();}

    // Return current task counter and increment afterwards
    uint32_t assign_task_id() {return _task_counter++;}

    // Enable/disable user tasks 
    void set_user_task_enable(bool user_toggle) {_user_task_enable = user_toggle;}

    // If system time shifts, all past scheduler tasks need to have their times altered
    void offset_task_times(int32_t time_offset_millis);
    
    // Millis() overflows in ~50 days; on overflow, tasks can run before they are scheduled
    // This method handles overflow by setting millis() to 0 and tasks to their relative run times
    void zero_millis_timer();
    
    void adjust_time(uint32_t new_millis);
private:
    // Structs & Classes
    
    // Allows functions and relevant information to scheduling the function to be packaged together
    struct SEEL_Sched_Unit
    {
        // Task function reference
        SEEL_Task* ref_task;
        // Gets set by system to determine whether to run the task at call
        uint32_t time_to_run;
        // Delay for tasks that require it
        uint32_t delay_millis;
        // INSTANCE ID used for tracking tasks, for debug use. 
        uint32_t task_id;

        // Constructors
        SEEL_Sched_Unit()
        : ref_task(NULL), time_to_run(0), delay_millis(0), task_id(0) {}
        SEEL_Sched_Unit(SEEL_Task* rt, uint32_t ttr, uint32_t delay, uint32_t tid) 
        : ref_task(rt), time_to_run(ttr), delay_millis(delay), task_id(tid) {}
    };

    // Sets system time
    void set_millis(uint32_t new_millis);

    // Member variables
    SEEL_Sched_Queue<SEEL_Sched_Unit> _scheduler_queue;
    SEEL_Sched_Unit* _current_task_ptr;
    uint32_t _task_counter;
    bool _user_task_enable;
};

#endif // SEEL_Scheduler_h