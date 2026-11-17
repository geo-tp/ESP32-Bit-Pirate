#pragma once

#include <map>
#include <string>
#include <vector>

#include "Interfaces/INvsService.h"

class FakeNvsService final : public INvsService {
public:
    std::map<std::string, std::string> values;
    std::vector<std::pair<std::string, std::string>> savedStrings;
    uint32_t openCalls = 0;
    uint32_t closeCalls = 0;
    bool failWrites = false;

    void open() override { ++openCalls; }
    void close() override { ++closeCalls; }

    void saveString(const std::string& key, const std::string& value) override {
        if (failWrites) return;
        values[key] = value;
        savedStrings.push_back({key, value});
    }

    std::string getString(const std::string& key, const std::string& defaultValue = "") override {
        auto it = values.find(key);
        return it == values.end() ? defaultValue : it->second;
    }
};

