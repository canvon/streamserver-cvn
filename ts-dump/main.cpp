#include <QCoreApplication>

#include "tspacket.h"
#include "tspacketv2.h"

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
    qint64 tsPacketSize = TSPacket::lengthBasic;
    int tsPacketClassVersion = 1;

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
        { "ts-packet-class-version",
          "Version of TS packet class to use: 1 or 2",
          "VERSION" },
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

    // TS packet class version
    {
        QString valueStr = parser.value("ts-packet-class-version");
        if (!valueStr.isNull()) {
            bool ok = false;
            tsPacketClassVersion = valueStr.toInt(&ok);
            if (!ok) {
                errout << a.applicationName() << ": "
                       << "TS packet class version: Can't convert to number: \""
                       << valueStr << "\""
                       << endl;
                return 2;
            }
            else if (!(1 <= tsPacketClassVersion && tsPacketClassVersion <= 2)) {
                errout << a.applicationName() << ": "
                       << "Invalid TS packet class version: Has to be 1 or 2, but got "
                       << tsPacketClassVersion
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

    std::unique_ptr<TS::PacketV2Parser> tsPacketV2ParserPtr;
    if (tsPacketClassVersion == 2) {
        tsPacketV2ParserPtr = std::make_unique<TS::PacketV2Parser>();

        if (tsPacketSize != TS::PacketV2::sizeBasic)
            tsPacketV2ParserPtr->setTSPacketSize(tsPacketSize);
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

            switch (tsPacketClassVersion) {
            case 1:
            {
                TSPacket packet(buf);
                if (doOffset)
                    out << "count=" << ++tsPacketCount << " ";

                QString outStr;
                QDebug(&outStr) << packet;
                out << outStr << endl;

                QString errMsg = packet.errorMessage();
                if (!errMsg.isEmpty()) {
                    out << "^ TS packet error: " << errMsg << endl;
                }

                break;
            }
            case 2:
            {
                if (!tsPacketV2ParserPtr) {
                    errout << a.applicationName() << ": "
                           << "TS packet v2 parser missing"
                           << endl;
                    return 1;
                }

                TS::PacketV2Parser::Parse parse;
                bool success = tsPacketV2ParserPtr->parse(buf, &parse);
                if (doOffset)
                    out << "count=" << ++tsPacketCount << " ";

                QString outStr;
                QDebug(&outStr) << parse.packet;
                out << outStr << endl;

                if (!success)
                    out << "^ TS packet error: " << parse.errorMessage << endl;

                break;
            }
            default:
                errout << a.applicationName() << ": "
                       << "Unsupported TS packet class version "
                       << tsPacketClassVersion
                       << endl;
                return 1;
            }

            if (doOffset)
                offset += buf.length();
        }

        if (args.length() > 1)
            out << endl;
    }

    //return a.exec();
    return ret;
}
