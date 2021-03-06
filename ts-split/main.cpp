#include <QCoreApplication>

#include "splitter.h"
#include "tspacket.h"
#include "log_backend.h"
#include "humanreadable.h"
#include "numericconverter.h"

#include <functional>
#include <QCommandLineParser>
#include <QFile>
#include <QTextStream>

using SSCvn::log::verbose;
using SSCvn::log::debug_level;

// TODO: Convert to ambitious use of namespaces.
using namespace SSCvn;

namespace {
    QTextStream out(stdout), errout(stderr);

    using std::placeholders::_1;

    template <typename T, T(*Converter)(const QString &, bool *ok) = HumanReadable::numericConverter<T>>
    bool convertOptionToNum(const char *errPrefix,
        std::function<void(T)> setter,
        const QString &s,
        std::function<std::remove_pointer_t<decltype(Converter)>> converter = Converter)
    {
        try {
            bool ok = false;
            T value = converter(s, &ok);
            if (!ok) {
                qCritical() << errPrefix << "Can't convert to number:" << s;
                return false;
            }

            setter(value);
            return true;
        }
        catch (std::exception &ex) {
            qCritical() << errPrefix << ex.what();
            return false;
        }
        catch (...) {
            qCritical() << errPrefix << "(Unrecognized exception)";
            return false;
        }
    }
}

