#include <QDataStream>
#include <QFile>
#include <QDebug>
#include "otau_file.h"
#include "otau_file_loader.h"

/*! The constructor.
 */
OtauFileLoader::OtauFileLoader()
{
}

/*! Reads a otau file from filesystem.
    \param path - the filepath
    \param of - reference to otau file which shall be filled
    \return true on success or false on error
 */
bool OtauFileLoader::readFile(const QString &path, OtauFile &of)
{
    QFile file(path);

    if (!file.open(QFile::ReadOnly))
    {
        qDebug() << Q_FUNC_INFO << file.errorString() << path;
        return false;
    }

    QByteArray arr = file.readAll();

    if (arr.isEmpty())
    {
        return false;
    }

    of.subElements.clear();

    of.path = path;

    if (path.endsWith(".bin") || path.endsWith(".GCF"))
    {
        OtauFile::SubElement sub;

        sub.tag = TAG_UPGRADE_IMAGE;

        // BitCloud specific internal container format within the upgrade image data

        // u32 memOffset
        // u32 length
        // u8  data[length]
        // ... 0..n more container ...
        // u8 CRC-8

        arr.append((char)0x77); // dummy temp

        { // write BitCloud container header
            QDataStream stream(&sub.data, QIODevice::WriteOnly);
            stream.setByteOrder(QDataStream::LittleEndian);

            stream << (uint32_t)0; // memOffset
            stream << (uint32_t)arr.size(); // length
        }

        sub.data.append(arr); // add real data

        char crc8 = 0; // TODO check for what the crc is calculated
        sub.data.append(crc8);

        sub.length = (uint32_t)sub.data.size();
        of.subElements.push_back(sub);
        return true;
    }
    else if (path.endsWith(".zigbee"))
    {
        return of.fromArray(arr);
    }
    else
    {
        return false;
    }
}

/*! Saves a otau file to the filesystem.
    \param path - the file path
    \param of - the otau file to write
    \return true on success false on error
 */
bool OtauFileLoader::saveFile(const QString &path, OtauFile &of)
{
    QFile file(path);

    if (!file.open(QFile::WriteOnly))
    {
        qDebug() << Q_FUNC_INFO << file.errorString() << path;
        return false;
    }

    QByteArray arr = of.toArray();

    if (file.write(arr) == -1)
    {
        return false;
    }

    return true;
}
