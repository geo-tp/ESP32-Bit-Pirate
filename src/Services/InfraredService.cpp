#include "InfraredService.h"

#include <algorithm>
#include <Vendors/MakeHex.h>
#include "Transformers/InfraredCommandLimits.h"
#include "Transformers/InfraredRemoteTransformer.h"
#include <cmath>
#include <limits>

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

bool InfraredService::sendInfraredCommand(InfraredCommand command) {
    if (!_sender || !InfraredCommandLimits::valid(command)) return false;

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
            return true;
        }

        case InfraredProtocolEnum::_SAMSUNG: {
            if (subdevice != -1) break;

            uint32_t frame = _sender->encodeSAMSUNG(device, func);
            _sender->sendSAMSUNG(frame);
            return true;
        }

        case InfraredProtocolEnum::_LG: {
            if (subdevice != -1) break;

            uint32_t frame = _sender->encodeLG(addr16, func);
            _sender->sendLG(frame);
            return true;
        }

        case InfraredProtocolEnum::_SANYO_LC7461: {
            uint32_t frame = _sender->encodeSanyoLC7461(addr16, func);
            _sender->sendSanyoLC7461(frame);
            return true;
        }

        case InfraredProtocolEnum::_PANASONIC:
        case InfraredProtocolEnum::PANASONIC_: {
            uint16_t vendor = getKaseikyoVendorIdCode(protoStr);
            uint8_t sub = (subdevice < 0) ? 0 : (uint8_t)subdevice;

            uint64_t frame = _sender->encodePanasonic(vendor, device, sub, func);
            _sender->sendPanasonic64(frame);
            return true;
        }

        case InfraredProtocolEnum::_PIONEER: {
            uint64_t frame = _sender->encodePioneer(addr16, func);
            _sender->sendPioneer(frame);
            return true;
        }

        default:
            break;
    }

    return sendMakeHexCommand(command);
}

void InfraredService::sendRaw(const std::vector<uint16_t>& timings, uint32_t khz) {
    if (!_sender || timings.empty()) return;
    _sender->sendRaw(timings.data(), timings.size(), khz);
}

InfraredCommand InfraredService::receiveInfraredCommand() {
    InfraredCommand command;
    if (!_receiver || !_receiver->decode(&_results)) return command;

    struct ResumeGuard {
        IRrecv* receiver;
        ~ResumeGuard() { receiver->resume(); }
    } guard{_receiver};

    if (_results.overflow || _results.decode_type == UNKNOWN) return command;

    // IRremoteESP8266 can identify many more protocols than our legacy
    // address/subdevice/function schema can express, including A/C states.
    // Preserve the native name, complete <=64-bit code, repeat flag, and
    // bounded extended state instead of silently discarding the decode.
    const auto nativeName = typeToString(_results.decode_type);
    command.setNativeDecode(nativeName.c_str(), _results.bits,
                            _results.bits <= 64 ? _results.value : 0,
                            _results.repeat);
    if (_results.bits > 64) {
        const size_t bytes = (_results.bits + 7u) / 8u;
        const size_t available = std::min<size_t>(bytes, sizeof(_results.state));
        command.setDecodedState(_results.state, available);
        if (available < bytes) command.markDecodedStateTruncated();
    }
    // Short repeat frames contain no new address/function fields.
    if (_results.bits == 0) {
        command.setProtocol(_IR_UNKNOWN);
        command.setDevice(-1);
        command.setSubdevice(-1);
        command.setFunction(-1);
        return command;
    }

    // Apple does not use the NEC complement/check fields. On this backend
    // it can arrive as NEC or NEC_LIKE; inspect the original wire payload.
    if ((_results.decode_type == NEC || _results.decode_type == NEC_LIKE) &&
        _results.bits == 32 &&
        InfraredRemoteTransformer::decodeAppleEitherBitOrder(
            static_cast<uint32_t>(_results.value), command)) return command;

    // Preserve the Sony frame length (12/15/20) and its extended address.
    if (_results.decode_type == SONY &&
        (_results.bits == 12 || _results.bits == 15 || _results.bits == 20)) {
        command.setProtocol(_results.bits == 20 ? SIRC20 :
                            (_results.bits == 15 ? SIRC15 : SIRC));
        command.setDevice(_results.address & (_results.bits == 20 ? 31 : 255));
        command.setSubdevice(_results.bits == 20 ?
                             (_results.command >> 7) & 255 : -1);
        command.setFunction(_results.command & 127);
        return command;
    }

    switch (_results.decode_type) {
        case NEC: command.setProtocol(_NEC); break;
        case PANASONIC: command.setProtocol(_PANASONIC); break;
        case JVC: command.setProtocol(_JVC); break;
        case SAMSUNG: command.setProtocol(SAMSUNG32); break;
        case SAMSUNG36: command.setProtocol(_SAMSUNG36); break;
        case DENON: command.setProtocol(_DENON); break;
        case RC5: command.setProtocol(_RC5); break;
        case RC6: command.setProtocol(_RC6); break;
        case RC5X: command.setProtocol(_RC5X); break;
        case DISH: command.setProtocol(DISH_NETWORK); break;
        case SHARP: command.setProtocol(_SHARP); break;
        case BOSE: command.setProtocol(_BOSE); break;
        case XMP: command.setProtocol(_XMP); break;
        case LG: command.setProtocol(_LG); break;
        case SANYO_LC7461: command.setProtocol(_SANYO_LC7461); break;
        case LEGOPF: command.setProtocol(LEGO); break;
        case MITSUBISHI: command.setProtocol(_MITSUBISHI); break;
        case PIONEER: command.setProtocol(_PIONEER); break;
        case GICABLE: command.setProtocol(G_I_CABLE); break;
        // NEC_LIKE, A/C states and all other supported library protocols:
        // identification and raw data remain available via native metadata.
        default: break;
    }

    if (command.getProtocol() == _RAW || command.getProtocol() == _IR_UNKNOWN) {
        command.setProtocol(_IR_UNKNOWN);
        command.setDevice(-1);
        command.setSubdevice(-1);
        command.setFunction(-1);
        return command;
    }
    // The library's address/command fields are wider than InfraredCommand.
    // Never silently narrow them into bogus values.
    if (_results.address > 0x7FFFu || _results.command > 0x7FFFu) {
        command.setProtocol(_IR_UNKNOWN);
        command.setDevice(-1);
        command.setSubdevice(-1);
        command.setFunction(-1);
        return command;
    }
    const uint32_t address = _results.address;
    command.setDevice(address & 0xFFu);
    const uint32_t sub = (address >> 8u) & 0xFFu;
    command.setSubdevice(sub == 0 ? -1 : sub);
    command.setFunction(_results.command);
    return command;
}

