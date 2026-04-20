#ifndef CERTIFICATEUTILS_H
#define CERTIFICATEUTILS_H

#include <QString>
#include <QJsonObject>

class CertificateUtils {
public:
    // Phase-1 core methods
    static bool generateJsonCertificate(const QJsonObject &metadata, const QString &filePath);
    static bool generatePdfCertificate(const QJsonObject &metadata, const QString &filePath);

    // Hash helpers for tamper-proofing
    static QByteArray calculateSha256(const QByteArray &data);
    static bool verifyJsonCertificate(const QString &jsonFilePath);

private:
    static QString jsonToPrettyString(const QJsonObject &metadata);
};

#endif // CERTIFICATEUTILS_H
