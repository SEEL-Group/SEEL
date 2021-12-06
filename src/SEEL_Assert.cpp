/*
The SEEL repository can be found at: https://github.com/SEEL-Group/SEEL
Copyright (C) SEEL Group 2021 all rights reserved
See license file in root folder for more licensing details
See SEEL_documentation.pdf for protocol description details

File purpose:   See SEEL_Assert.h
*/

#include "SEEL_Assert.h"
#include "SEEL_Queue.cpp"

SEEL_Queue<uint32_t> SEEL_Assert::_assert_queue;

void SEEL_Assert::print_nvm()
{
    if (!SEEL_ASSERT_ENABLE_NVM)
    {
        SEEL_Print::println(F("Assert NVM not enabled"));
        return;
    }

    const uint8_t BYTES_PER_LINE = 32;
    const uint32_t DIV_PER_FILE = EEPROM.length() / SEEL_ASSERT_NVM_MAX_FILE_NUM;
    String assert_fail_str = "";

    SEEL_Print::print(F("Device EEPROM Size: ")); SEEL_Print::println(EEPROM.length());
    SEEL_Print::flush();

    for (uint16_t i = 0; i < EEPROM.length(); ++i)
    {
        uint8_t val = EEPROM.read(i);

        if (SEEL_ASSERT_NVM_PRINT_BLOCK)
        {
            if (i % BYTES_PER_LINE == (BYTES_PER_LINE - 1))
            {
                SEEL_Print::println(val);
                SEEL_Print::flush();
            }
            else
            {
                SEEL_Print::print(val); SEEL_Print::print(" ");
            }
        }

        if (val != 0)
        {
            // Check each bit of val to see which assert(s) failed
            for (uint8_t j = 0; j < 8; ++j, val = val >> 1)
            {
                uint8_t val_bit = val & 1;

                if (val_bit == 1)
                {
                    String file_num_str = String(i / DIV_PER_FILE);
                    String line_num_str = String((i % DIV_PER_FILE) * 8 + j);
                    assert_fail_str += F("ASSERT FAIL: File ");
                    assert_fail_str += file_num_str;
                    assert_fail_str += F(", Line ");
                    assert_fail_str += line_num_str;
                    assert_fail_str += F("\n");
                }
            }
        }
    }

    // Print out assert fails at the end, if any
    if (assert_fail_str != "")
    {
        SEEL_Print::println(assert_fail_str);
        SEEL_Print::flush();
    }
}

void SEEL_Assert::clear_nvm()
{
    if (!SEEL_ASSERT_ENABLE_NVM)
    {
        SEEL_Print::println(F("Assert NVM not enabled"));
        return;
    }

    for (uint16_t i = 0; i < EEPROM.length(); ++i)
    {
        EEPROM.update(i, 0); // update only sets necessary bits to reduce wear
    }
}

void SEEL_Assert::equals(bool test, uint16_t file_num, uint16_t line_num)
{
    if (!SEEL_ASSERT_ENABLE || test)
    {
        // Assert passed
        return;
    }

    // Assert failed

    // Print assert failure
    SEEL_Print::print(F("ASSERT FAIL: File: ")); SEEL_Print::print(file_num);
    SEEL_Print::print(F(", Line: ")); SEEL_Print::println(line_num);

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

    // Write assert failure to EEPROM
    if (SEEL_ASSERT_ENABLE_NVM)
    {
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

        // EEPROM has EEPROM.length() bytes available, with each byte containing 8 bits
        uint32_t max_eeprom_bits = EEPROM.length() * 8; // EEPROM.length is uint16_t, will not overflow
        uint32_t max_line_size = max_eeprom_bits / SEEL_ASSERT_NVM_MAX_FILE_NUM;

        if (line_num < max_line_size)
        {
            uint32_t idx = ((EEPROM.length() / SEEL_ASSERT_NVM_MAX_FILE_NUM) * file_num) + (line_num / 8);
            uint32_t val = EEPROM.read(idx) | (1 << (line_num % 8)); // Don't modify the existing bits for that EEPROM index
            EEPROM.update(idx, val); // Update only sets necessary bits to reduce wear
        }
        else
        {
            SEEL_Print::print(F("Assert NVM line num invalid: ")); SEEL_Print::print(line_num);
            SEEL_Print::print(F(", max (given file size and EEPROM size): ")); SEEL_Print::println(max_line_size);
        }
    }
    else
    {
        SEEL_Print::println(F("Assert NVM not enabled"));
    }
}