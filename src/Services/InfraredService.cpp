#include "InfraredService.h"

#include <algorithm>
#include <Vendors/MakeHex.h>

#ifdef INFRARED_IREMOTE_ESP8266

/*
====================
IRremoteESP8266
====================
*/

#include <IRremoteESP8266.h>
#include <IRutils.h>

void InfraredService::configure(uint8_t tx, uint8_t rx) {
    if (tx == _txPin && rx == _rxPin) return;

    _txPin = tx;
    _rxPin = rx;

    if (_receiver) {
        _receiver->disableIRIn();
        delete _receiver;
        _receiver = nullptr;
    }

    if (_sender) {
        delete _sender;
        _sender = nullptr;
    }

    _sender = new IRsend(_txPin);
    _sender->begin();

    _receiver = new IRrecv(_rxPin, kRawBufferSize);

    _configured = true;
}

InfraredService::~InfraredService() {
    if (_receiver) {
        _receiver->disableIRIn();
        delete _receiver;
        _receiver = nullptr;
    }
    if (_sender) {
        delete _sender;
        _sender = nullptr;
    }
}

void InfraredService::startReceiver() {
    if (_receiver) _receiver->enableIRIn();
}

void InfraredService::stopReceiver() {
    if (_receiver) _receiver->disableIRIn();
}

void InfraredService::sendInfraredCommand(InfraredCommand command) {
    if (!_sender) return;

    std::string protoStr = InfraredProtocolMapper::toString(command.getProtocol());

    uint8_t device = command.getDevice();
    int subdevice   = command.getSubdevice();
    uint8_t func    = command.getFunction();

    uint16_t addr16 = packAddress(device, subdevice);

    // Handle this subset of protocols with iremote for better reliability
    switch (command.getProtocol()) {

        case InfraredProtocolEnum::_NEC:
        case InfraredProtocolEnum::_NEC2: {
            uint32_t frame = _sender->encodeNEC(addr16, func);
            _sender->sendNEC(frame);
            return;
        }

        case InfraredProtocolEnum::_SAMSUNG:
        case InfraredProtocolEnum::SAMSUNG20: {
            if (subdevice != -1) break;

            uint32_t frame = _sender->encodeSAMSUNG(device, func);
            _sender->sendSAMSUNG(frame);
            return;
        }

        case InfraredProtocolEnum::SONY20: {
            uint32_t frame = _sender->encodeSony(20, func, addr16, 0);
            _sender->sendSony(frame);
            return;
        }

        case InfraredProtocolEnum::_LG: {
            if (subdevice != -1) break;

            uint32_t frame = _sender->encodeLG(addr16, func);
            _sender->sendLG(frame);
            return;
        }

        case InfraredProtocolEnum::_SANYO_LC7461: {
            uint32_t frame = _sender->encodeSanyoLC7461(addr16, func);
            _sender->sendSanyoLC7461(frame);
            return;
        }

        case InfraredProtocolEnum::_PANASONIC:
        case InfraredProtocolEnum::PANASONIC_:
        case InfraredProtocolEnum::PANASONIC2: {
            uint16_t vendor = getKaseikyoVendorIdCode(protoStr);
            uint8_t sub = (subdevice < 0) ? 0 : (uint8_t)subdevice;

            uint64_t frame = _sender->encodePanasonic(vendor, device, sub, func);
            _sender->sendPanasonic64(frame);
            return;
        }

        case InfraredProtocolEnum::_PIONEER: {
            uint64_t frame = _sender->encodePioneer(addr16, func);
            _sender->sendPioneer(frame);
            return;
        }

        default:
            break;
    }

    // Fallback MakeHex for all other protcols
    int frequency = IR_DEFAULT_FREQUENCY_KHZ;
    std::vector<float> seq = encodeRemoteCommand(command, protoStr.c_str(), frequency);
    if (seq.empty()) return;

    std::vector<uint16_t> raw;
    raw.reserve(seq.size());
    for (float v : seq) {
        uint32_t us = (uint32_t)v;
        if (us > 0xFFFF) us = 0xFFFF;
        raw.push_back((uint16_t)us);
    }

    _sender->sendRaw(raw.data(), raw.size(), frequency);
}

