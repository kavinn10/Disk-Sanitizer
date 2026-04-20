#include "driveutils.h"
#include <windows.h>
#include <winioctl.h>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp> 
#include <cstring>

std::string DriveUtils::trim(const std::string &s) {
    size_t start = 0;
    while (start < s.size() && (s[start] == ' ' || s[start] == '\0')) ++start;
    size_t end = s.size();
    while (end > start && (s[end-1] == ' ' || s[end-1] == '\0')) --end;
    return (end > start) ? s.substr(start, end - start) : "";
}

std::string DriveUtils::parseIdentifyString(const uint8_t *data, int wordIndex, int byteLen) {
    std::string out;
    out.reserve(byteLen);
    const uint8_t *p = data + wordIndex * 2;
    for (int i = 0; i < byteLen / 2; ++i) {
        uint8_t high = p[i*2 + 1];
        uint8_t low  = p[i*2 + 0];
        out.push_back(high);
        out.push_back(low);
    }
    return trim(out);
}

unsigned long long DriveUtils::parseMaxLBAFromIdentify(const uint8_t *data) {
    auto read_word = [&](int w)->unsigned int {
        int idx = w * 2;
        return (unsigned int)data[idx] | ((unsigned int)data[idx+1] << 8);
    };
    unsigned long long w100 = read_word(100);
    unsigned long long w101 = read_word(101);
    unsigned long long w102 = read_word(102);
    unsigned long long w103 = read_word(103);
    unsigned long long lba48 = (w103 << 48) | (w102 << 32) | (w101 << 16) | w100;
    if (lba48 != 0) return lba48;
    unsigned long long w60 = read_word(60);
    unsigned long long w61 = read_word(61);
    return (w61 << 16) | w60; // fallback 28-bit
}

unsigned int DriveUtils::parseRotationFromIdentify(const uint8_t *data) {
    auto read_word = [&](int w)->unsigned int {
        int idx = w * 2;
        return (unsigned int)data[idx] | ((unsigned int)data[idx+1] << 8);
    };
    unsigned int wr217 = read_word(217);
    if (wr217 == 1 || wr217 == 0 || wr217 == 0xFFFF) return 0;
    return wr217;
}

// Detect ATA drives
bool DriveUtils::ataIdentify(HANDLE hDevice, DriveInfo &out) {
    const DWORD IDENTIFY_LEN = 512;
    DWORD bufferSize = sizeof(ATA_PASS_THROUGH_EX) + IDENTIFY_LEN;
    std::vector<BYTE> buf(bufferSize);
    memset(buf.data(), 0, bufferSize);

    ATA_PASS_THROUGH_EX *apt = reinterpret_cast<ATA_PASS_THROUGH_EX*>(buf.data());
    apt->Length = sizeof(ATA_PASS_THROUGH_EX);
    apt->TimeOutValue = 10;
    apt->DataTransferLength = IDENTIFY_LEN;
    apt->DataBufferOffset = sizeof(ATA_PASS_THROUGH_EX);
    apt->AtaFlags = ATA_FLAGS_DATA_IN;
    apt->CurrentTaskFile[6] = 0xEC;

    DWORD bytesReturned = 0;
    BOOL ok = DeviceIoControl(hDevice, IOCTL_ATA_PASS_THROUGH,
                              buf.data(), bufferSize,
                              buf.data(), bufferSize,
                              &bytesReturned, NULL);

    if (!ok) {
        ok = DeviceIoControl(hDevice, IOCTL_ATA_PASS_THROUGH_DIRECT,
                             buf.data(), bufferSize,
                             buf.data(), bufferSize,
                             &bytesReturned, NULL);
        if (!ok) return false;
    }

    const uint8_t *identify = buf.data() + apt->DataBufferOffset;
    out.serial   = parseIdentifyString(identify, 10, 20);
    out.firmware = parseIdentifyString(identify, 23, 8);
    out.model    = parseIdentifyString(identify, 27, 40);
    out.maxLBA   = parseMaxLBAFromIdentify(identify);
    out.rotationRPM = parseRotationFromIdentify(identify);

    if (out.rotationRPM == 0) {
        if (out.type.find("SATA") != std::string::npos) out.type = "SSD (SATA)";
        else out.type = "SSD";
    } else {
        out.type = "HDD";
    }

    if (out.maxLBA != 0) {
        unsigned long long bytes = out.maxLBA * 512ULL;
        out.sizeGB = static_cast<long long>(bytes / (1024ULL*1024ULL*1024ULL));
    }

    /
    out.secureEraseSupported = (identify[82] & 0x02) != 0; // simplified, word 82 bit1

    return true;
}

