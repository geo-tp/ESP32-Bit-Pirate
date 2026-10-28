#pragma once

#include <string>

class INvsService {
public:
    // Storage identifiers shared with early boot, before services exist.
    static constexpr char NVS_NAMESPACE[] = "wifi_settings";
    static constexpr char ONE_SHOT_BOOT_KEY[] = "oneshot_boot";

    virtual ~INvsService() = default;

    virtual void open() = 0;
    virtual void close() = 0;
    virtual void saveString(const std::string& key, const std::string& value) = 0;
    virtual std::string getString(const std::string& key, const std::string& defaultValue = "") = 0;
};
