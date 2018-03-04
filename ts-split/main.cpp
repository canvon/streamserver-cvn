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
        { "outfile", "Output file description",
          "startOffset=START,lenPackets=LENPACKETS,fileName=FILENAME" },
    });
    parser.addPositionalArgument("INPUT", "Input file to split into parts");
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

    // Output files
    QList<Splitter::Output> outputs;
    for (const QString &outputDesc : parser.values("outfile")) {
        Splitter::Output output { nullptr, -1, -1 };
        int iComma = -1;
        int iStart = iComma + 1;
        while ((iComma = outputDesc.indexOf(',', iStart)) >= 0) {
            const QString field = outputDesc.mid(iStart, iComma - iStart);

            if (field.isEmpty()) {
                qCritical() << "Invalid output file description: Contains empty field:" << outputDesc;
                return 2;
            }

            int iFieldEq = field.indexOf('=');
            if (iFieldEq < 0) {
                qCritical() << "Invalid output file description: Field" << field << "doesn't have key=value structure:" << outputDesc;
                return 2;
            }

            const QString key   = field.mid(0, iFieldEq);
            const QString value = field.mid(iFieldEq + 1);

            if (key.compare("startOffset", Qt::CaseInsensitive) == 0) {
                bool ok = false;
                output.startOffset = value.toLongLong(&ok);
                if (!ok) {
                    qCritical().nospace() << "Invalid output file description: Key " << key << ": Can't convert value to number: " << value;
                    return 2;
                }
            }
            else if (key.compare("lenPackets", Qt::CaseInsensitive) == 0) {
                bool ok = false;
                output.lenPackets = value.toInt(&ok);
                if (!ok) {
                    qCritical().nospace() << "Invalid output file description: Key " << key << ": Can't convert value to number: " << value;
                    return 2;
                }
            }
            else {
                qCritical().nospace() << "Invalid output file description: Invalid key " << key << ": " << outputDesc;
                return 2;
            }

            iStart = iComma + 1;
        }

        const QString rest = outputDesc.mid(iStart);
        const QString fileNameIntro = "fileName=";
        if (!rest.startsWith(fileNameIntro, Qt::CaseInsensitive)) {
            qCritical() << "Invalid output file description: fileName designator missing:" << outputDesc;
            return 2;
        }

        const QString fileName = rest.mid(fileNameIntro.length());
        if (fileName.isEmpty()) {
            qCritical() << "Invalid output file description: fileName is empty:" << outputDesc;
            return 2;
        }
        output.outputFile = new QFile(fileName, &a);

        outputs.append(output);
    }
    if (outputs.isEmpty()) {
        qCritical() << "No --outfile descriptions specified";
        return 2;
    }

    auto args = parser.positionalArguments();
    if (!(args.length() == 1)) {
        qCritical() << "Input file missing";
        return 2;
    }

    QFile inputFile(args.first(), &a);
    Splitter splitter(&a);
    splitter.setOutputs(outputs);
    splitter.openInput(&inputFile);
    splitter.tsReader()->setTSPacketSize(tsPacketSize);

    return a.exec();
}
