/*

########################################################################################################################
#                                                                                                                      #
#                                                                                                                      #
#                          This file was adapted from https://github.com/probonopd/MakeHex/.                           #
#                         It takes an IRP string and return the raw IR sequence. By John Fine                          #           
#                                                                                                                      #
#                                                                                                                      #
########################################################################################################################

*/

// Copyright 2005 John S. Fine

// You may use, copy, modify and/or distribute this program for private or commercial use provided that: 
// 1) You do not hold me responsible for any damage or negative consequences resulting from those activities. 
// 2) You include this copyright notice and disclaimer in any copy of any part or all of this program. 
// I do not provide any warranty of the correctness, safety, or suitibility of this program for any purpose. 
// If you do not agree to these conditions, you have no permission to use, copy, modify or distribute this program.

#pragma once

#include <vector>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <cmath>
#include <cstdio> 
#include <Data/InfraredProtocolDefinitions.h>
#include <Models/InfraredCommand.h>

std::vector<float> encodeRemoteCommand(const InfraredCommand& cmd, const char* protocolString, int& frequency);

class IRP {
public:
    IRP();
    ~IRP();

    bool readIrpString(char* str);
    void generate(int* s, int* r, std::vector<float>& raw);

private:
    struct Value {
        double m_val;
        int m_bits;
    };

    unsigned int reverse(unsigned int Number);
    bool match(const char* master);
    void setDigit(int d);
    char* copy();
    void getPair(int* result);
    void parseVal(Value& result, char*& in, int prec = 0);
    void genHex(float number);
    int genHex(const char* Pattern);
    char* m_digits[16];
    int m_frequency;
    int m_timeBase;
    int m_messageTime;
    int m_device[2];
    int m_functions[4];
    bool m_msb;
    const char* m_form;
    const char* m_prefix;
    const char* m_suffix;
    const char* m_rPrefix;
    const char* m_rSuffix;
    const char* m_def[26];
    char m_bufr[1024];
    const char* m_next;
    int m_bitGroup;
    int m_pendingBits;
    double m_cumulative;
    unsigned int m_mask[33];
    std::vector<float> m_hex;

public:
    int m_value[26];
};