void InfraredService::sendInfraredFileCommand(InfraredFileRemoteCommand command) {
    if (!_sender) return;

    if (command.protocol == InfraredProtocolEnum::_RAW) {
        _sender->sendRaw(command.rawData, command.rawDataSize, command.frequency);
        return;
    }

    uint8_t dev = command.address & 0xFF;
    uint8_t sub = (command.address >> 8) & 0xFF;

    InfraredCommand finalCmd(
        command.protocol,
        dev,
        sub,
        command.function
    );

    sendInfraredCommand(finalCmd);
}

void InfraredService::sendRaw(const std::vector<uint16_t>& timings, uint32_t khz) {
    if (!_sender || timings.empty()) return;
    _sender->sendRaw(timings.data(), timings.size(), khz);
}

InfraredCommand InfraredService::receiveInfraredCommand() {
    InfraredCommand command;
    if (!_receiver) return command;

    if (!_receiver->decode(&_results)) {
        return command;
    }

    _receiver->resume();

    if (_results.bits == 0 || _results.decode_type == UNKNOWN) {
        return InfraredCommand();
    }

    switch (_results.decode_type) {
        case NEC:       command.setProtocol(InfraredProtocolEnum::_NEC); break;
        case SONY:      command.setProtocol(InfraredProtocolEnum::SONY20); break;
        case PANASONIC: command.setProtocol(InfraredProtocolEnum::_PANASONIC); break;
        case JVC:       command.setProtocol(InfraredProtocolEnum::_JVC); break;
        case SAMSUNG:   command.setProtocol(InfraredProtocolEnum::SAMSUNG20); break;
        case DENON:     command.setProtocol(InfraredProtocolEnum::_DENON); break;
        case RC5:       command.setProtocol(InfraredProtocolEnum::_RC5); break;
        case RC6:       command.setProtocol(InfraredProtocolEnum::_RC6); break;
        case RC5X:      command.setProtocol(InfraredProtocolEnum::_RC5X); break;
        case DISH:      command.setProtocol(InfraredProtocolEnum::DISH_NETWORK); break;
        case SHARP:     command.setProtocol(InfraredProtocolEnum::_SHARP); break;
        case BOSE:      command.setProtocol(InfraredProtocolEnum::_BOSE); break;
        case XMP:       command.setProtocol(InfraredProtocolEnum::_XMP); break;
        case LG:        command.setProtocol(InfraredProtocolEnum::_LG); break;
        case SANYO_LC7461: command.setProtocol(InfraredProtocolEnum::_SANYO_LC7461); break;
        case LEGOPF:    command.setProtocol(InfraredProtocolEnum::LEGO); break;
        case MITSUBISHI: command.setProtocol(InfraredProtocolEnum::_MITSUBISHI); break;
        case PIONEER:    command.setProtocol(InfraredProtocolEnum::_PIONEER); break;

        default:
            return InfraredCommand();
    }

    uint32_t addr = _results.address;
    uint8_t dev = addr & 0xFF;
    uint8_t sub = (addr >> 8) & 0xFF;

    command.setDevice(dev);
    command.setSubdevice(sub == 0 ? -1 : sub);
    command.setFunction(_results.command & 0xFF);

    return command;
}

bool InfraredService::receiveRaw(std::vector<uint16_t>& timings, uint32_t& khz) {
    if (!_receiver) return false;

    if (!_receiver->decode(&_results)) return false;

    struct ResumeGuard {
        IRrecv* r;
        ~ResumeGuard(){ r->resume(); }
    } guard{_receiver};

    if (!_results.rawbuf || _results.rawlen <= 1) return false;

    khz = IR_DEFAULT_FREQUENCY_KHZ;
    timings.clear();
    timings.reserve(_results.rawlen - 1);

    for (uint16_t i = 1; i < _results.rawlen; i++) {
        uint32_t us = _results.rawbuf[i] * kRawTick;
        if (us > 0xFFFF) us = 0xFFFF;
        timings.push_back((uint16_t)us);
    }

    return true;
}

