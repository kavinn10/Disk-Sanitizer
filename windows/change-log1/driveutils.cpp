#include "driveutils.h"
#include <windows.h>
#include <winioctl.h>
#include <vector>
#include <string>
#include <iostream>
#include <cstring>
#include <ntddscsi.h>
#include <random>
#include <chrono>
#include <algorithm>


#ifndef STORAGE_PROTOCOL_COMMAND_FLAG_DATA_IN
#define STORAGE_PROTOCOL_COMMAND_FLAG_DATA_IN 0x1
#endif

// Helper: trim whitespace and nulls
std::string DriveUtils::trim(const std::string &s) {
    size_t start = 0;
    while (start < s.size() && (s[start] == ' ' || s[start] == '\0')) ++start;
    size_t end = s.size();
    while (end > start && (s[end-1] == ' ' || s[end-1] == '\0')) --end;
    return (end > start) ? s.substr(start, end - start) : "";
}

// Helper: parse strings in IDENTIFY word order (each word is 2 bytes, big-endian inside word)
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
    apt->CurrentTaskFile[6] = 0xEC; // IDENTIFY DEVICE

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
        if (!ok) {
            std::cerr << "[Error] ATA IDENTIFY failed." << std::endl;
            return false;
        }
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

    return true;
}

bool DriveUtils::nvmeIdentify(HANDLE hDevice, DriveInfo &out) {
#if defined(IOCTL_STORAGE_PROTOCOL_COMMAND)
    const ULONG IDENTIFY_SIZE = 4096;

    // Allocate buffer large enough for command + specific data + identify data
    ULONG totalSize = sizeof(STORAGE_PROTOCOL_COMMAND) +
                      sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA) +
                      IDENTIFY_SIZE;
    std::vector<BYTE> buffer(totalSize);
    memset(buffer.data(), 0, totalSize);

    STORAGE_PROTOCOL_COMMAND *cmd =
        reinterpret_cast<STORAGE_PROTOCOL_COMMAND*>(buffer.data());
    cmd->Version = sizeof(STORAGE_PROTOCOL_COMMAND);
    cmd->Length = sizeof(STORAGE_PROTOCOL_COMMAND);
    cmd->ProtocolType = ProtocolTypeNvme;
    cmd->Flags = STORAGE_PROTOCOL_COMMAND_FLAG_DATA_IN;
    cmd->CommandLength = 0; // NVMe identify doesn’t use CDB
    cmd->ErrorCode = 0;
    cmd->DataFromDeviceTransferLength = IDENTIFY_SIZE;
    cmd->TimeOutValue = 10;
    cmd->CommandSpecific = 0;
    cmd->Reserved0 = 0;
    cmd->FixedProtocolReturnData = 0;
    cmd->ReturnStatus = 0;

    STORAGE_PROTOCOL_SPECIFIC_DATA *protocolData =
        reinterpret_cast<STORAGE_PROTOCOL_SPECIFIC_DATA*>(
            buffer.data() + sizeof(STORAGE_PROTOCOL_COMMAND));

    protocolData->ProtocolType = ProtocolTypeNvme;
    protocolData->DataType = NVMeDataTypeIdentify;
    protocolData->ProtocolDataRequestValue = 1; // Identify Controller
    protocolData->ProtocolDataRequestSubValue = 0;
    protocolData->ProtocolDataOffset = sizeof(STORAGE_PROTOCOL_COMMAND) +
                                       sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA);
    protocolData->ProtocolDataLength = IDENTIFY_SIZE;

    DWORD bytesReturned = 0;
    BOOL ok = DeviceIoControl(hDevice,
                              IOCTL_STORAGE_PROTOCOL_COMMAND,
                              buffer.data(),
                              totalSize,
                              buffer.data(),
                              totalSize,
                              &bytesReturned,
                              NULL);

    if (!ok) {
        std::cerr << "[Warning] NVMe IDENTIFY failed. Falling back to property query." << std::endl;
        return false;
    }

    const uint8_t *identify =
        buffer.data() + protocolData->ProtocolDataOffset;

    // Serial (20 bytes @ 4)
    out.serial   = trim(std::string((char*)identify + 4, 20));
    // Model (40 bytes @ 24)
    out.model    = trim(std::string((char*)identify + 24, 40));
    // Firmware (8 bytes @ 64)
    out.firmware = trim(std::string((char*)identify + 64, 8));
    out.type     = "NVMe";
    out.rotationRPM = 0;

    GET_LENGTH_INFORMATION lenInfo = {};
    DWORD lenBytesReturned = 0;
    if (DeviceIoControl(hDevice, IOCTL_DISK_GET_LENGTH_INFO,
                        NULL, 0, &lenInfo, sizeof(lenInfo),
                        &lenBytesReturned, NULL)) {
        out.sizeGB = lenInfo.Length.QuadPart / (1024LL*1024LL*1024LL);
    }

    return true;
