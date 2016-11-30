#ifndef OTAU_FILE_H
#define OTAU_FILE_H

#include <stdint.h>
#include <QtGlobal>
#include <QByteArray>
#include <list>

//!< Header field control bits
#define OF_FC_SECURITY_CREDENTIAL_VERSION 0x0001
#define OF_FC_DEVICE_SPECIFIC             0x0002
#define OF_FC_HARDWARE_VERSION            0x0004

#define TAG_UPGRADE_IMAGE 0x0000

/*! \class OtauFile

    Represents a otau file conform to the ZigBee specification.
 */
struct OtauFile
{
    OtauFile();
    QByteArray toArray();
    bool fromArray(const QByteArray &arr);

    struct SubElement
    {
        /*!
            Tag

            0x0000 Upgrade Image
            0x0001 ECDCA Signature
            0x0002 ECDCA Signing Certificate
            0x0003 - 0xefff Reserved
            0xf000 - 0xffff Manufacturer Specific
         */
        uint16_t tag;
        uint32_t length;
        QByteArray data;
    };

    QString path;

    uint32_t upgradeFileId; //!< 0x0BEEF11E (file magic number)
    uint16_t headerVersion; //!< currently 0x0100 (major 01, minor 00)
    uint16_t headerLength;
    uint16_t headerFieldControl;
    uint16_t manufacturerCode;

    /*!
        Image Type

        0x0000 - 0xffbf  Manufacturer specific (device unique)
                 0xffc0  Security credential
                 0xffc1  Configuration
                 0xffc2  Log
        0xffc3 - 0xfffe  reserved
                 0xffff  wildcard
     */
    uint16_t imageType;
    uint32_t fileVersion;
    /*!
        ZigBee Stack Version

        0x0000 ZigBee 2006
        0x0001 ZigBee 2007
        0x0002 ZigBee PRO
        0x0003 ZigBee IP
     */
    uint16_t zigBeeStackVersion;
    uint8_t headerString[32];
    uint32_t totalImageSize; //!< including header

    // optional fields (depend on field control)
    /*!
         Security Credential Version

         0x00 SE 1.0
         0x01 SE 1.1
         0x02 SE 2.0
     */
    uint8_t securityCredentialVersion;
    quint64 upgradeFileDestination;
    uint16_t minHardwareVersion;
    uint16_t maxHardwareVersion;

    std::list<SubElement> subElements;
};

#endif // OTAU_FILE_H
