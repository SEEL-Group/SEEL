/*
The SEEL repository can be found at: https://github.com/SEEL-Group/SEEL
Copyright (C) SEEL Group 2021 all rights reserved
See license file in root folder for more licensing details
See SEEL_documentation.pdf for protocol description details

File purpose:   Enables users to use assert statements that write to EEPROM on failure
*/

#ifndef SEEL_Assert_h
#define SEEL_Assert_h

#include <EEPROM.h>

#include "SEEL_Params.h"
#include "SEEL_Print.h"
#include "SEEL_Queue.h"

class SEEL_Assert
{
public:

    // Finds valid used slots to initialize internal array pointer
    static void init_nvm();

    // Print current state of nvm
    static void print_nvm_block();

    // Print nvm assert fails
    static void print_nvm_fails();

    // Marks all existing nvm slots as unused
    static void clear_nvm();

    static void equals(bool test, uint16_t file_num, uint16_t line_num);

    static SEEL_Queue<uint32_t> _assert_queue;

private:
    SEEL_Assert();

    static uint32_t _nvm_arr_start; // nvm arr start index
    static uint32_t _nvm_arr_len; // nvm total length, # elements * SEEL_ASSERT_NVM_CELLS_PER_ENTRY
    static bool _nvm_initialized;
};

#endif // SEEL_Assert_h