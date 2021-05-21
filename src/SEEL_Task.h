/*
The SEEL repository can be found at: https://github.com/SEEL-Group/SEEL
Copyright (C) SEEL Group 2021 all rights reserved
See license file in root folder for more licensing details
See SEEL_documentation.pdf for protocol description details

File purpose:   SEEL Tasks that can be inserted into the scheduler
*/

#ifndef SEEL_Task_h
#define SEEL_Task_h

#include "SEEL_Defines.h"

// Users can either pass in a function pointer to SEEL_Task's constructor or 
// inherit SEEL_Task_h and add custom functions (allows the usage of state variables)

class SEEL_Task
{
public:
    // Typedefs
    typedef void (*func_ptr_t)(void);

    // ***************************************************
    // Member functions

    // Constructors
    // Default to user_task = true, user tasks should have this as "true" to prevent overriding LoRa tasks
    // Allows for users to pass in function pointer to create a SEEL_Task object
    SEEL_Task() : _user_task(true), _func(NULL) {}
    SEEL_Task(func_ptr_t f, bool ut = true) : _user_task(ut), _func(f) {}

    // Getters and setters
    bool get_user_status() { return _user_task; }
    void set_func(func_ptr_t f) { _func = f; }

    // Returns false if the task could not run (no instance)
    virtual void run() { _func ? _func() : (void)SEEL_Print::println(F("E-F")); } // Error - Function not set

    // Destructor
    // Use virtual to call appropriate destructor dynamically
    virtual ~SEEL_Task() {}

protected:
    // Member variables
    bool _user_task;

private:
    // Member variables
    func_ptr_t _func;
};

#endif // SEEL_Task_h