#include <QDataStream>
#include <QDebug>
#include <deconz.h>
#include "otau_file.h"

#define MANDATORY_HEADER_LENGTH 56

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
            it->length = (uint32_t)it->data.size();
            totalImageSize += it->length;
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

    // append tags
    {
        std::list<SubElement>::iterator it = subElements.begin();
        std::list<SubElement>::iterator end = subElements.end();
        for (;it != end; ++it)
        {
            stream << it->tag;
            stream << it->length;
            for (uint i = 0; i < it->length; i++)
            {
                stream << (uint8_t)it->data[i];
            }
        }
    }

    return arr;
}

/*! Reads the otau file from a byte array.
    \return true on success or false false if no valid data was found
 */
bool OtauFile::fromArray(const QByteArray &arr)
{
    if (arr.size() < MANDATORY_HEADER_LENGTH)
    {
        return false;
    }

    const char *hdr = "\x1e\xf1\xee\x0b";

    int offset = arr.indexOf(hdr);
    if (offset < 0)
    {
        return false;
    }

    uint processedLength = 0;
    QDataStream stream(arr);
    stream.setByteOrder(QDataStream::LittleEndian);

    while (offset > 0)
    {
        quint8 dummy;
        stream >> dummy;
        offset--;
    }

    stream >> upgradeFileId;
    stream >> headerVersion;
    stream >> headerLength;

    if (headerLength < MANDATORY_HEADER_LENGTH)
    {
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
        if (!isprint(headerString[i]))
            headerString[i] = ' ';
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

    // read tags

    subElements.clear();

    while (!stream.atEnd())
    {
        SubElement sub;

        stream >> sub.tag;
        if (stream.atEnd())
        {
            break; // invalid tag
        }
        stream >> sub.length;

        while (!stream.atEnd())
        {
            if ((uint32_t)sub.data.size() == sub.length)
            {
                break;
            }

            uint8_t ch;
            stream >> ch;
            sub.data.append((char)ch);
        }

        if ((uint32_t)sub.data.size() == sub.length)
        {
            subElements.push_back(sub);
        }
        else
        {
            DBG_Printf(DBG_INFO_L2, "sub element size does not match real size\n");
        }
    }

    raw = arr;
    return !subElements.empty();
}