void InfraredService::sendJam(uint8_t modeIndex,
                              uint16_t khz,
                              uint32_t& sweepIndex,
                              uint8_t density) {
    if (density == 0) density = 1;

    static std::vector<uint16_t> pat;
    uint16_t halfPeriod = 500 / khz;
    if (halfPeriod < 1) halfPeriod = 1;

    if (pat.empty()) {
        for (int i = 0; i < 40; i++) {
            pat.push_back(halfPeriod);
            pat.push_back(halfPeriod);
        }
    }

    JamMode mode = (JamMode)modeIndex;
    uint8_t sweepCount = sizeof(carrierKhz) / sizeof(carrierKhz[0]);
    uint16_t delayUs = 100 / density + 2;

    for (uint8_t i = 0; i < density; i++) {
        uint16_t f = khz;

        if (mode == JamMode::SWEEP) {
            f = carrierKhz[sweepIndex++ % sweepCount];
            sendRaw(pat, f);
        } else if (mode == JamMode::CARRIER) {
            sendRaw(pat, khz);
        } else {
            std::vector<uint16_t> rnd;
            for (int j = 0; j < 30; j++) rnd.push_back(random(5, 1001));
            if (random(10) < 3) f = carrierKhz[random(0, sweepCount)];
            sendRaw(rnd, f);
        }

        delayMicroseconds(delayUs);
    }
}

uint16_t InfraredService::packAddress(uint8_t device, int subdevice) {
    const uint8_t sub = (subdevice < 0) ? 0 : (uint8_t)subdevice;
    return (uint16_t(sub) << 8) | device;
}

uint16_t InfraredService::getKaseikyoVendorIdCode(const std::string& input) {
    std::string s = input;
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    // IRremoteESP8266 manufacturer codes
    if (s.find("panasonic")   != std::string::npos) return 0x4004;
    if (s.find("denon")       != std::string::npos) return 0x2A4C;
    if (s.find("mitsubishi")  != std::string::npos) return 0xC4D3;
    if (s.find("sharp")       != std::string::npos) return 0x555A;
    if (s.find("jvc")         != std::string::npos) return 0xC080;

    // Default Panasonic
    return 0x4004;
}

std::vector<std::string> InfraredService::getCarrierStrings() {
    std::vector<std::string> out;
    for (uint16_t f : carrierKhz) out.push_back(std::to_string(f));
    return out;
}

std::vector<std::string> InfraredService::getJamModeStrings() {
    return {"carrier", "sweep", "random"};
}

#else

/*
====================
Arduino-IRemote
====================
*/

#include <IRremote.hpp>
#if defined(INFRARED_RMT)
#include <limits>
#if defined(DEVICE_STICKS3)
#include <M5Unified.h>
#endif
#include "Transformers/SubGhzTransformer.h"
#endif

#if defined(INFRARED_RMT)
bool InfraredService::armReceiver() {
    rxSymbolCount = rxSymbols.size();
    if (rmtReadAsync(rxPin, rxSymbols.data(), &rxSymbolCount))
        return true;
    log_e("Infrared: cannot arm RMT receiver on GPIO %u", rxPin);
    stopReceiver();
    return false;
}

