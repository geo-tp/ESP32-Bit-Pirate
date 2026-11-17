#pragma once
#include <string>
#include <vector>
#include <cstring>
#include <cstdio>
#include <functional>
#include <cstddef>
#include <LittleFS.h>
#include "Interfaces/ILittleFsService.h"

struct httpd_req;

class LittleFsService : public ILittleFsService {
public:
    struct Entry {
        std::string name; 
        size_t      size;
        bool        isDir;
    };

    ~LittleFsService();

    bool begin(bool formatIfFail = true, bool readOnly = false);
    void end();

    bool mounted() const { return _mounted; }

    bool exists(const std::string& userPath) const;
    bool isDir (const std::string& userPath) const;
    size_t getFileSize(const std::string& userPath) const;

    std::vector<Entry> list(const std::string& userDir = "/") const;
    std::vector<std::string> listFiles(const std::string& userDir = "/", const std::string& extension = ".ir") const;

    bool readAll(const std::string& userPath, std::string& out) const;
    bool readChunks(const std::string& userPath,
                    const std::function<bool(const uint8_t*, size_t)>& writer) const override;

    bool write(const std::string& userPath, const std::string& data, bool append=false);
    bool write(const std::string& userPath, const uint8_t* data, size_t len, bool append=false);

    bool mkdirRecursive(const std::string& userDir) const;
    bool removeFile    (const std::string& userPath);
    bool rmdirRecursive(const std::string& userDir);
    bool renamePath    (const std::string& fromUserPath, const std::string& toUserPath);

    bool  getSpace(size_t& total, size_t& used) const;
    size_t freeBytes() const;
    bool  format();   // LittleFS.format()

    const std::string& basePath()       const { return _basePath; }
    const std::string& partitionLabel() const { return _partitionLabel; }
    bool isSafeRootFileName(const std::string& name) const;
    static bool normalizeUserPath(const std::string& in, std::string& out, bool dir=false);
    static const char* mimeFromPath(const char* path);

private:
    std::string _basePath; 
    std::string _partitionLabel;
    bool        _mounted = false;
    bool        _readOnly = false;

    static void ensureDirSlashes(std::string& p, bool dir);

    bool ensureParentDirs(const std::string& userFilePath) const;
    bool rmdirRecursiveImpl(const std::string& userDir);
};
