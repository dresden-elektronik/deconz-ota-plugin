#include <QDataStream>
#include <QDebug>
#include <deconz.h>
#include "otau_file.h"

#define MANDATORY_HEADER_LENGTH 56
#define SEGMENT_HEADER_LENGTH 6

/*! The constructor.
 */
OtauFile::OtauFile()
{
   upgradeFileId = 0x0BEEF11E;
   headerVersion = 0x0100;
   headerLength = 0;
   headerFieldControl = 0x0000;
   manufacturerCode = 0x1135;
   imageType = 0;
   fileVersion = 0;
   zigBeeStackVersion = 0x0002; // ZigBee PRO
   totalImageSize = 0;
}

/*! Packs the file in a byte array.
    \return the array holding the otau file
 */
QByteArray OtauFile::toArray()
{
    headerLength = MANDATORY_HEADER_LENGTH; // fixed length

    if (headerFieldControl & OF_FC_SECURITY_CREDENTIAL_VERSION)
    {
        headerLength += 1;
    }

    if (headerFieldControl & OF_FC_DEVICE_SPECIFIC)
    {
        headerLength += 8; // mac address
    }

    if (headerFieldControl & OF_FC_HARDWARE_VERSION)
    {
        headerLength += 2; // min hw version
        headerLength += 2; // max hw version
    }

    totalImageSize = headerLength;

    {
        std::list<SubElement>::iterator it = subElements.begin();
        std::list<SubElement>::iterator end = subElements.end();
        for (;it != end; ++it)
        {
            totalImageSize += (2 + 4); // fixed tag size
            totalImageSize += (uint32_t)it->data.size();
        }
    }

    QByteArray arr;
    QDataStream stream(&arr, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    stream << upgradeFileId;
    stream << headerVersion;
    stream << headerLength;
    stream << headerFieldControl;
    stream << manufacturerCode;
    stream << imageType;
    stream << fileVersion;
    stream << zigBeeStackVersion;
    for (uint i = 0; i < 32; i++)
    {
        stream << headerString[i];
    }

    stream << totalImageSize;

    if (headerFieldControl & OF_FC_SECURITY_CREDENTIAL_VERSION)
    {
        stream << securityCredentialVersion;
    }

    if (headerFieldControl & OF_FC_DEVICE_SPECIFIC)
    {
        stream << upgradeFileDestination;
    }

    if (headerFieldControl & OF_FC_HARDWARE_VERSION)
    {
        stream << minHardwareVersion;
        stream << maxHardwareVersion;
    }

    DBG_Printf(DBG_OTA, "OTAU: %s: %d bytes\n", qPrintable(path), totalImageSize);
    DBG_Printf(DBG_OTA, "OTAU:   ota header (%u bytes)\n", headerLength);

    // append tags
    {
        std::list<SubElement>::iterator it = subElements.begin();
        std::list<SubElement>::iterator end = subElements.end();
        for (;it != end; ++it)
        {
            stream << it->tag;
            stream << it->length;
            DBG_Printf(DBG_OTA, "OTAU:   tag 0x%04X, length 0x%08X (%d bytes)\n", it->tag, it->length, it->data.size() + SEGMENT_HEADER_LENGTH);

            for (int i = 0; i < it->data.size(); i++)
            {
                stream << (uint8_t)it->data[i];
            }
        }
    }

    DBG_Printf(DBG_OTA, "OTAU:   packed %d bytes\n", qPrintable(path), arr.length());

    return arr;
}

/*! Reads the otau file from a byte array.
    \return true on success or false false if no valid data was found
 */
bool OtauFile::fromArray(const QByteArray &arr)
{
    DBG_Printf(DBG_OTA, "OTAU: %s: %d bytes\n", qPrintable(path), arr.length());

    if (arr.size() < MANDATORY_HEADER_LENGTH)
    {
        DBG_Printf(DBG_OTA, "OTAU: %s: not an ota file (too small)\n", qPrintable(path));
        return false;
    }

    const char *hdr = "\x1e\xf1\xee\x0b";

    int offset = arr.indexOf(hdr);
    if (offset < 0)
    {
        DBG_Printf(DBG_OTA, "OTAU: %s: not an ota file (header not found)\n", qPrintable(path));
        return false;
    }

    uint processedLength = 0;
    QDataStream stream(arr);
    stream.setByteOrder(QDataStream::LittleEndian);

    for (int i = 0; i < offset; i++)
    {
        quint8 dummy;
        stream >> dummy;
    }

    stream >> upgradeFileId;
    stream >> headerVersion;
    stream >> headerLength;

    if (headerLength < MANDATORY_HEADER_LENGTH)
    {
        DBG_Printf(DBG_OTA, "OTAU: %s: not an ota file (invalid header length)\n", qPrintable(path));
        return false;
    }

    stream >> headerFieldControl;
    stream >> manufacturerCode;
    stream >> imageType;
    stream >> fileVersion;
    stream >> zigBeeStackVersion;
    for (uint i = 0; i < sizeof(headerString); i++)
    {
        stream >> headerString[i];
    }

    stream >> totalImageSize;
    processedLength = MANDATORY_HEADER_LENGTH;

    if (headerFieldControl & OF_FC_SECURITY_CREDENTIAL_VERSION)
    {
        stream >> securityCredentialVersion;
        processedLength += 1;
    }

    if (headerFieldControl & OF_FC_DEVICE_SPECIFIC)
    {
        stream >> upgradeFileDestination;
        processedLength += 8;
    }

    if (headerFieldControl & OF_FC_HARDWARE_VERSION)
    {
        stream >> minHardwareVersion;
        stream >> maxHardwareVersion;
        processedLength += 4;
    }

    // discard optional fields of header
    for (quint16 i = 0; headerLength > processedLength; i++)
    {
        quint8 dummy;
        stream >> dummy;
        processedLength++;
    }

    DBG_Printf(DBG_OTA, "OTAU:   offset %6d: ota header (%u bytes)\n", offset, processedLength);

    // read tags

    subElements.clear();

    while (arr.size() - offset - processedLength >= SEGMENT_HEADER_LENGTH)
    {
        SubElement sub;
        int size = 0;
        int start = offset + processedLength;

        stream >> sub.tag;
        processedLength += 2;
        stream >> sub.length;
        processedLength += 4;

        if (sub.length > arr.size() - offset - processedLength)
        {
            size = arr.size() - offset - processedLength;
        }
        else
        {
            size = sub.length;
        }

        while (!stream.atEnd())
        {
            if (sub.data.size() == size)
            {
                break;
            }

            uint8_t ch;
            stream >> ch;
            processedLength++;
            sub.data.append(static_cast<char>(ch));
            Q_ASSERT(sub.data.size() <= size);
        }

        if (sub.data.size() == size)
        {
            subElements.push_back(sub);
            DBG_Printf(DBG_OTA, "OTAU:   offset %6u: tag 0x%04X, length 0x%08X (%d bytes)\n", start, sub.tag, sub.length, size + SEGMENT_HEADER_LENGTH);
        }
        else
        {
            DBG_Printf(DBG_OTA, "OTAU:   offset %6u: ignore tag 0x%04X with invalid length\n", start, sub.tag);
        }
    }
    if (!stream.atEnd())
    {
        DBG_Printf(DBG_OTA, "OTAU:   offset %6u: ignore trailing %d bytes\n", offset + processedLength, arr.size() - offset - processedLength);
    }

    raw = arr;
    return !subElements.empty();
}