InfraredCommand InfraredService::receiveRmtCommand() {

    std::vector<uint16_t> timings;
    uint32_t khz;

    if (!receiveRmtRaw(timings, khz) || timings.size() >= RAW_BUFFER_LENGTH)
        return InfraredCommand{};
    
    // Inject RMT timings into IrReceiver raw params and then call InfraredService::receiveInfraredCommand
    // to trigger command decoding
    if (!IrReceiver.decodedIRData.rawDataPtr) return InfraredCommand{};
    auto &irparams = *IrReceiver.decodedIRData.rawDataPtr;
    irparams.rawlen = timings.size() + 1;
    irparams.rawbuf[0] = 0;

    for (size_t i = 0; i < timings.size(); ++i)
    {
        const uint32_t ticks = (timings[i] + MICROS_PER_TICK / 2) / MICROS_PER_TICK;
        if (ticks > std::numeric_limits<IRRawbufType>::max())
            return InfraredCommand{};

        irparams.rawbuf[i + 1] = ticks;
    }

    irparams.OverflowFlag = false;
    irparams.initialGapTicks = initialGapTicks;
    irparams.StateForISR = IR_REC_STATE_STOP;
    IrReceiver.decodedIRData.rawlen = irparams.rawlen;
    IrReceiver.decodedIRData.initialGapTicks = initialGapTicks;
    return decodeIrCommand(true);
}

bool InfraredService::receiveRmtRaw(std::vector<uint16_t> &timings, uint32_t &khz) {
    if (!receiving || !rmtReceiveCompleted(rxPin))
        return false;

    // A full buffer may be truncated; never expose it as a complete frame.
    timings.clear();
    if (rxSymbolCount > 0 && rxSymbolCount < rxSymbols.size()) {
        std::vector<rmt_symbol_word_t> symbols;
        symbols.reserve(rxSymbolCount);
        for (size_t i = 0; i < rxSymbolCount; ++i) {
            const auto &raw = rxSymbols[i];
            if (!raw.duration0) break;
            rmt_symbol_word_t symbol{};
            symbol.duration0 = raw.duration0;
            symbol.level0 = raw.level0;
            symbol.duration1 = raw.duration1;
            symbol.level1 = raw.level1;
            symbols.push_back(symbol);
            if (!raw.duration1) break;
        }
        // RX runs at 1 MHz. IR marks are active-low; discard leading idle.
        for (int32_t timing : SubGhzTransformer().symbolsToSignedTimings(symbols, 1)) {
            const bool space = timing > 0;
            const uint32_t us = space ? timing : -timing;
            if (timings.empty() && space) continue;
            if (!timings.empty() && space == ((timings.size() - 1) % 2 != 0)) {
                const uint32_t merged = timings.back() + us;
                if (merged > UINT16_MAX) { timings.clear(); break; }
                timings.back() = merged;
            } else {
                timings.push_back(us);
            }
        }
        if (!timings.empty() && timings.size() % 2 == 0) timings.pop_back();
    }
    const bool valid = !timings.empty();

    if (valid) {
        uint32_t frameUs = 0;
        for (uint16_t us : timings)
            frameUs += us;
        const uint32_t endUs = micros() - 20000; // RMT's trailing idle timeout.
        const uint32_t elapsed = endUs - lastFrameEndUs;
        const uint32_t gapUs = elapsed > frameUs ? elapsed - frameUs : 0;
        initialGapTicks = std::min<uint32_t>(gapUs / MICROS_PER_TICK, UINT16_MAX);
        lastFrameEndUs = endUs;
        khz = InfraredService::IR_DEFAULT_FREQUENCY_KHZ; // Demodulated RX provides envelope timings, not carrier frequency.
    }
    armReceiver();
    return valid;
}
#endif

void InfraredService::configureTransmitter(uint8_t tx) {
    IrSender.begin(tx);
    // begin() only records the pin; initialize PWM at zero duty for TX idle.
    IrSender.enableIROut(IR_DEFAULT_FREQUENCY_KHZ);
    IrSender.IRLedOff();
}

void InfraredService::configure(uint8_t tx, uint8_t rx) {
#if defined(INFRARED_RMT)
    stopReceiver();
    rxPin = rx;
    configureTransmitter(tx);
#else
    configureTransmitter(tx);
    IrReceiver.begin(rx, ENABLE_LED_FEEDBACK);
#endif
}

