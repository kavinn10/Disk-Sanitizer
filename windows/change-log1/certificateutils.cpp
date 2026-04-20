// we are using this generate json certificate with sha256 hash for integrity check and it will be given in pdf format to user

#include "certificateutils.h"
#include <QJsonDocument>
#include <QFile>
#include <QCryptographicHash>
#include <QDateTime>
#include <QTextStream>

// For PDF export (Qt)
#include <QPdfWriter>
#include <QPainter>

QString CertificateUtils::jsonToPrettyString(const QJsonObject &metadata)
{
    QJsonDocument doc(metadata);
    return QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
}

// Generate JSON certificate with metadata + SHA-256 hash
bool CertificateUtils::generateJsonCertificate(const QJsonObject &metadata, const QString &filePath)
{
    QJsonObject certObj = metadata;

    // Serialize metadata to JSON string
    QJsonDocument doc(metadata);
    QByteArray rawData = doc.toJson(QJsonDocument::Compact);

    // Add SHA-256 hash
    QByteArray hash = calculateSha256(rawData);
    certObj["sha256"] = QString(hash.toHex());

    // Final JSON with hash included
    QJsonDocument finalDoc(certObj);

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(finalDoc.toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

// Generate PDF certificate (human-readable, tamper-evident by showing hash)
bool CertificateUtils::generatePdfCertificate(const QJsonObject &metadata, const QString &filePath)
{
    QPdfWriter pdfWriter(filePath);
    pdfWriter.setPageSize(QPageSize(QPageSize::A4));
    pdfWriter.setTitle("Drive Sanitization Certificate");

    QPainter painter(&pdfWriter);
    painter.setFont(QFont("Helvetica", 12));

    int y = 100;
    painter.drawText(100, y, "Drive Sanitization Certificate");
    y += 50;

    // Print each metadata field
    for (auto it = metadata.begin(); it != metadata.end(); ++it)
    {
        QString line = QString("%1: %2").arg(it.key(), it.value().toString());
        painter.drawText(100, y, line);
        y += 30;
    }

    // Add SHA-256 hash for tamper-proofing
    QJsonDocument doc(metadata);
    QByteArray rawData = doc.toJson(QJsonDocument::Compact);
    QByteArray hash = calculateSha256(rawData);

    y += 40;
    painter.drawText(100, y, "SHA-256 Integrity Hash:");
    y += 30;
    painter.drawText(100, y, QString(hash.toHex()));

    painter.end();
    return true;
}

// SHA-256 calculation
QByteArray CertificateUtils::calculateSha256(const QByteArray &data)
{
    return QCryptographicHash::hash(data, QCryptographicHash::Sha256);
}

// Verify JSON certificate by recomputing hash
bool CertificateUtils::verifyJsonCertificate(const QString &jsonFilePath)
{
    QFile file(jsonFilePath);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QByteArray fileData = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(fileData);
    if (!doc.isObject())
        return false;

    QJsonObject obj = doc.object();
    if (!obj.contains("sha256"))
        return false;

    QString storedHash = obj["sha256"].toString();
    obj.remove("sha256"); // Remove before recalculating

    QByteArray rawData = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    QByteArray computedHash = calculateSha256(rawData);

    return (storedHash == QString(computedHash.toHex()));
}
