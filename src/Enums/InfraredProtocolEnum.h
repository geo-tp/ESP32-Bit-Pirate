#pragma once

#include <string>

typedef enum InfraredProtocolEnum {
    _48_NEC1 = 0,
    _48_NEC2 = 1,
    AIWA = 2,
    AKAI = 3,
    _APPLE = 4,
    BARCO = 5,
    BLAUPUNKT = 6,
    _BOSE = 7,
    CANALSAT = 8,
    _DENON = 9,
    DENON_K = 10,
    DENON_1_ = 11,
    DENON_2_ = 12,
    DGTEC = 13,
    DIRECTV = 14,
    DISH_NETWORK = 15,
    DISHPLAYER = 16,
    EMERSON = 17,
    F12 = 18,
    FUJITSU = 19,
    G_I_4DTV = 20,
    G_I_CABLE = 21,
    GXB = 22,
    GRUNDIG16 = 23,
    GRUNDIG16_30 = 24,
    _JVC = 25,
    JVC_48 = 26,
    JVC_2_ = 27,
    JERROLD = 28,
    KATHREIN = 29,
    KONKA = 30,
    LOGITECH = 31,
    LUMAGEN = 32,
    MCE = 33,
    _MITSUBISHI = 34,
    _NEC = 35,
    NEC1 = 36,
    NEC1_F16 = 37,
    NEC1_RNC = 38,
    NEC1_Y1 = 39,
    NEC1_Y2 = 40,
    NEC1_Y3 = 41,
    _NEC2 = 42,
    NEC2_F16 = 43,
    NECX1 = 44,
    NECX2 = 45,
    NRC16 = 46,
    NRC17 = 47,
    NOKIA = 48,
    NOKIA12 = 49,
    NOKIA32 = 50,
    PACEMSS = 51,
    _PANASONIC = 52,
    PANASONIC2 = 53,
    PANASONIC_OLD = 54,
    _PIONEER = 55,
    PROTON = 56,
    _RC5 = 57,
    RC5_7F = 58,
    _RC6 = 59,
    RC6_6_20 = 60,
    RCA = 61,
    RCA_OLD_ = 62,
    RCA_38 = 63,
    RCA_38_OLD_ = 64,
    RECS80 = 65,
    REPLAY = 66,
    SAMSUNG20 = 67,
    _SAMSUNG36 = 68,
    SCATL_6 = 69,
    SEJIN_1_38 = 70,
    SEJIN_1_56 = 71,
    _SHARP = 72,
    SHARPDVD = 73,
    SHARP_1_ = 74,
    SHARP_2_ = 75,
    SONY12 = 76,
    SONY15 = 77,
    SONY20 = 78,
    STREAMZAP = 79,
    TEAC_K = 80,
    THOMSON = 81,
    THOMSON7 = 82,
    TIVO_UNIT_0 = 83,
    TIVO_UNIT_1 = 84,
    TIVO_UNIT_2 = 85,
    TIVO_UNIT_3 = 86,
    TIVO_UNIT_4 = 87,
    TIVO_UNIT_5 = 88,
    TIVO_UNIT_6 = 89,
    TIVO_UNIT_7 = 90,
    TIVO_UNIT_8 = 91,
    TIVO_UNIT_9 = 92,
    VELLEMAN = 93,
    _XMP = 94,
    XMP_1 = 95,
    XMP_2 = 96,
    ZAPTOR_56 = 97,
    ZENITH = 98,
    DISH_NETWO = 99,
    PANASONIC_ = 100,
    TIVO_UNIT = 101,
    RCA_38_OLD = 102,
    _KASEIKYO = 103,
    _SAMSUNG = 104,
    NECext = 105,
    SIRC = 106,
    SIRC15 = 107,
    SIRC20 = 108,
    NEC42 = 109,
    NEC42ext = 110,
    _RC5X = 111,
    SAMSUNG32 = 112,
    LEGO = 113,
    _LG = 114,
    _SANYO_LC7461 = 115,
    _RAW = 116,
    _IR_UNKNOWN = 117,
} InfraredProtocolEnum;

