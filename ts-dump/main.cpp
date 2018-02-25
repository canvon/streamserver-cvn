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
        if (args.length() > 1)
            out << fileName << ":" << endl;

        QFile file(fileName);
        if (!file.open(QIODevice::ReadOnly)) {
            errout << a.applicationName()
                   << ": Error opening file \"" << fileName << "\": "
                   << file.errorString()
                   << endl;
            return 1;
        }

        QByteArray buf(tsPacketLen, 0);
        while (true) {
            qint64 readResult = file.read(buf.data(), buf.size());
            if (readResult < 0) {
                errout << a.applicationName()
                       << ": Error reading from \"" << fileName << "\": "
                       << file.errorString()
                       << endl;
                if (!(ret >= 1))
                    ret = 1;
                break;
            }
            else if (readResult == 0) {
                // Reached EOF.
                break;
            }
            // TODO: Handle partial reads gracefully.
            else if (readResult != tsPacketLen) {
                errout << a.applicationName()
                       << ": Got invalid bytes length of " << readResult
                       << " for file \"" << fileName << "\""
                       << endl;
                if (!(ret >= 1))
                    ret = 1;
                break;
            }

            TSPacket packet(buf);
            QString outStr;
            QDebug(&outStr) << packet;
            out << outStr << endl;

            QString errMsg = packet.errorMessage();
            if (!errMsg.isEmpty()) {
                out << "^ TS packet error: " << errMsg << endl;
            }
        }

        if (args.length() > 1)
            out << endl;
    }

    //return a.exec();
    return ret;
}
