#pragma once

#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "Interfaces/ILittleFsService.h"

class FakeLittleFsService final : public ILittleFsService {
public:
    bool mountedFlag = true;
    bool beginResult = true;
    bool writeResult = true;
    bool readResult = true;
    bool removeResult = true;
    size_t freeBytesValue = 1024 * 1024;
    size_t totalBytesValue = 1024 * 1024;
    size_t usedBytesValue = 0;
    uint32_t beginCalls = 0;
    uint32_t endCalls = 0;
    mutable uint32_t readAllCalls = 0;
    mutable uint32_t readChunksCalls = 0;
    std::map<std::string, std::string> files;
    std::string lastWritePath;
    std::string lastWriteData;
    bool lastWriteAppend = false;

    bool begin(bool = true, bool = false) override {
        ++beginCalls;
        mountedFlag = beginResult;
        return beginResult;
    }

    void end() override {
        ++endCalls;
        mountedFlag = false;
    }

    bool mounted() const override { return mountedFlag; }

    bool exists(const std::string& userPath) const override {
        return files.find(userPath) != files.end();
    }

    size_t getFileSize(const std::string& userPath) const override {
        const auto it = files.find(userPath);
        return it == files.end() ? 0 : it->second.size();
    }

    std::vector<std::string> listFiles(const std::string& userDir = "/",
                                       const std::string& extension = ".ir") const override {
        std::vector<std::string> result;
        for (const auto& [path, _] : files) {
            if (path.rfind(userDir, 0) != 0) continue;
            if (!extension.empty() &&
                (path.size() < extension.size() ||
                 path.substr(path.size() - extension.size()) != extension)) {
                continue;
            }

            std::string name = path.substr(userDir.size());
            if (!name.empty() && name[0] == '/') name.erase(0, 1);
            result.push_back(name);
        }
        return result;
    }

    bool readAll(const std::string& userPath, std::string& out) const override {
        ++readAllCalls;
        if (!readResult) return false;
        const auto it = files.find(userPath);
        if (it == files.end()) return false;
        out = it->second;
        return true;
    }

    bool readChunks(const std::string& userPath,
                    const std::function<bool(const uint8_t*, size_t)>& writer) const override {
        ++readChunksCalls;
        if (!readResult) return false;
        const auto it = files.find(userPath);
        if (it == files.end()) return false;
        const auto& data = it->second;
        for (size_t pos = 0; pos < data.size(); pos += 128) {
            const size_t len = std::min<size_t>(128, data.size() - pos);
            if (!writer(reinterpret_cast<const uint8_t*>(data.data() + pos), len)) return false;
        }
        return true;
    }

    bool write(const std::string& userPath, const std::string& data, bool append = false) override {
        lastWritePath = userPath;
        lastWriteData = data;
        lastWriteAppend = append;
        if (!writeResult) return false;
        if (append) files[userPath] += data;
        else files[userPath] = data;
        return true;
    }

    bool write(const std::string& userPath, const uint8_t* data, size_t len, bool append = false) override {
        return write(userPath, std::string(reinterpret_cast<const char*>(data), len), append);
    }

    bool removeFile(const std::string& userPath) override {
        if (!removeResult) return false;
        files.erase(userPath);
        return true;
    }

    bool getSpace(size_t& total, size_t& used) const override {
        total = totalBytesValue;
        used = usedBytesValue;
        return true;
    }

    size_t freeBytes() const override { return freeBytesValue; }
};
