#ifndef OTAU_FILE_LOADER_H
#define OTAU_FILE_LOADER_H

class QString;
struct OtauFile;

/*! \class OtauFileLoader

    Helper class to read and write otau files to and from the filesystem.
 */
class OtauFileLoader
{
public:
    OtauFileLoader();
    bool readFile(const QString &path, OtauFile &of);
    bool saveFile(const QString &path, OtauFile &of);
};

#endif // OTAU_FILE_LOADER_H
