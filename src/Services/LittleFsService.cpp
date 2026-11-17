#include "LittleFsService.h"

LittleFsService::~LittleFsService() {
    end();
}

bool LittleFsService::begin(bool formatIfFail, bool readOnly) {
    if (_mounted) {
        _readOnly = readOnly;
        return true;
    }

    _mounted = LittleFS.begin(formatIfFail);
    _readOnly = readOnly;
    return _mounted;
}

void LittleFsService::end() {
    if (_mounted) {
        LittleFS.end();
        _mounted = false;
    }
}

bool LittleFsService::normalizeUserPath(const std::string& in, std::string& out, bool dir) {
    std::string p = in;
    if (p.empty() || p[0] != '/') p = "/" + p;
    if (p.find("..") != std::string::npos) return false;

    // Collapse // -> /
    std::string q; q.reserve(p.size());
    char prev = 0;
    for (char c : p) {
        if (c=='/' && prev=='/') continue;
        q.push_back(c);
        prev = c;
    }
    out.swap(q);
    ensureDirSlashes(out, dir);
    return true;
}

void LittleFsService::ensureDirSlashes(std::string& p, bool dir) {
    if (dir) {
        if (p.size() > 1 && p.back() != '/') p.push_back('/');
        // la racine "/" reste "/"
    } else {
        while (p.size() > 1 && p.back() == '/') p.pop_back();
    }
}

bool LittleFsService::exists(const std::string& userPath) const {
    if (!_mounted) return false;
    return LittleFS.exists(userPath.c_str());
}

bool LittleFsService::isDir(const std::string& userPath) const {
    if (!_mounted) return false;

    // Root
    if (userPath.empty() || userPath == "/") {
        fs::File root = LittleFS.open("/");
        return (bool)root && root.isDirectory();
    }

    fs::File dir = LittleFS.open(userPath.c_str());
    return (bool)dir && dir.isDirectory();
}

std::vector<LittleFsService::Entry> LittleFsService::list(const std::string& userDir) const {
    std::vector<Entry> out;
    if (!_mounted) return out;

    fs::File dir = LittleFS.open(userDir.c_str());
    if (!dir || !dir.isDirectory()) return out;

    for (fs::File f = dir.openNextFile(); f; f = dir.openNextFile()) {
        std::string full = f.name();
        std::string name = full;

        if (full.rfind(userDir, 0) == 0) {
            name = full.substr(userDir.size());
            if (!name.empty() && name[0] == '/') name.erase(0, 1);
        } else if (full.size() && full[0]=='/') {
            // limit
            name = full.substr(1);
        }

        out.push_back(Entry{
            /*name=*/name,
            /*size=*/static_cast<size_t>(f.size()),
            /*isDir=*/f.isDirectory()
        });
        f.close();
    }
    dir.close();
    return out;
}

size_t LittleFsService::getFileSize(const std::string& userPath) const {
    if (!_mounted) return 0;

    std::string p;
    if (!normalizeUserPath(userPath, p, /*dir=*/false)) return 0;

    fs::File f = LittleFS.open(p.c_str(), "r");
    if (!f) return 0;

    size_t size = f.size();
    f.close();
    return size;
}

bool LittleFsService::readAll(const std::string& userPath, std::string& out) const {
    if (!_mounted) return false;
    std::string p;
    if (!normalizeUserPath(userPath, p, /*dir=*/false)) return false;

    fs::File f = LittleFS.open(p.c_str(), "r");
    if (!f) return false;

    out.clear();
    out.reserve(f.size());
    std::vector<uint8_t> buf(2048);
    while (true) {
        int n = f.read(buf.data(), buf.size());
        if (n < 0) { f.close(); return false; }
        if (n == 0) break;
        out.append(reinterpret_cast<const char*>(buf.data()), n);
    }
    f.close();
    return true;
}

bool LittleFsService::readChunks(const std::string& userPath,
                                 const std::function<bool(const uint8_t*, size_t)>& writer) const {
    if (!_mounted) return false;
    std::string p;
    if (!normalizeUserPath(userPath, p, /*dir=*/false)) return false;

    fs::File f = LittleFS.open(p.c_str(), "r");
    if (!f) return false;

    uint8_t buf[512]; // Keep the streaming IR path light on the ESP32 task stack.
    bool ok = true;
    while (true) {
        int n = f.read(buf, sizeof(buf));
        if (n < 0) { ok = false; break; }
        if (n == 0) break;
        if (!writer(buf, (size_t)n)) { ok = false; break; }
    }
    f.close();
    return ok;
}

bool LittleFsService::ensureParentDirs(const std::string& userFilePath) const {
    auto pos = userFilePath.find_last_of('/');
    if (pos == std::string::npos || pos == 0) return true;
    const std::string dir = userFilePath.substr(0, pos);
    return mkdirRecursive(dir);
}

bool LittleFsService::write(const std::string& userPath, const std::string& data, bool append) {
    return write(userPath, reinterpret_cast<const uint8_t*>(data.data()), data.size(), append);
}

bool LittleFsService::write(const std::string& userPath, const uint8_t* data, size_t len, bool append) {
    if (!_mounted || _readOnly) return false;

    if (!ensureParentDirs(userPath)) return false;

    const char* mode = append ? "a" : "w";
    fs::File f = LittleFS.open(userPath.c_str(), mode, append ? false : true);
    if (!f) return false;

    size_t off = 0;
    const size_t CHUNK = 4096;
    bool ok = true;
    while (off < len) {
        size_t n = (len - off < CHUNK) ? (len - off) : CHUNK;
        size_t w = f.write(data + off, n);
        if (w != n) { ok = false; break; }
        off += n;
    }
    f.close();
    return ok;
}

