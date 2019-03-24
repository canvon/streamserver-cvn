#include <QCoreApplication>

#ifndef TS_PACKET_V2
#include "tspacket.h"
#else
#include "tspacketv2.h"
#endif

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
    int verbose = 0;
    bool doOffset = false;
    qint64 tsPacketSize
#ifndef TS_PACKET_V2
        = TSPacket::lengthBasic;
#else
        = TS::PacketV2::sizeBasic;
#endif

    QCommandLineParser parser;
    parser.setApplicationDescription("Dump MPEG-TS packet contents");
    parser.addHelpOption();
    parser.addPositionalArgument("FILE", "File to parse as MPEG-TS stream", "FILE [...]");
    parser.addOptions({
        { { "v", "verbose" }, "Increase verbose level" },
        { { "q", "quiet"   }, "Decrease verbose level" },
        { "offset",
          "Output file offset of TS packet" },
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
    }

    // offset
    if (parser.isSet("offset"))
        doOffset = true;

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

#ifdef TS_PACKET_V2
    TS::PacketV2Parser tsParser;
    if (tsPacketSize != TS::PacketV2::sizeBasic)
        tsParser.setTSPacketSize(tsPacketSize);
#endif

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

        qint64 offset = 0, tsPacketCount = 0;
        QByteArray buf(tsPacketSize, 0);
        while (true) {
            if (doOffset)
                out << "offset=" << offset << " ";

            qint64 readResult = file.read(buf.data(), buf.size());
            if (readResult < 0) {
                if (doOffset)
                    out << "(err)" << endl;

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
                if (doOffset)
                    out << "(EOF)" << endl;
                break;
            }
            // TODO: Handle partial reads gracefully.
            else if (readResult != tsPacketSize) {
                if (doOffset)
                    out << "(short)" << endl;

                errout << a.applicationName()
                       << ": Got invalid bytes length of " << readResult
                       << " for file \"" << fileName << "\""
                       << endl;
                if (!(ret >= 1))
                    ret = 1;
                break;
            }

#ifndef TS_PACKET_V2
            TSPacket packet(buf);
            if (doOffset)
                out << "count=" << ++tsPacketCount << " ";

            if (verbose >= 0) {
                QString outStr;
                QDebug(&outStr) << packet;
                out << outStr << endl;
            }

            QString errMsg = packet.errorMessage();
            if (!errMsg.isEmpty()) {
                out << "^ TS packet error: " << errMsg << endl;
            }
#else
            TS::PacketV2 packet;
            QString errorMessage;
            bool success = tsParser.parse(buf, &packet, &errorMessage);
            if (doOffset)
                out << "count=" << ++tsPacketCount << " ";

            if (verbose >= 0) {
                QString outStr;
                QDebug(&outStr) << packet;
                out << outStr << endl;
            }

            if (!success)
                out << "^ TS packet error: " << errorMessage << endl;
#endif

            if (doOffset)
                offset += buf.length();
        }

        if (args.length() > 1)
            out << endl;
    }

    //return a.exec();
    return ret;
}
