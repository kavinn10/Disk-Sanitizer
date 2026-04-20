#ifndef DRIVEUTILS_H
#define DRIVEUTILS_H

#include <string>
#include <vector>
#include <windows.h>
#include <cstdint>

// Stores drive details
struct DriveInfo {
    std::string model;
    std::string type;           // "HDD", "SSD (SATA)", "NVMe", "USB", etc.
    long long sizeGB;
    std::string serial;
    std::string firmware;       // firmware revision from IDENTIFY / NVMe
    unsigned long long maxLBA;  // from IDENTIFY or NVMe namespace
    unsigned int rotationRPM;   // 0 => SSD or unknown
    std::string devicePath;     // e.g. "\\\\.\\PhysicalDrive0"

    void debugPrint() const;
};

class DriveUtils {
public:
    // Utility
    static std::string trim(const std::string &s);

    // Drive enumeration
    static std::vector<DriveInfo> getDrives();

    // Wiping (dummy + real)
    static bool dummyWipe(const std::string &model, const std::string &type);
    static bool wipeDrive(const std::string &devicePath, int passes = 1, bool verify = false);

private:
    // Phase 2 helpers
    static bool ataIdentify(HANDLE hDevice, DriveInfo &out);
    static bool nvmeIdentify(HANDLE hDevice, DriveInfo &out);
    static std::string parseIdentifyString(const uint8_t *data, int wordIndex, int byteLen);
    static unsigned long long parseMaxLBAFromIdentify(const uint8_t *data);
    static unsigned int parseRotationFromIdentify(const uint8_t *data);
};

#endif // DRIVEUTILS_H
