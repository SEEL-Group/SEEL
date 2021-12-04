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
    static void clear();

    static void equals(bool test, uint16_t file_num = 0, uint16_t line_num = 0);

    SEEL_Queue<uint32_t> _assert_queue;

private:
    SEEL_Assert();
};

#endif // SEEL_Assert_h