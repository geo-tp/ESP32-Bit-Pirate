// USB CDC SDR adapter
// Experimental tuning: 100 to 6000 MHz; recommended RF region: 2.4 GHz
// Using internal low level RF from the S3

#pragma once

#include "Interfaces/IInput.h"
#include "Interfaces/IHostSerial.h"

// One-shot boot adapter: exclusive raw RF reception
class SdrCdcAdapter {
public:
    static void run(IInput& input, IHostSerial& hostSerial);
};
