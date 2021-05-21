/*
The SEEL repository can be found at: https://github.com/SEEL-Group/SEEL
Copyright (C) SEEL Group 2021 all rights reserved
See license file in root folder for more licensing details
See SEEL_documentation.pdf for protocol description details

File purpose:   Users can enable/disable SEEL debugging messages by initializing this file
                Allows for platform independent print statements in SEEL
*/

#ifndef SEEL_Print_h
#define SEEL_Print_h

#include <Console.h>

#include "SEEL_Params.h"

class SEEL_Print
{
public:
    static void init(Stream* s)
    {
        _print_stream = s;
    }

    template<typename T>
    static size_t print(T t)
    {
        if (_print_stream == NULL)
        {
            return 0;
        }
        
        return _print_stream->print(t);
    }

    template<typename T>
    static size_t println(T t)
    {
        if (_print_stream == NULL)
        {
            return 0;
        }

        return _print_stream->println(t);
    }

    static void flush()
    {
        if (_print_stream == NULL)
        {
            return;
        }
        
        _print_stream->flush();
    }

private:
    SEEL_Print();

    static Stream* _print_stream;
};

#endif // SEEL_Print_h