class InfraredProtocolMapper {
public:
    static const std::string toString(InfraredProtocolEnum protocol) {
        switch (protocol) {
            case _48_NEC1: return "nec1";
            case _48_NEC2: return "nec2";
            case AIWA: return "aiwa";
            case AKAI: return "akai";
            case _APPLE: return "apple";
            case BARCO: return "barco";
            case BLAUPUNKT: return "blaupunkt";
            case _BOSE: return "bose";
            case CANALSAT: return "canalsat";
            case _DENON: return "denon";
            case DENON_K: return "Denon-K";
            case DENON_1_: return "denon";
            case DENON_2_: return "denon";
            case DGTEC: return "Dgtec";
            case DIRECTV: return "directv";
            case DISH_NETWORK:
            case DISHPLAYER:
            case DISH_NETWO: return "DishPlayer_Network";
            case EMERSON: return "emerson";
            case F12: return "f12";
            case FUJITSU: return "fujitsu";
            case G_I_4DTV: return "GI4dtv";
            case G_I_CABLE: return "GI_cable";
            case GXB: return "gxb";
            case GRUNDIG16: return "grundig16";
            case GRUNDIG16_30: return "grundig16_30";
            case _JVC:
            case JVC_48: return "jvc";
            case JVC_2_: return "jvc_two_frames";
            case JERROLD: return "Jerrold";
            case KATHREIN: return "kathrein";
            case KONKA: return "konka";
            case LOGITECH: return "logitech";
            case LUMAGEN: return "lumagen";
            case MCE: return "mce";
            case _MITSUBISHI: return "Mitsubishi";
            case _NEC:
            case NEC1: return "nec1";
            case NEC1_F16: return "nec1_f16";
            case NEC1_RNC: return "nec1_rnc";
            case NEC1_Y1: return "nec1_y1";
            case NEC1_Y2: return "nec1_y2";
            case NEC1_Y3: return "nec1_y3";
            case _NEC2: return "nec2";
            case NEC2_F16: return "nec2_f16";
            case NECX1: return "NECx1";
            case NECX2:
            case NECext: return "NECx2";
            case NEC42: return "NEC42";
            case NEC42ext: return "NEC42ext";
            case NRC16: return "nrc16";
            case NRC17: return "nrc17";
            case NOKIA:
            case NOKIA32: return "Nokia32";
            case NOKIA12: return "Nokia32single";
            case PACEMSS: return "pacemss";
            case _PANASONIC:
            case PANASONIC_: return "panasonic";
            case PANASONIC2: return "panasonic2";
            case PANASONIC_OLD: return "panasonic_old";
            case _PIONEER: return "pioneer";
            case PROTON: return "Proton";
            case _RC5: return "rc5";
            case RC5_7F: return "rc5odd";
            case _RC5X: return "rc5x";
            case _RC6: return "rc6";
            case RC6_6_20: return "rc6-6-20";
            case RCA:
            case RCA_OLD_:
            case RCA_38:
            case RCA_38_OLD_:
            case RCA_38_OLD: return "rca";
            case RECS80: return "recs80_45";
            case REPLAY: return "replay";
            case SAMSUNG20: return "Samsung20";
            case SAMSUNG32: return "Samsung32";
            case _SAMSUNG36: return "Samsung36";
            case SCATL_6: return "scatl_6";
            case SEJIN_1_38: return "sejin_1_38";
            case SEJIN_1_56: return "sejin_1_56";
            case _SHARP:
            case SHARPDVD:
            case SHARP_1_:
            case SHARP_2_: return "sharp";
            case SIRC: return "SIRC";
            case SIRC15: return "SIRC15";
            case SIRC20: return "SIRC20";
            case SONY12: return "sony12";
            case SONY15: return "sony15";
            case SONY20: return "sony20";
            case STREAMZAP: return "streamzap";
            case TEAC_K: return "Teac-K";
            case THOMSON:
            case THOMSON7: return "Thomson";
            case TIVO_UNIT_0:
            case TIVO_UNIT_1:
            case TIVO_UNIT_2:
            case TIVO_UNIT_3:
            case TIVO_UNIT_4:
            case TIVO_UNIT_5:
            case TIVO_UNIT_6:
            case TIVO_UNIT_7:
            case TIVO_UNIT_8:
            case TIVO_UNIT_9:
            case TIVO_UNIT: return "Tivo-Nec1";
            case VELLEMAN: return "velleman";
            case _XMP:
            case XMP_1:
            case XMP_2: return "XMP";
            case ZAPTOR_56: return "zaptor_56";
            case ZENITH: return "zenith";
            case _KASEIKYO: return "Kaseikyo";
            case _SAMSUNG: return "Samsung";
            case _LG: return "LG";
            case _SANYO_LC7461: return "Sanyo_LC7461";
            case _RAW: return "raw";

            default: return "unknown";
        }
    }

