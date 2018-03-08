#include <QCoreApplication>

#include "splitter.h"
#include "tspacket.h"
#include "log_backend.h"
#include "humanreadable.h"

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
          "DESCR" },
        { "outfiles-template", "Output files template description",
          "DESCR" },
    });
    parser.addPositionalArgument("OUTFILE", "Output file description:\n"
            "Syntax: KEY1=VALUE1,KEY2=VALUE2,...,FINAL=REST\n"
            "Available keys/values:\n"
            "  startOffset=OFFSET  OR\n"
            "  startPacket=NUMBER  OR\n"
            "  startDiscontSegment=NUMBER\n"
            "  lenBytes=NUMBER    OR\n"
            "  lenPackets=NUMBER  OR\n"
            "  lenDiscontSegments=NUMBER\n"
            "  fileName=FILENAME",
        "[--outfile OUTFILE]");
    parser.addPositionalArgument("OUTTEMPLATE", "Output files template description:"
        "\n--outfiles-template discontSegments=[1-7:3:11],fileFormat=myout%03d.ts",
        "[--outfiles-template OUTTEMPLATE]");
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
        const char *errPrefix = "Invalid output file description:";
        Splitter::Output output;
        int iComma = -1;
        int iStart = iComma + 1;
        while ((iComma = outputDesc.indexOf(',', iStart)) >= 0) {
            const QString field = outputDesc.mid(iStart, iComma - iStart);

            if (field.isEmpty()) {
                qCritical() << errPrefix << "Contains empty field:" << outputDesc;
                return 2;
            }

            int iFieldEq = field.indexOf('=');
            if (iFieldEq < 0) {
                qCritical() << errPrefix << "Field" << field << "doesn't have key=value structure:" << outputDesc;
                return 2;
            }

            const QString key   = field.mid(0, iFieldEq);
            const QString value = field.mid(iFieldEq + 1);

            auto checkIsStartKindNone = [&]() {
                if (output.start.startKind == Splitter::StartKind::None)
                    return true;

                qCritical().nospace()
                    << errPrefix << " Key " << key << ": "
                    << "Start kind was already set to " << output.start.startKind
                    << ": " << value;
                return false;
            };
            auto checkIsLengthKindNone = [&]() {
                if (output.length.lenKind == Splitter::LengthKind::None)
                    return true;

                qCritical().nospace()
                    << errPrefix << " Key " << key << ": "
                    << "Length kind was already set to " << output.length.lenKind
                    << ": " << value;
                return false;
            };

            if (key.compare("startOffset", Qt::CaseInsensitive) == 0) {
                if (!checkIsStartKindNone())
                    return 2;
                bool ok = false;
                output.start.startKind = Splitter::StartKind::Offset;
                output.start.startOffset = value.toLongLong(&ok);
                if (!ok) {
                    qCritical().nospace() << errPrefix << " Key " << key << ": Can't convert value to number: " << value;
                    return 2;
                }
            }
            else if (key.compare("startPacket", Qt::CaseInsensitive) == 0) {
                if (!checkIsStartKindNone())
                    return 2;
                bool ok = false;
                output.start.startKind = Splitter::StartKind::Packet;
                output.start.startPacket = value.toLongLong(&ok);
                if (!ok) {
                    qCritical().nospace() << errPrefix << " Key " << key << ": Can't convert value to number: " << value;
                    return 2;
                }
            }
            else if (key.compare("startDiscontSegment", Qt::CaseInsensitive) == 0) {
                if (!checkIsStartKindNone())
                    return 2;
                bool ok = false;
                output.start.startKind = Splitter::StartKind::DiscontinuitySegment;
                output.start.startDiscontSegment = value.toLongLong(&ok);
                if (!ok) {
                    qCritical().nospace() << errPrefix << " Key " << key << ": Can't convert value to number: " << value;
                    return 2;
                }
            }
            else if (key.compare("lenBytes", Qt::CaseInsensitive) == 0) {
                if (!checkIsLengthKindNone())
                    return 2;
                bool ok = false;
                output.length.lenKind = Splitter::LengthKind::Bytes;
                output.length.lenBytes = value.toInt(&ok);
                if (!ok) {
                    qCritical().nospace() << errPrefix << " Key " << key << ": Can't convert value to number: " << value;
                    return 2;
                }
            }
            else if (key.compare("lenPackets", Qt::CaseInsensitive) == 0) {
                if (!checkIsLengthKindNone())
                    return 2;
                bool ok = false;
                output.length.lenKind = Splitter::LengthKind::Packets;
                output.length.lenPackets = value.toInt(&ok);
                if (!ok) {
                    qCritical().nospace() << errPrefix << " Key " << key << ": Can't convert value to number: " << value;
                    return 2;
                }
            }
            else if (key.compare("lenDiscontSegments", Qt::CaseInsensitive) == 0) {
                if (!checkIsLengthKindNone())
                    return 2;
                bool ok = false;
                output.length.lenKind = Splitter::LengthKind::DiscontinuitySegments;
                output.length.lenDiscontSegments = value.toInt(&ok);
                if (!ok) {
                    qCritical().nospace() << errPrefix << " Key " << key << ": Can't convert value to number: " << value;
                    return 2;
                }
            }
            else {
                qCritical().nospace() << errPrefix << " Invalid key " << key << ": " << outputDesc;
                return 2;
            }

            iStart = iComma + 1;
        }

        const QString rest = outputDesc.mid(iStart);
        const QString fileNameIntro = "fileName=";
        if (!rest.startsWith(fileNameIntro, Qt::CaseInsensitive)) {
            qCritical() << errPrefix << "fileName designator missing:" << outputDesc;
            return 2;
        }

        const QString fileName = rest.mid(fileNameIntro.length());
        if (fileName.isEmpty()) {
            qCritical() << errPrefix << "fileName is empty:" << outputDesc;
            return 2;
        }
        output.outputFile = new QFile(fileName, &a);

        outputs.append(output);
    }

    // Output templates.
    QList<Splitter::OutputTemplate> outputTemplates;
    for (const QString &outTemplDescr : parser.values("outfiles-template")) {
        const char *errPrefix = "Invalid output template description:";
        HumanReadable::KeyValueOption opt { outTemplDescr };
        Splitter::OutputTemplate outTempl;

        while (!opt.buf.isEmpty()) {
            const QString key = opt.takeKey();
            if (key.isEmpty()) {
                qCritical() << errPrefix << "Rest does not contain a key:" << opt.buf;
                return 2;
            }
            else if (key.compare("discontSegments", Qt::CaseInsensitive) == 0) {
                if (outTempl.outputFilesKind != Splitter::TemplateKind::None) {
                    qCritical() << errPrefix << "Output template kind has already been set to" << outTempl.outputFilesKind;
                    return 2;
                }
                outTempl.outputFilesKind = Splitter::TemplateKind::DiscontinuitySegments;
                const QString value = opt.takeValue();
                const QStringList rangeStrs = value.isEmpty() ? QStringList() : value.split(':');  // (No rangeStrs when empty.)
                for (const QString &rangeStr : rangeStrs) {
                    Splitter::OutputTemplate::range_type range;
                    const QStringList rangeBounds = rangeStr.split('-');
                    switch (rangeBounds.length()) {
                    case 1:
                    {
                        const QString &bound(rangeBounds.first());
                        if (bound.isEmpty()) {
                            qCritical().nospace()
                                << errPrefix << " Key " << key << ": "
                                << "Value contains empty range: " << value;
                            return 2;
                        }
                        bool ok = false;
                        int boundNum = bound.toInt(&ok);
                        if (!ok) {
                            qCritical().nospace()
                                << errPrefix << " Key " << key << ": "
                                << "Can't convert part of range list to number: " << bound;
                        }

                        range.first = QVariant(boundNum);
                        range.second = range.first;
                        break;
                    }
                    case 2:
                    {
                        const QString &from(rangeBounds.first());
                        const QString &to(rangeBounds.last());
                        if (from.isEmpty()) {
                            range.first = QVariant();
                        }
                        else {
                            bool ok = false;
                            int fromNum = from.toInt(&ok);
                            if (!ok) {
                                qCritical().nospace()
                                    << errPrefix << " Key " << key << ": "
                                    << "Can't convert lower bound of range to number: " << rangeStr;
                                return 2;
                            }
                            range.first = QVariant(fromNum);
                        }
                        if (to.isEmpty()) {
                            range.second = QVariant();
                        }
                        else {
                            bool ok = false;
                            int toNum = to.toInt(&ok);
                            if (!ok) {
                                qCritical().nospace()
                                    << errPrefix << " Key " << key << ": "
                                    << "Can't convert upper bound of range to number: " << rangeStr;
                                return 2;
                            }
                            range.second = QVariant(toNum);
                        }
                        break;
                    }
                    default:
                        qCritical().nospace()
                            << errPrefix << " Key " << key << ": "
                            << "Value contains invalid range: " << value;
                        return 2;
                    }

                    outTempl.filter.append(range);
                }
            }
            else if (key.compare("fileFormat", Qt::CaseInsensitive) == 0) {
                outTempl.outputFilesFormatString = opt.takeRest();
            }
            else {
                qCritical() << errPrefix << "Invalid key" << key;
                return 2;
            }  // if chain over key
        }  // while buf not empty

        outputTemplates.append(outTempl);
    }  // for outfiles-template options

    // Sanity check.
    if (outputs.isEmpty() && outputTemplates.isEmpty()) {
        qCritical() << "No --outfile/--outfiles-template descriptions specified";
        return 2;
    }
    else {
        if (verbose >= 1 && !outputs.isEmpty()) {
            qInfo() << "Output requests before run:";
            for (const Splitter::Output &output : outputs)
                qInfo() << output;
        }

#if 0  // FIXME: Implement QDebug for Splitter::OutputTemplate
        if (verbose >= 1 && !outputTemplates.isEmpty()) {
            qInfo() << "Output templates before run:";
            for (const Splitter::OutputTemplate &outputTemplate : outputTemplates)
                qInfo() << outputTemplate;
        }
#endif
    }

    auto args = parser.positionalArguments();
    if (!(args.length() == 1)) {
        qCritical() << "Input file missing";
        return 2;
    }

    QFile inputFile(args.first(), &a);
    Splitter splitter(&a);
    if (!outputs.isEmpty())
        splitter.setOutputRequests(outputs);
    if (!outputTemplates.isEmpty())
        splitter.setOutputTemplates(outputTemplates);
    splitter.openInput(&inputFile);
    splitter.tsReader()->setTSPacketSize(tsPacketSize);

    int ret = a.exec();

    if (verbose >= 1) {
        qInfo() << "Output results after run:";
        for (const Splitter::Output &result : splitter.outputResults())
            qInfo() << result;
    }

    return ret;
}