int main(int argc, char *argv[])
{
    log::backend::logoutPtr = &errout;
    qInstallMessageHandler(&log::backend::msgHandler);

    QCoreApplication a(argc, argv);
    qint64 tsPacketSize = 0;

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
        { "discontsegs-format", "Discontinuity segments file format string",
          "FMT" },
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
            "  fileName=FILENAME\n",
        "[--outfile OUTFILE]");
    parser.addPositionalArgument("OUTTEMPLATE", "Output files template description:\n"
            "Syntax: KEY1=VALUE1,KEY2=VALUE2,...,FINAL=REST\n"
            "Available keys/values:\n"
            "  discontSegments=RANGE1:RANGE2:...:RANGEN\n"
            "    (e.g., 1-7:9:11, or nothing)\n"
            "  fileFormat=PRINTF_FORMAT\n"
            "    (e.g., myout%03d.ts)\n",
        "[--outfiles-template OUTTEMPLATE]");
    parser.addPositionalArgument("DISCONTSEGSFORMAT", "Discontinuity segments file format string,\n"
            "this is an alias for: --outfiles-template discontSegments=,fileFormat=...\n",
        "[--discontsegs-format DISCONTSEGSFORMAT]");
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
        HumanReadable::KeyValueOption opt { outputDesc };
        Splitter::Output output;

        while (!opt.buf.isEmpty()) {
            const QString key = opt.takeKey();
            if (key.isEmpty()) {
                qCritical() << errPrefix << "Rest does not contain a key:" << opt.buf;
                return 2;
            }

            QString errPrefix2;
            QDebug(&errPrefix2).nospace() << errPrefix << " Key " << key << ":";

            if (key.compare("startOffset", Qt::CaseInsensitive) == 0) {
                if (!convertOptionToNum<qint64>(qPrintable(errPrefix2),
                        std::bind(&Splitter::Start::setStartOffsetOnce, &output.start, _1),
                        opt.takeValue()))
                    return 2;
            }
            else if (key.compare("startPacket", Qt::CaseInsensitive) == 0) {
                if (!convertOptionToNum<qint64>(qPrintable(errPrefix2),
                        std::bind(&Splitter::Start::setStartPacketOnce, &output.start, _1),
                        opt.takeValue()))
                    return 2;
            }
            else if (key.compare("startDiscontSegment", Qt::CaseInsensitive) == 0) {
                if (!convertOptionToNum<int>(qPrintable(errPrefix2),
                        std::bind(&Splitter::Start::setStartDiscontSegmentOnce, &output.start, _1),
                        opt.takeValue()))
                    return 2;
            }
            else if (key.compare("lenBytes", Qt::CaseInsensitive) == 0) {
                if (!convertOptionToNum<qint64>(qPrintable(errPrefix2),
                        std::bind(&Splitter::Length::setLenBytesOnce, &output.length, _1),
                        opt.takeValue()))
                    return 2;
            }
            else if (key.compare("lenPackets", Qt::CaseInsensitive) == 0) {
                if (!convertOptionToNum<qint64>(qPrintable(errPrefix2),
                        std::bind(&Splitter::Length::setLenPacketsOnce, &output.length, _1),
                        opt.takeValue()))
                    return 2;
            }
            else if (key.compare("lenDiscontSegments", Qt::CaseInsensitive) == 0) {
                if (!convertOptionToNum<int>(qPrintable(errPrefix2),
                        std::bind(&Splitter::Length::setLenDiscontSegmentsOnce, &output.length, _1),
                        opt.takeValue()))
                    return 2;
            }
            else if (key.compare("fileName", Qt::CaseInsensitive) == 0) {
                const QString fileName = opt.takeRest();
                if (fileName.isEmpty()) {
                    qCritical() << errPrefix << "fileName is empty:" << outputDesc;
                    return 2;
                }
                output.outputFile = new QFile(fileName, &a);
            }
            else {
                qCritical().nospace() << errPrefix << " Invalid key " << key << ": " << outputDesc;
                return 2;
            }
        }

        outputs.append(output);
    }

    // Output templates.
    QList<Splitter::OutputTemplate> outputTemplates;
    for (const QString &outTemplDesc : parser.values("outfiles-template")) {
        const char *errPrefix = "Invalid output template description:";
        HumanReadable::KeyValueOption opt { outTemplDesc };
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
                    try {
                        auto range = Splitter::OutputTemplate::range_type::fromString(rangeStr);
                        outTempl.filter.append(range);
                    }
                    catch (std::exception &ex) {
                        qCritical().nospace()
                            << errPrefix << " Key " << key << ": "
                            << "Value contains invalid range string " << rangeStr << ": "
                            << ex.what();
                        return 2;
                    }
                    catch (...) {
                        qCritical().nospace()
                            << errPrefix << " Key " << key << ": "
                            << "Value contains invalid range string " << rangeStr << ": "
                            << "(Unrecognized exception type)";
                        return 2;
                    }
                }
            }
            else if (key.compare("fileFormat", Qt::CaseInsensitive) == 0) {
                const QString fileFormat = opt.takeRest();
                if (fileFormat.isEmpty()) {
                    qCritical() << errPrefix << "fileFormat is empty:" << outTemplDesc;
                    return 2;
                }
                outTempl.outputFilesFormatString = fileFormat;
            }
            else {
                qCritical().nospace() << errPrefix << " Invalid key " << key << ": " << outTemplDesc;
                return 2;
            }  // if chain over key
        }  // while buf not empty

        outputTemplates.append(outTempl);
    }  // for outfiles-template options

    // Discontinuity segments format (alias for a specific output template).
    for (const QString &discontSegsFormat : parser.values("discontsegs-format")) {
        outputTemplates.append({ Splitter::TemplateKind::DiscontinuitySegments, discontSegsFormat });
    }

    const auto args = parser.positionalArguments();
    if (!(args.length() >= 1)) {
        qCritical() << "Input file missing!";
        return 2;
    }
    else if (args.length() > 1) {
        qCritical() << "Currently, only one input file is supported!";
        return 2;
    }
    const QString inputFileName = args.first();

    // If no output specifications have been given, default to
    // split by discontinuity segments, and derive file format string
    // from input file name.
    if (outputs.isEmpty() && outputTemplates.isEmpty()) {
        if (verbose >= 0) {
            qInfo() << "No output specifications given, constructing output file format string from input file name" << inputFileName << "...";
        }

        // Support chopping a single, well-known extension.
        const QStringList extensions { ".ts", ".m2ts" };
        QString inputFileStem = inputFileName;
        for (const QString &extension : extensions) {
            if (inputFileStem.endsWith(extension, Qt::CaseInsensitive)) {
                inputFileStem.chop(extension.length());
                break;
            }
        }

        if (verbose >= 1) {
            qInfo() << "Detected input file stem" << inputFileStem;
        }

        QString outputFileFormat = inputFileStem;

        // Escape percents in file name.
        outputFileFormat.replace('%', "%%");

        // Append a hopefully-useful digits expando + extension.
        outputFileFormat.append(".%03d.ts");

        // Create output template.
        outputTemplates.append({ Splitter::TemplateKind::DiscontinuitySegments, outputFileFormat });
    }

    if (verbose >= 1 && !outputs.isEmpty()) {
        qInfo() << "Output requests before run:";
        for (const Splitter::Output &output : outputs)
            qInfo() << output;
    }

    if (verbose >= 1 && !outputTemplates.isEmpty()) {
        qInfo() << "Output templates before run:";
        for (const Splitter::OutputTemplate &outputTemplate : outputTemplates)
            qInfo() << outputTemplate;
    }

    QFile inputFile(inputFileName, &a);
    Splitter splitter(&a);
    if (!outputs.isEmpty())
        splitter.setOutputRequests(outputs);
    if (!outputTemplates.isEmpty())
        splitter.setOutputTemplates(outputTemplates);
    splitter.openInput(&inputFile);
    if (tsPacketSize > 0) {
        TS::Reader &reader(*splitter.tsReader());
        reader.setTSPacketAutoSize(false);
        reader.setTSPacketSize(tsPacketSize);
    }

    int ret = a.exec();

    if (verbose >= 1) {
        qInfo() << "Output results after run:";
        for (const Splitter::Output &result : splitter.outputResults())
            qInfo() << result;
    }

    return ret;
}