void InfraredService::startReceiver() {
#if defined(INFRARED_RMT)
    if (receiving || rxPin == 0xFF) return;
#if defined(DEVICE_STICKS3)
    restoreSpeaker = M5.Speaker.isRunning();
    M5.Speaker.end();
#endif
    IrSender.IRLedOff();
    receiving = rmtInit(rxPin, RMT_RX_MODE, RMT_MEM_NUM_BLOCKS_3, 1000000);
    if (!receiving || !rmtSetRxMinThreshold(rxPin, 1) || !rmtSetRxMaxThreshold(rxPin, 20000)) {
        log_e("Infrared: cannot configure RMT receiver on GPIO %u", rxPin);
        stopReceiver();
        return;
    }
    lastFrameEndUs = micros();
    auto *raw = IrReceiver.decodedIRData.rawDataPtr;
    IrReceiver.decodedIRData = {};
    IrReceiver.decodedIRData.rawDataPtr = raw;
    armReceiver();
#else
    IrReceiver.start();
#endif
}

void InfraredService::stopReceiver() {
#if defined(INFRARED_RMT)
    if (receiving) rmtDeinit(rxPin);
    receiving = false;
#if defined(DEVICE_STICKS3)
    if (restoreSpeaker) M5.Speaker.begin();
    restoreSpeaker = false;
#endif
#else
    IrReceiver.stop();
#endif
}

void InfraredService::sendInfraredCommand(InfraredCommand command) {
    uint16_t vendorCode;
    std::string protocolString = InfraredProtocolMapper::toString(command.getProtocol());
    uint8_t device = command.getDevice();
    uint8_t subdevice = command.getSubdevice() == -1 ? 0 : command.getSubdevice();

    // Combine device and subdevice into a 16-bit integer
    uint16_t address = (static_cast<uint16_t>(subdevice) << 8) | device;

    switch (command.getProtocol()) {
        case InfraredProtocolEnum::_SAMSUNG:
        case InfraredProtocolEnum::SAMSUNG20: {
            IrSender.sendSamsung(address, command.getFunction(), 0);
            break;
        }

        case InfraredProtocolEnum::_PANASONIC:
        case InfraredProtocolEnum::PANASONIC2: {
            // Panasonic can be used by many manufacturers in the IRDB format, we check for vendor name
            vendorCode = getKaseikyoVendorIdCode(protocolString);
            
            IrSender.sendKaseikyo(address, command.getFunction(), 0, vendorCode);
            break;
        }

        // Same for Sony20
        case InfraredProtocolEnum::SONY20: {
            IrSender.sendSony(address, command.getFunction(), 0, SIRCS_20_PROTOCOL);  
            break;
        }

        // LEGO
        case InfraredProtocolEnum::LEGO: {
            IrSender.sendLegoPowerFunctions(address, command.getFunction(), 0);  
            break;
        }
        
        // Handle by MakeHex
        default: {
            int frequency = IR_DEFAULT_FREQUENCY_KHZ; // Default frequency, passed by reference to encodeRemoteCommand
            std::vector<float> sequence = encodeRemoteCommand(command, protocolString.c_str(), frequency);

            // Convert for sendRaw
            uint16_t* raw = new uint16_t[sequence.size()];
            for (size_t i = 0; i < sequence.size(); ++i) {
                raw[i] = static_cast<uint16_t>(sequence[i]);
            }
            
            // Send the raw generated sequence with the correct frequency
            IrSender.sendRaw(raw, sequence.size(), frequency);
            delete[] raw;
        }
    }
}

void InfraredService::sendInfraredFileCommand(InfraredFileRemoteCommand command) {

    if (command.protocol == InfraredProtocolEnum::_RAW) {
        IrSender.sendRaw(command.rawData, command.rawDataSize, command.frequency);
        return;
    }

    uint8_t device = static_cast<uint8_t>(command.address & 0xFF);
    uint8_t sub = static_cast<uint8_t>((command.address >> 8) & 0xFF);
    auto finalCmd = InfraredCommand(
        command.protocol,
        device,
            sub,
            command.function
        );

    sendInfraredCommand(finalCmd);
}