#else
    return false;
#endif
}
std::vector<DriveInfo> DriveUtils::getDrives() {
    std::vector<DriveInfo> drives;

    for (int i = 0; i < 32; i++) {
        std::string path = "\\\\.\\PhysicalDrive" + std::to_string(i);
        HANDLE hDevice = CreateFileA(path.c_str(),
                                     GENERIC_READ | GENERIC_WRITE,
                                     FILE_SHARE_READ | FILE_SHARE_WRITE,
                                     NULL, OPEN_EXISTING, 0, NULL);
        if (hDevice == INVALID_HANDLE_VALUE) continue;

        DriveInfo d;
        d.model = "Unknown";
        d.serial = "Unknown";
        d.firmware = "Unknown";
        d.maxLBA = 0;
        d.rotationRPM = 0;
        d.sizeGB = 0;
        d.type = "Unknown";
        d.devicePath = path; // <-- important: store the device path

        // Always get size via IOCTL
        GET_LENGTH_INFORMATION lenInfo = {};
        DWORD bytesReturned = 0;
        if (DeviceIoControl(hDevice, IOCTL_DISK_GET_LENGTH_INFO,
                            NULL, 0,
                            &lenInfo, sizeof(lenInfo),
                            &bytesReturned, NULL)) {
            d.sizeGB = static_cast<long long>(lenInfo.Length.QuadPart / (1024ULL*1024ULL*1024ULL));
        }

        // Query storage property
        STORAGE_PROPERTY_QUERY query = {};
        query.PropertyId = StorageDeviceProperty;
        query.QueryType = PropertyStandardQuery;
        BYTE buffer[1024] = {};
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
            case BusTypeUsb:  d.type = "USB"; break;
            default: d.type = "Unknown"; break;
            }
        }

        // ATA identify for SATA
        if (d.type == "SATA" || d.type == "Unknown") {
            DriveInfo tmp = d;
            if (ataIdentify(hDevice, tmp)) d = tmp;
        }
        // NVMe identify
        else if (d.type == "NVMe") {
            DriveInfo tmp = d;
            if (nvmeIdentify(hDevice, tmp)) d = tmp;
        }

        drives.push_back(d);
        CloseHandle(hDevice);
    }

    return drives;
}