bool InfraredService::receiveRaw(std::vector<uint16_t>& timings, uint32_t& khz) {
    if (!_receiver) return false;

    if (!_receiver->decode(&_results)) return false;

    struct ResumeGuard {
        IRrecv* r;
        ~ResumeGuard(){ r->resume(); }
    } guard{_receiver};

    timings.clear();
    if (_results.overflow || !_results.rawbuf || _results.rawlen <= 1) return false;

    khz = IR_DEFAULT_FREQUENCY_KHZ;
    timings.reserve(_results.rawlen - 1);

    for (uint16_t i = 1; i < _results.rawlen; i++) {
        const uint32_t us = static_cast<uint32_t>(_results.rawbuf[i]) * kRawTick;
        if (!InfraredRemoteTransformer::appendRawTiming(timings, i % 2 == 1, us)) return false;
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
        timings.reserve(rxSymbolCount * 2);
        for (size_t i = 0; i < rxSymbolCount; ++i) {
            const auto &raw = rxSymbols[i];
            if (!raw.duration0) break;
            // RX is 1 MHz and active-low. Merge adjacent levels in-place, with
            // no intermediate symbol/signed-timing buffers or narrowing overflow.
            if (!InfraredRemoteTransformer::appendRawTiming(timings, !raw.level0, raw.duration0)) break;
            if (!raw.duration1) break;
            if (!InfraredRemoteTransformer::appendRawTiming(timings, !raw.level1, raw.duration1)) break;
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
    _configured = true;
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

bool InfraredService::sendInfraredCommand(InfraredCommand command) {
    if (!_configured || !InfraredCommandLimits::valid(command)) return false;
    uint16_t vendorCode;
    std::string protocolString = InfraredProtocolMapper::toString(command.getProtocol());
    uint8_t device = command.getDevice();
    uint8_t subdevice = command.getSubdevice() == -1 ? 0 : command.getSubdevice();

    // Combine device and subdevice into a 16-bit integer
    uint16_t address = (static_cast<uint16_t>(subdevice) << 8) | device;

    switch (command.getProtocol()) {
        case InfraredProtocolEnum::_SAMSUNG: {
            IrSender.sendSamsung(address, command.getFunction(), 0);
            break;
        }

        case InfraredProtocolEnum::_PANASONIC: {
            // Panasonic can be used by many manufacturers in the IRDB format, we check for vendor name
            vendorCode = getKaseikyoVendorIdCode(protocolString);
            
            IrSender.sendKaseikyo(address, command.getFunction(), 0, vendorCode);
            break;
        }

        // LEGO
        case InfraredProtocolEnum::LEGO: {
            IrSender.sendLegoPowerFunctions(address, command.getFunction(), 0);  
            break;
        }
        
        default:
            return sendMakeHexCommand(command);
    }
    return true;
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
    if (!IrReceiver.decode()) return command;

    const IRData& data = IrReceiver.decodedIRData;
    struct ResumeGuard {
        ~ResumeGuard() { IrReceiver.resume(); }
    } guard;
    if (data.protocol == UNKNOWN ||
        (data.flags & IRDATA_FLAGS_WAS_OVERFLOW)) return command;

    command.setNativeDecode(getProtocolString(data.protocol),
                            data.numberOfBits,
                            data.numberOfBits <= 64 ?
                                static_cast<uint64_t>(data.decodedRawData) : 0,
                            (data.flags & IRDATA_FLAGS_IS_REPEAT) != 0);

    if (data.numberOfBits == 0) {
        command.setProtocol(_IR_UNKNOWN);
        command.setDevice(-1);
        command.setSubdevice(-1);
        command.setFunction(-1);
        return command;
    }

    // Apple has a fixed 0x87 identifier and non-NEC function bits.
    // Never pass the generic IRremote address/command values to MakeHex.
    if ((data.protocol == APPLE || data.protocol == NEC ||
         data.protocol == ONKYO) && data.numberOfBits == 32 &&
        InfraredRemoteTransformer::decodeAppleEitherBitOrder(
            static_cast<uint32_t>(data.decodedRawData), command)) return command;

    if (data.protocol == SONY &&
        (data.numberOfBits == 12 || data.numberOfBits == 15 ||
         data.numberOfBits == 20)) {
        command.setProtocol(data.numberOfBits == 20 ? SIRC20 :
                            (data.numberOfBits == 15 ? SIRC15 : SIRC));
        command.setDevice(data.numberOfBits == 20 ? data.address & 31 : data.address);
        command.setSubdevice(data.numberOfBits == 20 ?
                             (data.address >> 5) & 255 : -1);
        command.setFunction(data.command & 127);
        return command;
    }

    switch (data.protocol) {
        case NEC: command.setProtocol(_NEC); break;
        case NEC2: command.setProtocol(_NEC2); break;
        // Onkyo's 16-bit command is not a normal NEC8 command.
        case ONKYO: command.setProtocol(_IR_UNKNOWN); break;
        case DENON: command.setProtocol(_DENON); break;
        case JVC: command.setProtocol(_JVC); break;
        case RC5: command.setProtocol(_RC5); break;
        case RC6: command.setProtocol(_RC6); break;
        case LG: command.setProtocol(_LG); break;
        case SAMSUNG: command.setProtocol(SAMSUNG32); break;
        case SAMSUNG48: command.setProtocol(_SAMSUNG); break;
        case PANASONIC: command.setProtocol(_PANASONIC); break;
        case KASEIKYO_DENON: command.setProtocol(DENON_K); break;
        case KASEIKYO: command.setProtocol(_KASEIKYO); break;
        case LEGO_PF: command.setProtocol(LEGO); break;
        // APPLE (if not validated), RC6A, SAMSUNGLG, Kaseikyo vendor
        // variants, FAST, MAGIQUEST, MARANTZ, BOSEWAVE, BANG_OLUFSEN,
        // PULSE_DISTANCE, PULSE_WIDTH and OTHER remain labelled by the
        // library without claiming a MakeHex-compatible address/function.
        default: command.setProtocol(_IR_UNKNOWN); break;
    }

    if (command.getProtocol() == _IR_UNKNOWN ||
        data.address > 0x7FFFu || data.command > 0x7FFFu) {
        command.setProtocol(_IR_UNKNOWN);
        command.setDevice(-1);
        command.setSubdevice(-1);
        command.setFunction(-1);
        return command;
    }
    if (data.protocol == PANASONIC || data.protocol == KASEIKYO ||
        data.protocol == KASEIKYO_DENON) {
        command.setDevice((data.address >> 8) & 0xFF);
        command.setSubdevice(data.address & 0xFF);
    } else if (data.protocol == SAMSUNG) {
        command.setDevice(data.address & 0xFF);
        command.setSubdevice((data.address >> 8) & 0xFF);
    } else {
        command.setDevice(data.address & 0xFF);
        const int sub = (data.address >> 8) & 0xFF;
        command.setSubdevice(sub == 0 ? -1 : sub);
    }
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

    timings.clear();
    if (raw == nullptr || raw->rawlen <= 1 || raw->OverflowFlag) {
        return false;
    }

    khz = IR_DEFAULT_FREQUENCY_KHZ; // Demodulated RX cannot measure the carrier.

    timings.reserve(raw->rawlen - 1);

    for (uint16_t i = 1; i < raw->rawlen; i++) {
        // Convert ticks to microseconds
        uint32_t us = static_cast<uint32_t>(raw->rawbuf[i]) * MICROS_PER_TICK;
        if (!InfraredRemoteTransformer::appendRawTiming(timings, i % 2 == 1, us)) return false;
    }

    return true;
#endif
}

void InfraredService::sendRaw(const std::vector<uint16_t>& timings, uint32_t khz) {
    if (!_configured || timings.empty() || timings.size() > UINT16_MAX || khz == 0 || khz > 255) return;
    IrSender.sendRaw(timings.data(), timings.size(), khz);
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

bool InfraredService::sendInfraredFileCommand(const InfraredFileRemoteCommand& command) {
    if (!command.error.empty()) return false;
    if (command.legacyDsf) {
        if (command.address > 65535 || command.function > 255) return false;
        return sendInfraredCommand(InfraredCommand(command.protocol, command.address & 255,
                                                   command.address >> 8, command.function));
    }
    if (command.fileProtocol == "raw") {
        if (command.frequency < 10000 || command.frequency > 1000000 ||
            !std::isfinite(command.dutyCycle) || command.dutyCycle <= 0 || command.dutyCycle > 1)
            return false;
        // RAW is already an owned waveform; do not duplicate it for transmission.
        const unsigned duty = std::clamp(static_cast<unsigned>(std::lround(command.dutyCycle * 100)), 1u, 100u);
        return sendWaveform(command.rawData, command.frequency, duty);
    }
    const auto wave = InfraredRemoteTransformer::fromFileCommand(command, fileToggle);
    const bool sent = sendWaveform(wave.pulses, wave.frequencyHz, wave.dutyPercent);
    if (sent && (command.fileProtocol == "RC5" || command.fileProtocol == "RC5X" ||
                 command.fileProtocol == "RC6")) fileToggle = !fileToggle;
    return sent;
}

bool InfraredService::sendMakeHexCommand(const InfraredCommand& command) {
    int frequencyHz = 38000;
    const auto name = InfraredProtocolMapper::toString(command.getProtocol());
    const auto timings = encodeRemoteCommand(command, name.c_str(), frequencyHz);
    return frequencyHz > 0 && sendRawSequence(timings, static_cast<uint32_t>(frequencyHz));
}

bool InfraredService::sendRawSequence(const std::vector<float>& timings, uint32_t frequencyHz) {
    if (timings.empty() || timings.size() % 2 != 0) return false;
    std::vector<uint32_t> pulses;
    pulses.reserve(timings.size());
    for (float duration : timings) {
        if (!std::isfinite(duration) || duration < 1 ||
            static_cast<double>(duration) > UINT32_MAX) return false;
        pulses.push_back(static_cast<uint32_t>(duration));
    }
    return sendWaveform(pulses, frequencyHz);
}

bool InfraredService::sendWaveform(const std::vector<uint32_t>& timings, uint32_t frequencyHz, unsigned dutyPercent) {
#ifdef INFRARED_IREMOTE_ESP8266
    if (!_sender) return false;
#else
    if (!_configured || (frequencyHz + 500ULL) / 1000 > 255) return false;
#endif
    if (timings.empty() || timings.size() > 65535 || frequencyHz < 1000 ||
        frequencyHz > 1000000 || dutyPercent < 1 || dutyPercent > 100) return false;
    uint64_t total = 0;
    for (size_t i = 0; i < timings.size(); ++i) {
        // mark() accepts 16 bits on both backends. Validate the entire frame
        // before emitting anything; spaces can be split without extra marks.
        if (!timings[i] || (i % 2 == 0 && timings[i] > UINT16_MAX)) return false;
        total += timings[i];
        if (total > 10000000) return false;
    }
#ifdef INFRARED_IREMOTE_ESP8266
    auto& sender = *_sender;
    sender.enableIROut(frequencyHz, dutyPercent);
#else
    auto& sender = IrSender;
    // Arduino-IRremote exposes integer kHz and a compile-time duty cycle.
    sender.enableIROut((frequencyHz + 500) / 1000);
#endif
    for (size_t i = 0; i < timings.size(); ++i) {
        if (i % 2 == 0) sender.mark(static_cast<uint16_t>(timings[i]));
        else {
            uint32_t remaining = timings[i];
            while (remaining) {
                const uint16_t chunk = static_cast<uint16_t>(std::min<uint32_t>(remaining, UINT16_MAX));
                sender.space(chunk);
                remaining -= chunk;
            }
        }
    }
#ifdef INFRARED_IREMOTE_ESP8266
    sender.space(0); // Public API also switches the LED off at 100% duty.
#else
    sender.IRLedOff();
#endif
    return true;
}
