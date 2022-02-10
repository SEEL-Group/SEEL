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

// SEEL Files, max file num defined in SEEL_Params.h
// Avoid file '0' since default is 0, use for error case
constexpr uint16_t SEEL_ASSERT_FILE_NUM_QUEUE        = 1;
constexpr uint16_t SEEL_ASSERT_FILE_NUM_SCHEDULER    = 2;
constexpr uint16_t SEEL_ASSERT_FILE_NUM_NODE         = 3;
constexpr uint16_t SEEL_ASSERT_FILE_NUM_GNODE        = 4;
constexpr uint16_t SEEL_ASSERT_FILE_NUM_SNODE        = 5;
// User Files
constexpr uint16_t SEEL_ASSERT_FILE_NUM_USER0        = 6;
constexpr uint16_t SEEL_ASSERT_FILE_NUM_USER1        = 7;

class SEEL_Assert
{
public:

    // Finds valid used slots to initialize internal array pointer
    static inline void init_nvm()
    {
#if SEEL_ASSERT_ENABLE == TRUE && SEEL_ASSERT_ENABLE_NVM == TRUE
        init_nvm_helper();
#endif // SEEL_ASSERT_ENABLE && SEEL_ASSERT_ENABLE_NVM
    }

    // Print current state of nvm
    static inline void print_nvm_block()
    {
#if SEEL_ASSERT_ENABLE == TRUE && SEEL_ASSERT_ENABLE_NVM == TRUE
        print_nvm_block_helper();
#endif // SEEL_ASSERT_ENABLE && SEEL_ASSERT_ENABLE_NVM
    }

    // Print nvm assert fails
    static inline void print_nvm_fails()
    {
#if SEEL_ASSERT_ENABLE == TRUE && SEEL_ASSERT_ENABLE_NVM == TRUE
        print_nvm_fails_helper();
#endif // SEEL_ASSERT_ENABLE && SEEL_ASSERT_ENABLE_NVM
    }

    // Marks all existing nvm slots as unused
    static inline void clear_nvm()
    {
#if SEEL_ASSERT_ENABLE == TRUE && SEEL_ASSERT_ENABLE_NVM == TRUE
        clear_nvm_helper();
#endif // SEEL_ASSERT_ENABLE && SEEL_ASSERT_ENABLE_NVM
    }

    static inline void assert(bool test, uint16_t file_num, uint16_t line_num, bool nvm_write = true)
    {
#if SEEL_ASSERT_ENABLE == TRUE
        assert_helper(test, file_num, line_num, nvm_write);
#endif // SEEL_ASSERT_ENABLE
    }

#if SEEL_ASSERT_ENABLE == TRUE
    static SEEL_Queue<uint32_t> _assert_queue;
#endif // SEEL_ASSERT_ENABLE

private:
    SEEL_Assert();

#if SEEL_ASSERT_ENABLE == TRUE
#if SEEL_ASSERT_ENABLE_NVM == TRUE
    static void init_nvm_helper();
    static void print_nvm_block_helper();
    static void print_nvm_fails_helper();
    static void clear_nvm_helper();

    static uint32_t _nvm_arr_start; // nvm arr start index
    static uint32_t _nvm_arr_len; // nvm total length, # elements * SEEL_ASSERT_NVM_CELLS_PER_ENTRY
    static bool _nvm_initialized;
#endif // SEEL_ASSERT_ENABLE_NVM

    static void assert_helper(bool test, uint16_t file_num, uint16_t line_num, bool nvm_write);
#endif // SEEL_ASSERT_ENABLE
};

#endif // SEEL_Assert_h