InfraredCommand InfraredService::receiveInfraredCommand() {
#if defined(INFRARED_RMT)
    return receiveRmtCommand();
#else
    return decodeIrCommand(false);
#endif
}

InfraredCommand InfraredService::decodeIrCommand(bool injected) {
    (void)injected;
    InfraredCommand command;

    if (!IrReceiver.decode()) {
        return command;
    }

    const IRData& data = IrReceiver.decodedIRData;
    IrReceiver.resume();

    // Ignore invalid
    if (data.numberOfBits == 0 || data.protocol == UNKNOWN) {
        return command;
    }

    // Mapping protocols
    switch (data.protocol) {
        case NEC:
            command.setProtocol(_NEC);
            break;
        case NEC2:
        case ONKYO:
            command.setProtocol(_NEC2);
            break;

        case DENON:
            command.setProtocol(_DENON);
            break;

        case APPLE:
            command.setProtocol(_APPLE);
            break;

        case SONY:
            command.setProtocol(SONY20);
            break;

        case JVC:
            command.setProtocol(_JVC);
            break;

        case RC5:
            command.setProtocol(_RC5);
            break;

        case BOSEWAVE:
        case BANG_OLUFSEN:
            command.setProtocol(_BOSE);
            break;

        case RC6:
            command.setProtocol(_RC6);
            break;
        case LG:
        case LG2:
        case SAMSUNG:
            command.setProtocol(SAMSUNG20);
            break;

        case SAMSUNGLG:
        case SAMSUNG48:
            command.setProtocol(_SAMSUNG);
            break;

        case PANASONIC:
            command.setProtocol(_PANASONIC);
            break;

        case KASEIKYO_DENON:
            command.setProtocol(DENON_K);
            break;

        case KASEIKYO:
        case KASEIKYO_SHARP:
        case KASEIKYO_JVC:
        case KASEIKYO_MITSUBISHI:
            command.setProtocol(_KASEIKYO);
            break;

        case LEGO_PF:
            command.setProtocol(LEGO);
            break;

        default:
            return InfraredCommand();
    }

    // Panasonic and derived
    if (data.protocol == PANASONIC || data.protocol == KASEIKYO || data.protocol == KASEIKYO_JVC ||
        data.protocol == KASEIKYO_SHARP || data.protocol == KASEIKYO_MITSUBISHI || data.protocol == KASEIKYO_DENON) {
        // PANASONIC: 16 bits = 8b subdevice + 8b device
        command.setDevice((data.address >> 8) & 0xFF);
        command.setSubdevice(data.address & 0xFF);
        
    // Samsung
    } else if (data.protocol == SAMSUNG || data.protocol == SAMSUNGLG) {
        command.setDevice(data.address & 0xFF);
        command.setSubdevice((data.address >> 8) & 0xFF);

    } else {
        // que l'adresse
        int device = data.address & 0xFF;
        int subdevice = (data.address >> 8) & 0xFF;

        // Si subdevice est 0, ignore
        command.setDevice(device);
        command.setSubdevice(subdevice == 0 ? -1 : subdevice);
    }

    // Commande principale
    command.setFunction(data.command);

    return command;
}

