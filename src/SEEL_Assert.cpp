/*
The SEEL repository can be found at: https://github.com/SEEL-Group/SEEL
Copyright (C) SEEL Group 2021 all rights reserved
See license file in root folder for more licensing details
See SEEL_documentation.pdf for protocol description details

File purpose:   See SEEL_Assert.h
*/

#include "SEEL_Assert.h"
#include "SEEL_Queue.cpp"

#if SEEL_ASSERT_ENABLE == TRUE
SEEL_Queue<uint32_t> SEEL_Assert::_assert_queue;

#if SEEL_ASSERT_ENABLE_NVM == TRUE
uint32_t SEEL_Assert::_nvm_arr_start = 0;
uint32_t SEEL_Assert::_nvm_arr_len = 0;
bool SEEL_Assert::_nvm_initialized = false;

void SEEL_Assert::init_nvm_helper()
{
    // Initialize start to 0 for the case the entire cell block is valid
    _nvm_arr_start = 0;
    _nvm_arr_len = 0;

    uint32_t wrap_count = 0;
    bool unused_seen = false;
    bool head_set = false;
    for (uint16_t i = 0; i < EEPROM.length(); i += SEEL_ASSERT_NVM_CELLS_PER_ENTRY)
    {
        uint8_t val = EEPROM.read(i);
        bool valid_entry = val >> 7;

        if (valid_entry)
        {
            if (unused_seen)
            {
                if (!head_set) // head not set
                {
                    // This entry marks a transition from unused to used entry, set this cell as dummy head
                    _nvm_arr_start = i;
                    head_set = true;
                }
                _nvm_arr_len += SEEL_ASSERT_NVM_CELLS_PER_ENTRY;
            }
            else // unused not seen yet
            {
                 // We have a valid entry but haven't seen ununsed cell yet, this could be a wrap-around case
                ++wrap_count;
            }
        }
        else // not valid entry
        {
            unused_seen = true;
            if (head_set)
            {
                // We've ended the continuous array search, return to avoid futher searches; we know 
                // this is not a wrap-around case
                _nvm_initialized = true;
                return;
            }
        }
    }

    // If we got here, that means last entry of NVM array was valid entry, so there may be a wrap-around case;
    // add the previous wrap-around values to length
    _nvm_arr_len += wrap_count * SEEL_ASSERT_NVM_CELLS_PER_ENTRY;
    _nvm_initialized = true;
}

void SEEL_Assert::print_nvm_block_helper()
{
    if (!_nvm_initialized)
    {
        SEEL_Print::println(F("Assert NVM not initialized"));
        return;
    }

    SEEL_Print::print(F("Device EEPROM Size: ")); SEEL_Print::println(EEPROM.length());
    SEEL_Print::flush();

    for (uint16_t i = 0; i < EEPROM.length(); ++i)
    {
        uint8_t val = EEPROM.read(i);

        if (SEEL_ASSERT_NVM_PRINT_BLOCK)
        {
            if (i % SEEL_ASSERT_NVM_PRINT_BLOCK_WIDTH == (SEEL_ASSERT_NVM_PRINT_BLOCK_WIDTH - 1))
            {
                SEEL_Print::println(val);
                SEEL_Print::flush();
            }
            else
            {
                SEEL_Print::print(val); SEEL_Print::print(" ");
            }
        }
    }
}

void SEEL_Assert::print_nvm_fails_helper()
{
    if (!_nvm_initialized)
    {
        SEEL_Print::println(F("Assert NVM not initialized"));
        return;
    }

    SEEL_Print::print(F("Device EEPROM Size: ")); SEEL_Print::println(EEPROM.length());
    SEEL_Print::flush();

    bool valid_entry = false; // tracks whether groupings of SEEL_ASSERT_NVM_CELLS_PER_ENTRY are valid
    bool prev_valid_entry = false;
    for (uint16_t i = 0; i < EEPROM.length(); ++i)
    {
        uint8_t val = EEPROM.read(i);

        uint16_t file_num = 0;
        uint16_t line_num = 0;
        // Format:
        // [(1bit, cell used); (7bits, (uint8)(File_num >> 8))] [(8bits, (uint8)File_num)] [(8bits, (uint8)(Line_num >> 8))] [(8bits, (uint8)Line_num)]
        if (i % SEEL_ASSERT_NVM_CELLS_PER_ENTRY == 0)
        {
            valid_entry = (val >> 7);

            if (valid_entry)
            {
                file_num += (val & 0x7F) << 8; // erase the cell_used bit
            }
            else
            {
                prev_valid_entry = false;
            }
        }
        else if (valid_entry)
        {
            switch(i % SEEL_ASSERT_NVM_CELLS_PER_ENTRY)
            {
                case 1:
                    file_num += val;
                    break;
                case 2:
                    line_num += val << 8;
                    break;
                case 3:
                    line_num += val;
                    String assert_fail_str = "";
                    if (prev_valid_entry)
                    {
                        assert_fail_str += F("PRINT ASSERT FAIL: File ");
                    }
                    else
                    {
                        assert_fail_str += F("PRINT ASSERT FAIL (Maybe Dummy Head): File "); // maybe since it could be a starting block and wraparound expensive to check
                    }
                    assert_fail_str += String(file_num);
                    assert_fail_str += F(", Line ");
                    assert_fail_str += String(line_num);
                    SEEL_Print::println(assert_fail_str);
                    prev_valid_entry = true;
                    break;
                default:
                    SEEL_Print::println(F("NVM Invalid Cell Index in Print"));
                    return;
            }
        }
    }
    SEEL_Print::flush();
}