    static const InfraredProtocolEnum toEnum(const std::string& protocolStr) {
        if (protocolStr == "nec1") return _48_NEC1;
        if (protocolStr == "NEC1") return _48_NEC1;
        if (protocolStr == "nec2") return _48_NEC2;
        if (protocolStr == "aiwa") return AIWA;
        if (protocolStr == "akai") return AKAI;
        if (protocolStr == "apple") return _APPLE;
        if (protocolStr == "barco") return BARCO;
        if (protocolStr == "blaupunkt") return BLAUPUNKT;
        if (protocolStr == "bose") return _BOSE;
        if (protocolStr == "canalsat") return CANALSAT;
        if (protocolStr == "denon") return _DENON;
        if (protocolStr == "Denon-K") return DENON_K;
        if (protocolStr == "denon1") return DENON_1_;
        if (protocolStr == "denon2") return DENON_2_;
        if (protocolStr == "Dgtec") return DGTEC;
        if (protocolStr == "directv") return DIRECTV;
        if (protocolStr == "DishPlayer_Network") return DISH_NETWORK;
        if (protocolStr == "emerson") return EMERSON;
        if (protocolStr == "f12") return F12;
        if (protocolStr == "fujitsu") return FUJITSU;
        if (protocolStr == "GI4dtv") return G_I_4DTV;
        if (protocolStr == "GI_cable") return G_I_CABLE;
        if (protocolStr == "gxb") return GXB;
        if (protocolStr == "grundig16") return GRUNDIG16;
        if (protocolStr == "grundig16_30") return GRUNDIG16_30;
        if (protocolStr == "jvc") return _JVC;
        if (protocolStr == "jvc_two_frames") return JVC_2_;
        if (protocolStr == "Jerrold") return JERROLD;
        if (protocolStr == "kathrein") return KATHREIN;
        if (protocolStr == "konka") return KONKA;
        if (protocolStr == "logitech") return LOGITECH;
        if (protocolStr == "lumagen") return LUMAGEN;
        if (protocolStr == "mce") return MCE;
        if (protocolStr == "Mitsubishi") return _MITSUBISHI;
        if (protocolStr == "nec") return _NEC;
        if (protocolStr == "NEC") return _NEC;
        if (protocolStr == "nec1_f16") return NEC1_F16;
        if (protocolStr == "nec1_rnc") return NEC1_RNC;
        if (protocolStr == "nec1_y1") return NEC1_Y1;
        if (protocolStr == "nec1_y2") return NEC1_Y2;
        if (protocolStr == "nec1_y3") return NEC1_Y3;
        if (protocolStr == "nec2_f16") return NEC2_F16;
        if (protocolStr == "NECx1") return NECX1;
        if (protocolStr == "NECx2") return NECX2;
        if (protocolStr == "NECext") return NECext;
        if (protocolStr == "NEC42") return NEC42;
        if (protocolStr == "NEC42ext") return NEC42ext;
        if (protocolStr == "nrc16") return NRC16;
        if (protocolStr == "nrc17") return NRC17;
        if (protocolStr == "Nokia32") return NOKIA;
        if (protocolStr == "Nokia32single") return NOKIA12;
        if (protocolStr == "pacemss") return PACEMSS;
        if (protocolStr == "panasonic") return _PANASONIC;
        if (protocolStr == "panasonic2") return PANASONIC2;
        if (protocolStr == "panasonic_old") return PANASONIC_OLD;
        if (protocolStr == "pioneer") return _PIONEER;
        if (protocolStr == "Proton") return PROTON;
        if (protocolStr == "rc5") return _RC5;
        if (protocolStr == "RC5") return _RC5;
        if (protocolStr == "rc5x") return _RC5X;
        if (protocolStr == "RC5X") return _RC5X;
        if (protocolStr == "rc5odd") return RC5_7F;
        if (protocolStr == "rc6") return _RC6;
        if (protocolStr == "RC6") return _RC6;
        if (protocolStr == "rc6-6-20") return RC6_6_20;
        if (protocolStr == "rca") return RCA;
        if (protocolStr == "recs80_45") return RECS80;
        if (protocolStr == "replay") return REPLAY;
        if (protocolStr == "Samsung") return _SAMSUNG;
        if (protocolStr == "Samsung20") return SAMSUNG20;
        if (protocolStr == "Samsung32") return SAMSUNG32;
        if (protocolStr == "Samsung36") return _SAMSUNG36;
        if (protocolStr == "scatl_6") return SCATL_6;
        if (protocolStr == "sejin_1_38") return SEJIN_1_38;
        if (protocolStr == "sejin_1_56") return SEJIN_1_56;
        if (protocolStr == "sharp") return _SHARP;
        if (protocolStr == "SIRC") return SIRC;
        if (protocolStr == "SIRC15") return SIRC15;
        if (protocolStr == "SIRC20") return SIRC20;
        if (protocolStr == "sony12") return SONY12;
        if (protocolStr == "sony15") return SONY15;
        if (protocolStr == "sony20") return SONY20;
        if (protocolStr == "streamzap") return STREAMZAP;
        if (protocolStr == "Teac-K") return TEAC_K;
        if (protocolStr == "Thomson") return THOMSON;
        if (protocolStr == "Tivo-Nec1") return TIVO_UNIT;
        if (protocolStr == "velleman") return VELLEMAN;
        if (protocolStr == "XMP") return _XMP;
        if (protocolStr == "zaptor_56") return ZAPTOR_56;
        if (protocolStr == "zenith") return ZENITH;
        if (protocolStr == "DishPlayer_Network") return DISH_NETWO;
        if (protocolStr == "Kaseikyo") return _KASEIKYO;
        if (protocolStr == "LG") return _LG;
        if (protocolStr == "Sanyo_LC7461") return _SANYO_LC7461;
        if (protocolStr == "raw") return _RAW;
        return _IR_UNKNOWN;
    }

};