bool DriveUtils::nvmeIdentify(HANDLE hDevice, DriveInfo &out) {
#if defined(IOCTL_STORAGE_PROTOCOL_COMMAND)
    const ULONG IDENTIFY_SIZE = 4096;
    ULONG totalSize = sizeof(STORAGE_PROTOCOL_COMMAND) +
                      sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR) +
                      IDENTIFY_SIZE;
    std::vector<BYTE> buffer(totalSize);
    memset(buffer.data(), 0, totalSize);

    STORAGE_PROTOCOL_COMMAND *cmd =
        reinterpret_cast<STORAGE_PROTOCOL_COMMAND*>(buffer.data());
    cmd->Version = sizeof(STORAGE_PROTOCOL_COMMAND);
    cmd->Length = sizeof(STORAGE_PROTOCOL_COMMAND);
    cmd->ProtocolType = ProtocolTypeNvme;
    cmd->CommandLength = 0;
    cmd->DataFromDeviceTransferLength = IDENTIFY_SIZE;

    STORAGE_PROTOCOL_DATA_DESCRIPTOR *desc =
        reinterpret_cast<STORAGE_PROTOCOL_DATA_DESCRIPTOR*>(
            buffer.data() + sizeof(STORAGE_PROTOCOL_COMMAND));
    desc->Version = sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR);
    desc->Length = sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR);
    desc->ProtocolType = ProtocolTypeNvme;
    desc->DataBlockOffset = sizeof(STORAGE_PROTOCOL_COMMAND) +
                            sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR);
    desc->DataBlockLength = IDENTIFY_SIZE;

    DWORD bytesReturned = 0;
    BOOL ok = DeviceIoControl(hDevice, IOCTL_STORAGE_PROTOCOL_COMMAND,
                              buffer.data(), totalSize,
                              buffer.data(), totalSize,
                              &bytesReturned, NULL);
    if (!ok) return false;

    const uint8_t *identify = buffer.data() + desc->DataBlockOffset;
    out.serial   = trim(std::string((char*)identify + 4, 20));
    out.model    = trim(std::string((char*)identify + 24, 40));
    out.firmware = trim(std::string((char*)identify + 64, 8));
    out.type     = "NVMe";
    out.rotationRPM = 0;

    GET_LENGTH_INFORMATION lenInfo = {};
    if (DeviceIoControl(hDevice, IOCTL_DISK_GET_LENGTH_INFO,
                        NULL, 0, &lenInfo, sizeof(lenInfo),
                        &bytesReturned, NULL)) {
        out.sizeGB = lenInfo.Length.QuadPart / (1024LL*1024LL*1024LL);
    }

    out.secureEraseSupported = true; 
    return true;
#else
    return false;
#endif
}

// Phase 2: Get drives and log
std::vector<DriveInfo> DriveUtils::getDrives() {
    std::vector<DriveInfo> drives;
    nlohmann::json logJson;

    for (int i = 0; i < 32; i++) {
        std::string path = "\\\\.\\PhysicalDrive" + std::to_string(i);
        HANDLE hDevice = CreateFileA(path.c_str(),
                                     GENERIC_READ | GENERIC_WRITE,
                                     FILE_SHARE_READ | FILE_SHARE_WRITE,
                                     NULL, OPEN_EXISTING, 0, NULL);
        if (hDevice == INVALID_HANDLE_VALUE) continue;

        DriveInfo d;
        d.model = "Unknown"; d.serial = "Unknown"; d.firmware = "Unknown";
        d.maxLBA = 0; d.rotationRPM = 0; d.sizeGB = 0; d.type = "Unknown";
        d.secureEraseSupported = false;

        STORAGE_PROPERTY_QUERY query = {};
        query.PropertyId = StorageDeviceProperty;
        query.QueryType = PropertyStandardQuery;

        BYTE buffer[1024] = {};
        DWORD bytesReturned = 0;
        if (DeviceIoControl(hDevice, IOCTL_STORAGE_QUERY_PROPERTY,
                            &query, sizeof(query),
                            &buffer, sizeof(buffer),
                            &bytesReturned, NULL)) {
            STORAGE_DEVICE_DESCRIPTOR *desc = (STORAGE_DEVICE_DESCRIPTOR *)buffer;
            std::string model  = (desc->ProductIdOffset ? std::string((char*)(buffer + desc->ProductIdOffset)) : "");
            std::string vendor = (desc->VendorIdOffset ? std::string((char*)(buffer + desc->VendorIdOffset)) : "");
            std::string serial = (desc->SerialNumberOffset ? std::string((char*)(buffer + desc->SerialNumberOffset)) : "");
            d.model = trim(vendor + " " + model);
            if (!serial.empty()) d.serial = trim(serial);

            switch (desc->BusType) {
                case BusTypeAta:  d.type = "SATA"; break;
                case BusTypeNvme: d.type = "NVMe"; break;
                case BusTypeScsi: d.type = "SCSI"; break;
                case BusTypeUsb:  d.type = "USB";  break;
                default: d.type = "Unknown"; break;
            }
        }

        // ATA/NVMe identification
        if (d.type == "SATA" || d.type == "Unknown") {
            DriveInfo tmp = d; if (ataIdentify(hDevice, tmp)) d = tmp;
        } else if (d.type == "NVMe") {
            DriveInfo tmp = d; if (nvmeIdentify(hDevice, tmp)) d = tmp;
        }

        drives.push_back(d);

        // Log to JSON
        logJson["drives"].push_back({
            {"model", d.model},
            {"serial", d.serial},
            {"firmware", d.firmware},
            {"type", d.type},
            {"sizeGB", d.sizeGB},
            {"rotationRPM", d.rotationRPM},
            {"secureEraseSupported", d.secureEraseSupported}
        });

        CloseHandle(hDevice);
    }

    // Save log for NIST traceability
    std::ofstream logFile("drive_inventory_phase2.json");
    logFile << logJson.dump(4);
    logFile.close();

    return drives;
}

bool DriveUtils::dummyWipe(const std::string &model, const std::string &type) {
    std::cout << "[Dummy] Sanitizing drive: " << model << " (" << type << ")" << std::endl;
    return true;
}
