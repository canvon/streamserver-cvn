#include <QCoreApplication>

#include "tspacket.h"

#include <QCommandLineParser>
#include <QFile>
#include <QTextStream>

namespace {
    QTextStream out(stdout), errout(stderr);
}

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    int ret = 0;
    qint64 tsPacketSize = TSPacket::lengthBasic;

    QCommandLineParser parser;
    parser.setApplicationDescription("Split MPEG-TS stream into files");
    parser.addHelpOption();
    parser.addOptions({
        { { "s", "ts-packet-size" },
          "MPEG-TS packet size (e.g., 188 bytes)",
          "SIZE" },
    });
    parser.process(a);

    // TS packet size
    {
        QString valueStr = parser.value("ts-packet-size");
        if (!valueStr.isNull()) {
            bool ok = false;
            tsPacketSize = valueStr.toLongLong(&ok);
            if (!ok) {
                errout << a.applicationName() << ": "
                       << "TS packet size: Conversion to number failed for \""
                       << valueStr << "\""
                       << endl;
                return 2;
            }
        }
    }

    auto args = parser.positionalArguments();
    if (!(args.length() > 0)) {
        errout << a.applicationName()
               << ": Invalid arguments"
               << endl;
        return 2;
    }

    //for (QString arg : args) {
    errout << a.applicationName() << ": Not implemented, yet!" << endl;

    //return a.exec();
    return ret;
}