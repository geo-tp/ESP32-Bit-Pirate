#pragma once

static const char* const autoCompleteWords[] = {

    // --- General ---
    "help","mode","man","system","logic","analogic","wizard", "hex", "profile",
    "alias", "listen", "sys", "delay", "delayms", "delayus", "repeat",

    // --- 1WIRE ---
    "scan","ping","sniff","read","write","temp","ibutton","eeprom","config",

    // --- UART / HDUART ---
    "autobaud","bridge","at","spam","glitch","xmodem","swap", "emulator",
    "trigger", "raw",

    // --- I2C ---
    "discovery","identify","slave","dump","flood","health","monitor","freq",
    "recover","jam", "regs",

    // --- SPI ---
    "sdcard","flash",

    // --- 2WIRE / 3WIRE ---
    "smartcard",

    // --- DIO ---
    "set","pullup","pulldown","pulse","servo","pwm","toggle","measure","reset",
    "pins",

    // --- LED ---
    "fill","blink","rainbow","chase","cycle","wave","setprotocol",

    // --- INFRARED ---
    "send","receive","devicebgone","remote","replay","record","load",

    // --- USB ---
    "stick", "storage","keyboard","mouse","gamepad","jiggle", "host", "sysctrl", "adapter",

    // --- BLUETOOTH ---
    "pair","spoof","status","server",

    // --- WIFI ---
    "connect","probe","deauth","disconnect","ap", "ap spam",
    "ssh","telnet","nc","nmap","modbus", "repeater", "extender",
    "http","lookup","webui", "flood",

    // --- JTAG ---
    "scan swd","scan jtag","openocd",

    // --- I2S ---
    "play","test",

    // --- SUBGHZ ---
    "sweep","decode","bruteforce","trace","setfrequency",
    "waterfall", "ear",

    // --- RFID ---
    "clone","erase",

    // --- RF24 ---
    "setchannel",

    // --- LORA ---
    "rssi", "cad", "activity", "airtime", "toa", "setfreq",
    "meshtastic", "mesh",

    // ---- FM ---
    "broadcast",

    // --- CELL ---
    "call", "sms", "modem", "sim", "network", "unlock", "ussd", "setmode",
    "operator", "phonebook",

    // --- MODE shortcuts ---
    "m hiz","m 1wire","m uart","m hduart","m i2c","m spi",
    "m 2wire","m 3wire","m dio","m led","m infrared",
    "m usb","m bluetooth","m wifi","m jtag","m i2s","m can",
    "m ethernet","m subghz","m rfid","m rf24", "m onewire", 
    "m cell", "m fm", "m lora", "m expander",

    "mode hiz","mode 1wire","mode uart","mode hduart","mode i2c","mode spi",
    "mode 2wire","mode 3wire","mode dio","mode led","mode infrared",
    "mode usb","mode bluetooth","mode wifi","mode jtag","mode i2s","mode can",
    "mode ethernet","mode subghz","mode rfid","mode rf24", "mode onewire",
    "mode cell", "mode fm", "mode lora", "mode expander",

    nullptr
};
