#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

class ILittleFsService {
public:
    virtual ~ILittleFsService() = default;

    virtual bool begin(bool formatIfFail = true, bool readOnly = false) = 0;
    virtual void end() = 0;
    virtual bool mounted() const = 0;
    virtual bool exists(const std::string& userPath) const = 0;
    virtual size_t getFileSize(const std::string& userPath) const = 0;
    virtual std::vector<std::string> listFiles(const std::string& userDir = "/",
                                               const std::string& extension = ".ir") const = 0;
    virtual bool readAll(const std::string& userPath, std::string& out) const = 0;
    // Stream fixed-size chunks. Callbacks returning false stop the read.
    virtual bool readChunks(const std::string& userPath,
                            const std::function<bool(const uint8_t*, size_t)>& writer) const = 0;
    virtual bool write(const std::string& userPath, const std::string& data, bool append = false) = 0;
    virtual bool write(const std::string& userPath, const uint8_t* data, size_t len, bool append = false) = 0;
    virtual bool removeFile(const std::string& userPath) = 0;
    virtual bool getSpace(size_t& total, size_t& used) const = 0;
    virtual size_t freeBytes() const = 0;
};
