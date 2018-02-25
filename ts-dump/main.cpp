#include <QCoreApplication>

#include "tspacket.h"

#include <QCommandLineParser>
#include <QDebug>
#include <QFile>
#include <QTextStream>

namespace {
    QTextStream out(stdout), errout(stderr);
}

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    int ret = 0;
    qint64 tsPacketLen = TSPacket::lengthBasic;

    QCommandLineParser parser;
    parser.setApplicationDescription("Dump MPEG-TS packet contents");
    parser.process(a);

    auto args = parser.positionalArguments();
    if (!(args.length() > 0)) {
        errout << a.applicationName()
               << ": Invalid arguments"
               << endl;
        return 2;
    }

    for (QString arg : args) {
        QString fileName = arg;
        out << fileName << ":" << endl;

        QFile file(fileName);
        if (!file.open(QIODevice::ReadOnly)) {
            errout << a.applicationName()
                   << ": Error opening file \"" << fileName << "\": "
                   << file.errorString()
                   << endl;
            return 1;
        }

        while (!file.atEnd()) {
            QByteArray bytes = file.read(tsPacketLen);
            if (bytes.isNull()) {
                errout << a.applicationName()
                       << ": Error reading from \"" << fileName << "\": "
                       << file.errorString()
                       << endl;
                if (!(ret >= 1))
                    ret = 1;
                break;
            }
            else if (bytes.isEmpty()) {
                // Reached EOF. (?)
                break;
            }
            else if (bytes.length() != tsPacketLen) {
                errout << a.applicationName()
                       << ": Got invalid bytes length of " << bytes.length()
                       << " for file \"" << fileName << "\""
                       << endl;
                if (!(ret >= 1))
                    ret = 1;
                break;
            }

            TSPacket packet(bytes);
            QString outStr;
            QDebug(&outStr) << packet;
            out << outStr << endl;
        }

        out << endl;
    }

    //return a.exec();
    return ret;
}
