#ifndef DRIVEUTILS_H
#define DRIVEUTILS_H

#include <string>
#include <vector>

struct DriveInfo {
    std::string model;
    std::string type;         // "HDD", "SSD (SATA)", "NVMe", "USB", etc.
    long long sizeGB;
    std::string serial;
    std::string firmware;     // firmware revision from IDENTIFY / NVMe
    unsigned long long maxLBA; // from IDENTIFY or NVMe namespace
    unsigned int rotationRPM; // 0 => SSD or unknown
};

class DriveUtils {
public:
    static std::vector<DriveInfo> getDrives();
    static bool dummyWipe(const std::string &model, const std::string &type);

private:
    // Phase 2 helpers
    static bool ataIdentify(HANDLE hDevice, DriveInfo &out);
    static bool nvmeIdentify(HANDLE hDevice, DriveInfo &out);
    static std::string parseIdentifyString(const uint8_t *data, int wordIndex, int byteLen);
    static unsigned long long parseMaxLBAFromIdentify(const uint8_t *data);
    static unsigned int parseRotationFromIdentify(const uint8_t *data);
    static std::string trim(const std::string &s);
};

#endif // DRIVEUTILS_H