bool InfraredService::receiveRaw(std::vector<uint16_t>& timings, uint32_t& khz) {
#if defined(INFRARED_RMT)
    return receiveRmtRaw(timings, khz);
#else
    if (!IrReceiver.decode()) {
        return false;
    }

    const IRData& data = IrReceiver.decodedIRData;
    const auto* raw = data.rawDataPtr;

    // Ensure we resume after processing
    struct ResumeGuard {
        ~ResumeGuard(){ IrReceiver.resume(); }
    } guard;

    if (raw == nullptr || raw->rawlen <= 1) {
        return false;
    }

    khz = IR_DEFAULT_FREQUENCY_KHZ; // TODO: default frequency, handle that ?

    timings.clear();
    timings.reserve(raw->rawlen - 1);

    for (uint16_t i = 1; i < raw->rawlen; i++) {
        // Convert ticks to microseconds
        uint32_t us = static_cast<uint32_t>(raw->rawbuf[i]) * MICROS_PER_TICK;
        timings.push_back(static_cast<uint16_t>(us));
    }

    return true;
#endif
}

void InfraredService::sendRaw(const std::vector<uint16_t>& timings, uint32_t khz) {
    if (timings.empty()) return;

    uint16_t* buf = new uint16_t[timings.size()];
    for (size_t i = 0; i < timings.size(); ++i) buf[i] = timings[i];

    IrSender.sendRaw(buf, timings.size(), khz);
    delete[] buf;
}

void InfraredService::sendJam(uint8_t modeIndex,
                                 uint16_t khz,
                                 uint32_t& sweepIndex,
                                 uint8_t density) {

    // Burst pattern
    static std::vector<uint16_t> pat;
    uint16_t halfPeriodUs = 500 / khz;
    if (halfPeriodUs < 1) halfPeriodUs = 1;

    if (pat.empty()) {
        pat.reserve(80);
        for (int i = 0; i < 40; ++i) {
            pat.push_back(halfPeriodUs);
            pat.push_back(halfPeriodUs);
        }
    }

    auto mode = static_cast<JamMode>(modeIndex);

    // Delay shrinks as density grows
    const uint16_t interBurstDelayUs = 100 / density + 2;
    const uint8_t sweepCount = sizeof(carrierKhz) / sizeof(carrierKhz[0]);

    for (uint8_t b = 0; b < density; ++b) {

        uint16_t f = khz;

        if (mode == JamMode::SWEEP) {
            f = carrierKhz[sweepIndex++ % sweepCount];
            sendRaw(pat, f);
        }
        else if (mode == JamMode::CARRIER) {
            sendRaw(pat, khz);
        }
        else { // RANDOM
            std::vector<uint16_t> rnd;
            rnd.reserve(30);
            for (int i = 0; i < 30; ++i) {
                rnd.push_back((uint16_t)random(5, 1001));
            }

            // Occasionally vary frequency
            if (random(10) < 3) {
                f = carrierKhz[random(0, sweepCount)];
            }

            sendRaw(rnd, f);
        }

        delayMicroseconds(interBurstDelayUs);
    }
}

uint16_t InfraredService::getKaseikyoVendorIdCode(const std::string& input) {
    std::string lowerInput = input;
    std::transform(lowerInput.begin(), lowerInput.end(), lowerInput.begin(), ::tolower);

    if (lowerInput.find("panasonic") != std::string::npos) {
        return PANASONIC_VENDOR_ID_CODE;
    } else if (lowerInput.find("denon") != std::string::npos) {
        return DENON_VENDOR_ID_CODE;
    } else if (lowerInput.find("mitsubishi") != std::string::npos) {
        return MITSUBISHI_VENDOR_ID_CODE;
    } else if (lowerInput.find("sharp") != std::string::npos) {
        return SHARP_VENDOR_ID_CODE;
    } else if (lowerInput.find("jvc") != std::string::npos) {
        return JVC_VENDOR_ID_CODE;
    } else {
        return PANASONIC_VENDOR_ID_CODE; // default
    }
}

std::vector<std::string> InfraredService::getCarrierStrings() {
    std::vector<std::string> out;
    out.reserve(sizeof(carrierKhz) / sizeof(carrierKhz[0]));

    for (uint16_t f : carrierKhz) {
        out.push_back(std::to_string(f));
    }
    return out;
}

std::vector<std::string> InfraredService::getJamModeStrings() {
    return {"carrier", "sweep", "random"};
}

#endif
