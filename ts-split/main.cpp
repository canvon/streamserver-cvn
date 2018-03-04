#include <QCoreApplication>

#include "splitter.h"
#include "tspacket.h"
#include "log_backend.h"

#include <QCommandLineParser>
#include <QFile>
#include <QTextStream>

using log::verbose;
using log::debug_level;

namespace {
    QTextStream out(stdout), errout(stderr);
}

int main(int argc, char *argv[])
{
    log::backend::logoutPtr = &errout;
    qInstallMessageHandler(&log::backend::msgHandler);

    QCoreApplication a(argc, argv);
    qint64 tsPacketSize = TSPacket::lengthBasic;

    QCommandLineParser parser;
    parser.setApplicationDescription("Split MPEG-TS stream into files");
    parser.addHelpOption();
    parser.addOptions({
        { { "v", "verbose" }, "Increase verbose level" },
        { { "q", "quiet"   }, "Decrease verbose level" },
        { { "d", "debug"   }, "Enable debugging. (Increase debug level.)" },
        { { "s", "ts-packet-size" },
          "MPEG-TS packet size (e.g., 188 bytes)",
          "SIZE" },
    });
    parser.process(a);

    // Apply incremental options.
    for (QString opt : parser.optionNames()) {
        if (opt == "v" || opt == "verbose")
            verbose++;
        else if (opt == "q" || opt == "quiet")
            verbose--;
        else if (opt == "d" || opt == "debug")
#ifdef QT_NO_DEBUG_OUTPUT
            qFatal("No debug output compiled in, can't enable debugging!");
#else
            debug_level++;
#endif
    }

    // TS packet size
    {
        QString valueStr = parser.value("ts-packet-size");
        if (!valueStr.isNull()) {
            bool ok = false;
            tsPacketSize = valueStr.toLongLong(&ok);
            if (!ok) {
                qCritical() << "Invalid TS packet size: Can't convert to number:" << valueStr;
                return 2;
            }
        }
    }

    auto args = parser.positionalArguments();
    if (!(args.length() == 1)) {
        qCritical() << "Invalid arguments";
        return 2;
    }

    QFile inputFile(args.first(), &a);
    Splitter splitter(&a);
    splitter.openInput(&inputFile);
    splitter.tsReader()->setTSPacketSize(tsPacketSize);

    return a.exec();
}