bool LittleFsService::mkdirRecursive(const std::string& userDir) const {
    if (!_mounted || _readOnly) return false;

    std::string d;
    if (!normalizeUserPath(userDir, d, /*dir=*/true)) return false;

    size_t start = 1; // ignore first '/'
    while (start < d.size()) {
        size_t next = d.find('/', start);
        if (next == std::string::npos) next = d.size();
        std::string cur = d.substr(0, next);
        if (cur.size() > 1 && !LittleFS.exists(cur.c_str())) {
            if (!LittleFS.mkdir(cur.c_str())) return false;
        }
        start = next + 1;
    }
    return true;
}

bool LittleFsService::removeFile(const std::string& userPath) {
    if (!_mounted || _readOnly) return false;
    std::string p;
    if (!normalizeUserPath(userPath, p, /*dir=*/false)) return false;
    return LittleFS.remove(p.c_str());
}

bool LittleFsService::rmdirRecursiveImpl(const std::string& userDir) {
    fs::File dir = LittleFS.open(userDir.c_str());
    if (!dir || !dir.isDirectory()) return false;

    for (fs::File f = dir.openNextFile(); f; f = dir.openNextFile()) {
        std::string child = f.name();
        if (f.isDirectory()) {
            f.close();
            if (!rmdirRecursiveImpl(child)) { dir.close(); return false; }
        } else {
            f.close();
            if (!LittleFS.remove(child.c_str())) { dir.close(); return false; }
        }
    }
    dir.close();
    return LittleFS.rmdir(userDir.c_str());
}

bool LittleFsService::rmdirRecursive(const std::string& userDir) {
    if (!_mounted || _readOnly) return false;
    std::string d;
    if (!normalizeUserPath(userDir, d, /*dir=*/true)) return false;

    if (d == "/") return false;

    return rmdirRecursiveImpl(d);
}

bool LittleFsService::renamePath(const std::string& fromUserPath, const std::string& toUserPath) {
    if (!_mounted || _readOnly) return false;
    std::string a, b;
    if (!normalizeUserPath(fromUserPath, a, /*dir=*/false)) return false;
    if (!normalizeUserPath(toUserPath,   b, /*dir=*/false)) return false;

    if (!ensureParentDirs(b)) return false;

    return LittleFS.rename(a.c_str(), b.c_str());
}

bool LittleFsService::getSpace(size_t& total, size_t& used) const {
    if (!_mounted) return false;
    total = LittleFS.totalBytes();
    used  = LittleFS.usedBytes();
    return true;
}

size_t LittleFsService::freeBytes() const {
    size_t total=0, used=0;
    if (!getSpace(total, used)) return 0;
    return (total > used) ? (total - used) : 0;
}

bool LittleFsService::format() {
    if (_mounted) LittleFS.end();
    bool ok = LittleFS.format();
    _mounted = LittleFS.begin(true, _basePath.c_str(), 10, _partitionLabel.c_str());
    return ok && _mounted;
}

const char* LittleFsService::mimeFromPath(const char* path) {
    if (!path) return "application/octet-stream";
    const char* dot = std::strrchr(path, '.');
    if (!dot) return "application/octet-stream";
    if (!strcasecmp(dot, ".html") || !strcasecmp(dot, ".htm")) return "text/html";
    if (!strcasecmp(dot, ".css"))  return "text/css";
    if (!strcasecmp(dot, ".js"))   return "application/javascript";
    if (!strcasecmp(dot, ".json")) return "application/json";
    if (!strcasecmp(dot, ".png"))  return "image/png";
    if (!strcasecmp(dot, ".jpg") || !strcasecmp(dot, ".jpeg")) return "image/jpeg";
    if (!strcasecmp(dot, ".gif"))  return "image/gif";
    if (!strcasecmp(dot, ".svg"))  return "image/svg+xml";
    if (!strcasecmp(dot, ".ico"))  return "image/x-icon";
    if (!strcasecmp(dot, ".txt"))  return "text/plain";
    if (!strcasecmp(dot, ".wasm")) return "application/wasm";
    return "application/octet-stream";
}

bool LittleFsService::isSafeRootFileName(const std::string& name) const {
    if (name.empty()) return false;
    if (name.find('\0') != std::string::npos) return false;
    if (name.find("..") != std::string::npos) return false;
    if (name.find('/') != std::string::npos || name.find('\\') != std::string::npos) return false;
    return true;
}

std::vector<std::string> LittleFsService::listFiles(const std::string& userDir,
                                                    const std::string& extension) const {
    std::vector<std::string> files;
    if (!_mounted) return files;

    fs::File dir = LittleFS.open(userDir.c_str());
    if (!dir || !dir.isDirectory()) return files;

    // Normalize extension filter
    std::string extFilter = extension;
    for (auto& c : extFilter) c = static_cast<char>(tolower((unsigned char)c));
    const bool matchAll = extFilter.empty() || extFilter == "*";

    // support "ir" as ".ir"
    if (!matchAll && !extFilter.empty() && extFilter[0] != '.') {
        extFilter = "." + extFilter;
    }

    // Ensure iteration from start
    dir.seekDir(0);

    while (true) {
        bool isDir = false;
        String s = dir.getNextFileName(&isDir);
        if (s.length() == 0) break;

        if (isDir) continue;

        std::string name = s.c_str();
        if (!name.empty() && name[0] == '/') name.erase(0, 1);

        if (matchAll) {
            files.push_back(name);
            continue;
        }

        // Extension compare (lowercase)
        auto pos = name.rfind('.');
        if (pos == std::string::npos) continue;

        std::string ext = name.substr(pos);
        for (auto& c : ext) c = static_cast<char>(tolower((unsigned char)c));

        if (ext == extFilter) {
            files.push_back(name);
        }
    }

    dir.close();
    return files;
}