void SEEL_Assert::clear_nvm_helper()
{
    if (!_nvm_initialized)
    {
        SEEL_Print::println(F("Assert NVM not initialized"));
        return;
    }

    for (uint16_t i = 0; i < EEPROM.length(); i+= SEEL_ASSERT_NVM_CELLS_PER_ENTRY)
    {
        EEPROM.update(i, 0); // update only sets necessary bits to reduce wear
    }

    // Re-init with dummy head
    _nvm_arr_start = (_nvm_arr_start + _nvm_arr_len) % EEPROM.length();
    EEPROM.update(_nvm_arr_start, 0x80);
    _nvm_arr_len = SEEL_ASSERT_NVM_CELLS_PER_ENTRY;
}
#endif // SEEL_ASSERT_ENABLE_NVM

void SEEL_Assert::equals_helper(bool test, uint16_t file_num, uint16_t line_num)
{
    if (test)
    {
        // Assert passed
        return;
    }
    // Assert failed

    // Print assert failure
    SEEL_Print::print(F("ASSERT FAIL: File ")); SEEL_Print::print(file_num);
    SEEL_Print::print(F(", Line ")); SEEL_Print::println(line_num);

    // Add error to assert queue
    uint32_t error = 0;
    error += (uint32_t)file_num << 16;
    error += line_num;
    // use find() to reduce duplicate in queue
    if (_assert_queue.find(error) == NULL)
    {
        if (!_assert_queue.add(error))
        {
            SEEL_Print::println(F("Assert Queue Full, assert discarded"));
        }
    }

    #if SEEL_ASSERT_ENABLE_NVM == TRUE
    // Write assert failure to EEPROM
    if (!_nvm_initialized)
    {
        SEEL_Print::println(F("Assert NVM not initialized"));
        return;
    }
    if (file_num >= SEEL_ASSERT_NVM_MAX_FILE_NUM)
    {
        SEEL_Print::print(F("Assert NVM file num invalid: ")); SEEL_Print::println(file_num);
        return;
    }
    if (line_num >= SEEL_ASSERT_NVM_MAX_LINE_NUM)
    {
        SEEL_Print::print(F("Assert NVM line num invalid: ")); SEEL_Print::println(line_num);
        return;
    }

    if (_nvm_arr_len + SEEL_ASSERT_NVM_CELLS_PER_ENTRY > EEPROM.length() ||
        (((_nvm_arr_start + _nvm_arr_len) % EEPROM.length()) + SEEL_ASSERT_NVM_CELLS_PER_ENTRY) > EEPROM.length()) // Check for overflow at end
    {
        // EEPROM full, discarding assert
        SEEL_Print::println(F("NVM Full, assert discarded"));
        return;
    }

    for (uint8_t i = 0; i < SEEL_ASSERT_NVM_CELLS_PER_ENTRY; ++i)
    {
        uint32_t cell_index = (_nvm_arr_start + _nvm_arr_len + i) % EEPROM.length();
        uint8_t val;

        // Format:
        // [(1bit, cell used); (7bits, (uint8)(File_num >> 8))] [(8bits, (uint8)File_num)] [(8bits, (uint8)(Line_num >> 8))] [(8bits, (uint8)Line_num)]
        switch(i % SEEL_ASSERT_NVM_CELLS_PER_ENTRY)
        {
            case 0:
                val = (uint8_t)(file_num >> 8);
                val |= (1 << 7); // Overwrite the MSB bit with special field to check if cell is used
                break;
            case 1:
                val = (uint8_t)(file_num);
                break;
            case 2:
                val = (uint8_t)(line_num >> 8);
                break;
            case 3:
                val = (uint8_t)(line_num);
                break;
            default:
                SEEL_Print::println(F("NVM Invalid Cell Index in Add"));
                return;
        }

        EEPROM.update(cell_index, val);
    }

    _nvm_arr_len += SEEL_ASSERT_NVM_CELLS_PER_ENTRY;
    #endif // SEEL_ASSERT_ENABLE_NVM
}
#endif // SEEL_ASSERT_ENABLE