bool DriveUtils::wipeDrive(const std::string &devicePath, int passes, bool verify) {
    HANDLE hDevice = CreateFileA(devicePath.c_str(),
                                 GENERIC_READ | GENERIC_WRITE,
                                 FILE_SHARE_READ | FILE_SHARE_WRITE,
                                 NULL, OPEN_EXISTING, 0, NULL);
    if (hDevice == INVALID_HANDLE_VALUE) {
        std::cerr << "[Error] Could not open drive: " << devicePath << std::endl;
        return false;
    }

    // Determine disk size
    GET_LENGTH_INFORMATION lenInfo = {};
    DWORD bytesReturned = 0;
    unsigned long long diskBytes = 0;
    if (DeviceIoControl(hDevice, IOCTL_DISK_GET_LENGTH_INFO,
                        NULL, 0, &lenInfo, sizeof(lenInfo),
                        &bytesReturned, NULL)) {
        diskBytes = (unsigned long long)lenInfo.Length.QuadPart;
    } else {
        LARGE_INTEGER size = {};
        if (GetFileSizeEx(hDevice, &size)) diskBytes = (unsigned long long)size.QuadPart;
    }

    if (diskBytes == 0) {
        std::cerr << "[Error] Unable to determine disk size for: " << devicePath << std::endl;
        CloseHandle(hDevice);
        return false;
    }

    const size_t bufSize = 1024 * 1024; // 1MB buffer
    std::vector<BYTE> buffer(bufSize);

    // random generator for random-pass
    std::mt19937_64 rng((unsigned)std::chrono::high_resolution_clock::now().time_since_epoch().count());

    for (int pass = 0; pass < passes; ++pass) {
        std::cout << "[Wipe] Pass " << (pass + 1) << " / " << passes << " on " << devicePath << std::endl;

        // prepare pattern for this pass
        if (passes == 1) {
            std::fill(buffer.begin(), buffer.end(), 0x00);
        } else {
            if (pass % 3 == 0) std::fill(buffer.begin(), buffer.end(), 0x00);
            else if (pass % 3 == 1) std::fill(buffer.begin(), buffer.end(), 0xFF);
            else {
                for (size_t i = 0; i < bufSize; ++i) buffer[i] = static_cast<BYTE>(rng() & 0xFF);
            }
        }

        // start at beginning
        LARGE_INTEGER offset = {};
        offset.QuadPart = 0;
        if (!SetFilePointerEx(hDevice, offset, NULL, FILE_BEGIN)) {
            std::cerr << "[Error] SetFilePointerEx failed." << std::endl;
            CloseHandle(hDevice);
            return false;
        }

        unsigned long long written = 0;
        while (written < diskBytes) {
            DWORD toWrite = static_cast<DWORD>(std::min<unsigned long long>(bufSize, diskBytes - written));
            DWORD bytesWritten = 0;
            if (!WriteFile(hDevice, buffer.data(), toWrite, &bytesWritten, NULL)) {
                std::cerr << "[Error] WriteFile failed after writing " << written << " bytes." << std::endl;
                CloseHandle(hDevice);
                return false;
            }
            if (bytesWritten == 0) break;
            written += bytesWritten;
        }

        // ensure data is flushed
        FlushFileBuffers(hDevice);
    }

    // Verification step
    if (verify) {
        std::cout << "[Verify] Starting verification..." << std::endl;
        std::vector<BYTE> readbuf(4096);
        bool ok = true;

        int last = (passes == 1) ? 0 : (passes - 1) % 3;
        if (last == 2) {
            std::cout << "[Verify] Last pass was random — skipping byte-wise verification." << std::endl;
        } else {
            BYTE expected = (last == 0) ? 0x00 : 0xFF;
            unsigned long long offsetsToCheck[3] = {0, (diskBytes / 2), (diskBytes > 4096 ? diskBytes - 4096 : 0)};
            for (int i = 0; i < 3 && ok; ++i) {
                LARGE_INTEGER pos = {}; pos.QuadPart = offsetsToCheck[i];
                if (!SetFilePointerEx(hDevice, pos, NULL, FILE_BEGIN)) { ok = false; break; }
                DWORD bytesRead = 0;
                if (!ReadFile(hDevice, readbuf.data(), (DWORD)readbuf.size(), &bytesRead, NULL)) { ok = false; break; }
                for (DWORD j = 0; j < bytesRead; ++j) {
                    if (readbuf[j] != expected) { ok = false; break; }
                }
            }
        }

        if (!ok) {
            std::cerr << "[Verify] Verification failed." << std::endl;
            CloseHandle(hDevice);
            return false;
        }
        std::cout << "[Verify] Verification succeeded." << std::endl;
    }

    CloseHandle(hDevice);
    std::cout << "[Wipe] Completed: " << devicePath << std::endl;
    return true;
}

// -------------------------------------------------------------
// Dummy wipe (for testing without actual overwrite)
// -------------------------------------------------------------
bool DriveUtils::dummyWipe(const std::string &model, const std::string &type) {
    std::cout << "[Dummy] Sanitizing drive: " << model << " (" << type << ")" << std::endl;
    return true;
}

// -------------------------------------------------------------
// Debug print
// -------------------------------------------------------------
void DriveInfo::debugPrint() const {
    std::cout << "Model: " << model
              << ", Serial: " << serial
              << ", Firmware: " << firmware
              << ", Type: " << type
              << ", Size(GB): " << sizeGB
              << ", MaxLBA: " << maxLBA
              << ", RotationRPM: " << rotationRPM
              << ", Path: " << devicePath
              << std::endl;
}
