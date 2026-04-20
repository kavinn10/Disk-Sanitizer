# 🔒 SecureWipe – Cross-Platform Data Wiping & Certification Tool

## 📌 Overview
India is facing a growing **e-waste crisis**, generating **1.75M+ tonnes annually**. A major roadblock in safe disposal and recycling of laptops, PCs, and smartphones is **fear of data breaches**. Millions of devices remain unused or improperly discarded because users lack trust in existing wiping tools.

**SecureWipe** is a **cross-platform, secure, and verifiable data wiping solution** designed to:
- Erase all user data permanently from **HDDs, SSDs, and Android devices**.
- Generate **tamper-proof, digitally signed wipe certificates** (PDF + JSON).
- Provide a **simple one-click interface** for general users.
- Work **offline** via bootable ISO/USB.
- Support **third-party verification** of erasure.
- Comply with **NIST SP 800-88 data sanitization standards**.

This tool empowers users, recyclers, and organizations to safely dispose IT assets, **reduce hoarding**, and promote **India’s circular economy initiatives**.

---

## 🚀 Features
- ✅ **Cross-Platform:** Runs on **Windows, Linux, and Android**.
- ✅ **Secure Erasure:** Multi-pass overwrites (HDD) + crypto-shredding (SSD/Android).
- ✅ **Hidden Areas Covered:** Wipes HPA/DCO, remapped sectors, and SSD secure erase.
- ✅ **One-Click Simplicity:** Easy-to-use GUI + Bootable USB.
- ✅ **Tamper-Proof Certificates:** Digitally signed certificates in **PDF/JSON**.
- ✅ **Standards-Compliant:** Built on **NIST SP 800-88** guidelines.
- ✅ **Offline Mode:** Works without internet connectivity.
- ✅ **Third-Party Verification:** Public certificate validation for trust.

---

## 🔑 Unique Value Propositions (UVP)
- Works smoothly across **Windows, Linux, and Android**.
- Ensures **trust and transparency** with verifiable digital wipe certificates.
- Eliminates need for costly **physical shredding**.
- Reduces **e-waste**, enabling safe device reuse.

---

## 🛠️ Technical Details

### Algorithm Development
- **HDDs:** Multi-pass overwrite (random + DoD/NIST patterns).
- **SSDs/Android:** Crypto-shredding + ATA/NVMe secure erase.

### Security Methods
- **AES-256 encryption** used for sanitization.
- **Secure erase commands** executed at hardware level.

### Certification
- **Digitally signed certificates** (PDF & JSON).
- Immutable and **tamper-proof**.

### Compliance
- Fully aligned with **NIST SP 800-88 Rev. 1** guidelines.

---

## 🌍 Impact
- **Social:** Protects personal and organizational data from leaks.
- **Economic:** Saves costs compared to physical shredding/disposal.
- **Environmental:** Reduces e-waste and supports sustainable recycling.

---

## 📊 Benefits
- 🔐 **Stronger Security:** Permanent erasure with zero recovery.
- 💰 **Cost Savings:** Eliminates dependency on expensive physical destruction.
- 🌱 **Sustainability:** Promotes eco-friendly IT asset reuse.

---

## 📦 Installation & Usage

### Bootable ISO/USB
1. Download the latest **ISO release** from [Releases](./releases).
2. Flash to a USB drive using **Rufus/Etcher**.
3. Boot device from USB.
4. Select target drive → Click **Wipe Now** → Get **Certificate**.

### Desktop Application (Windows/Linux)
```bash
# Clone repo
$ git clone https://github.com/your-org/securewipe.git
$ cd securewipe

# Install dependencies
$ ./install.sh

# Run app
$ ./securewipe
```

### Android Application
- Install **SecureWipe.apk** from [Releases](./releases).
- Launch app → Select storage → Wipe securely → Download certificate.

---

## 📄 Certificate Output Example
```json
{
  "device_id": "NVME-SAMSUNG-123456",
  "algorithm": "AES-256 + Crypto-Shredding",
  "standard": "NIST SP 800-88 Rev.1",
  "wiped_on": "2025-09-13T12:45:00Z",
  "status": "Success",
  "certificate_signature": "0x8a9f...d3e7"
}
```

---

## 🛡️ Roadmap
- [ ] Add **cloud-based certificate registry** for public verification.
- [ ] Enhance **mobile app** with remote wipe.
- [ ] Integrate **open APIs** for recyclers/ITAD providers.
- [ ] Multi-language UI (Hindi, English, etc.).

---

## 🤝 Contributing
We welcome contributions! Please fork the repo and submit a PR. For major changes, open an issue first to discuss.

---

## 📜 License
This project is licensed under the **MIT License**.

---

## 🌐 Acknowledgments
- **NIST SP 800-88** for data sanitization standards.
- Inspiration from global e-waste management & circular economy initiatives.

---

