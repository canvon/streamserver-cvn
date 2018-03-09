#include "splitter.h"

#include "tspacket.h"
#include "tsreader.h"
#include "tswriter.h"
#include "log.h"
#include "exceptionbuilder.h"

#include <string>
#include <stdexcept>
#include <QPointer>
#include <QHash>
#include <QStack>
#include <QFile>
#include <QDebug>
#include <QCoreApplication>

using log::verbose;

class SplitterImpl {
    QPointer<QFile>              _inputFilePtr;
    std::unique_ptr<TS::Reader>  _tsReaderPtr;
    QList<Splitter::Output>      _outputRequests, _outputResults;
    QList<Splitter::OutputTemplate>      _outputTemplates;
    typedef std::shared_ptr<TS::Writer>  _writerPtr_type;
    QHash<QFile *, _writerPtr_type>      _outputWriters;

    Splitter::Output &findOrDefaultOutputResult(QFile *outputFile);

    friend Splitter;
};

Splitter::Splitter(QObject *parent) :
    QObject(parent), _implPtr(std::make_unique<SplitterImpl>())
{

}

Splitter::~Splitter()
{

}

TS::Reader *Splitter::tsReader()
{
    return _implPtr->_tsReaderPtr.get();
}

const TS::Reader *Splitter::tsReader() const
{
    return _implPtr->_tsReaderPtr.get();
}

const QList<Splitter::Output> &Splitter::outputRequests() const
{
    return _implPtr->_outputRequests;
}

void Splitter::setOutputRequests(const QList<Splitter::Output> &requests)
{
    for (const Output &outRequest : requests) {
        if (!outRequest.outputFile)
            throw std::invalid_argument("Splitter: Set outputs: Output file can't be null");
        switch (outRequest.start.startKind) {
        case StartKind::None:
            throw std::invalid_argument("Splitter: Set outputs: Start kind can't be none");
        case StartKind::Offset:
            if (!(outRequest.start.startOffset >= 0))
                throw std::invalid_argument("Splitter: Set outputs: Start offset must be positive or zero");
            break;
        case StartKind::Packet:
            if (!(outRequest.start.startPacket >= 1))
                throw std::invalid_argument("Splitter: Set outputs: Start packet must be positive");
            break;
        case StartKind::DiscontinuitySegment:
            if (!(outRequest.start.startDiscontSegment >= 1))
                throw std::invalid_argument("Splitter: Set outputs: Start discontinuity segment must be positive");
            break;
        default:
            throw std::invalid_argument("Splitter: Set outputs: Invalid start kind " + std::to_string((int)outRequest.start.startKind));
        }

        switch (outRequest.length.lenKind) {
        case LengthKind::None:
            throw std::invalid_argument("Splitter: Set outputs: Length kind can't be none");
        case LengthKind::Bytes:
            if (!(outRequest.length.lenBytes >= 0))
                throw std::invalid_argument("Splitter: Set outputs: Length in bytes must be positive or zero");
            break;
        case LengthKind::Packets:
            if (!(outRequest.length.lenPackets >= 0))
                throw std::invalid_argument("Splitter: Set outputs: Length in packets must be positive or zero");
            break;
        case LengthKind::DiscontinuitySegments:
            if (!(outRequest.length.lenDiscontSegments >= 0))
                throw std::invalid_argument("Splitter: Set outputs: Length in discontinuity segments must be positive or zero");
            break;
        default:
            throw std::invalid_argument("Splitter: Set outputs: Invalid length kind " + std::to_string((int)outRequest.length.lenKind));
        }
    }

    _implPtr->_outputRequests = requests;
}

const QList<Splitter::OutputTemplate> &Splitter::outputTemplates() const
{
    return _implPtr->_outputTemplates;
}

void Splitter::setOutputTemplates(const QList<Splitter::OutputTemplate> &templates)
{
    const char *errPrefix = "Splitter: Set output templates:";
    for (const OutputTemplate &outTemplate : templates) {
        switch (outTemplate.outputFilesKind) {
        case TemplateKind::DiscontinuitySegments:
        {
            const char *errPrefix2 = "Discontinuity segment filter range:";
            for (const OutputTemplate::range_type &range : outTemplate.filter) {
                if (!(range.hasLowerBound && range.hasUpperBound))
                    // Partial range is always ok. (?)
                    continue;

                if (!(range.lowerBoundValue <= range.upperBoundValue)) {
                    throw static_cast<std::invalid_argument>(ExceptionBuilder()
                        << errPrefix << errPrefix2
                        << "Range is not ordered: From" << range.lowerBoundValue
                        << "to" << range.upperBoundValue);
                }
            }
            break;
        }
        default:
        {
            QString exMsg;
            QDebug(&exMsg) << errPrefix << "Invalid output files kind" << outTemplate.outputFilesKind;
            throw std::invalid_argument(exMsg.toStdString());
        }
        }

        const QString &format(outTemplate.outputFilesFormatString);
        if (format.isEmpty())
            throw std::invalid_argument(errPrefix + std::string(" Invalid output files format string: Can't be empty"));

        const QString example = QString::asprintf(qPrintable(format), 1);
        if (example.isEmpty()) {
            QString exMsg;
            QDebug(&exMsg) << errPrefix << "Invalid output files format string: Result for running with single number argument was empty:"
                           << format;
            throw std::invalid_argument(exMsg.toStdString());
        }

        if (verbose >= 1) {
            qInfo() << "Splitter: Output files format string" << format
                    << "will expand to, e.g.:" << example;
        }
    }

    _implPtr->_outputTemplates = templates;
}

const QList<Splitter::Output> &Splitter::outputResults() const
{
    return _implPtr->_outputResults;
}

Splitter::Output &SplitterImpl::findOrDefaultOutputResult(QFile *outputFile)
{
    for (Splitter::Output &output : _outputResults) {
        if (output.outputFile == outputFile)
            return output;
    }

    Splitter::Output output;
    output.outputFile = outputFile;
    _outputResults.append(output);
    return _outputResults.last();
}

void Splitter::openInput(QFile *inputFile)
{
    if (!inputFile)
        throw std::invalid_argument("Splitter: Open input: Input file can't be null");

    _implPtr->_inputFilePtr = inputFile;

    if (!inputFile->open(QIODevice::ReadOnly)) {
        qFatal("Splitter: Error opening input file \"%s\": %s",
               qPrintable(inputFile->fileName()),
               qPrintable(inputFile->errorString()));
    }

    _implPtr->_tsReaderPtr = std::make_unique<TS::Reader>(inputFile, this);
    TS::Reader &reader(*_implPtr->_tsReaderPtr);

    // Set up signals.
    connect(&reader, &TS::Reader::tsPacketReady, this, &Splitter::handleTSPacketReady);
    connect(&reader, &TS::Reader::discontEncountered, this, &Splitter::handleDiscontEncountered);
    connect(&reader, &TS::Reader::eofEncountered, this, &Splitter::handleEOFEncountered);
    connect(&reader, &TS::Reader::errorEncountered, this, &Splitter::handleErrorEncountered);
}

void Splitter::handleTSPacketReady(const TSPacket &packet)
{
    TS::Reader &reader(*_implPtr->_tsReaderPtr);
    const qint64 packetOffset = reader.tsPacketOffset();
    const qint64 packetCount  = reader.tsPacketCount();
    const int discontSegment  = reader.discontSegment();

    // Dump.
    if (verbose >= 2) {
        qInfo().nospace()
            << "[" << packetOffset << "] "
            << "Packet: " << packet;
    }

    // Conditionally forward to output files.
    QStack<int> removeRequestIndices;
    QList<Output> &outputRequests(_implPtr->_outputRequests);
    for (int i = 0, len = outputRequests.length(); i < len; i++) {
        Output &outRequest(outputRequests[i]);
        QFile &outputFile(*outRequest.outputFile);
        Output &result(_implPtr->findOrDefaultOutputResult(&outputFile));

        // Started, yet?
        bool isStarted = false;
        switch (outRequest.start.startKind) {
        case StartKind::Offset:
            if (outRequest.start.startOffset <= packetOffset)
                isStarted = true;
            break;
        case StartKind::Packet:
            if (outRequest.start.startPacket <= packetCount)
                isStarted = true;
            break;
        case StartKind::DiscontinuitySegment:
            if (outRequest.start.startDiscontSegment <= discontSegment)
                isStarted = true;
            break;
        default:
        {
            QString exMsg;
            QDebug(&exMsg) << "Splitter: Unsupported output start kind" << outRequest.start.startKind;
            throw std::runtime_error(exMsg.toStdString());
        }
        }
        if (!isStarted)
            continue;

        // Finished, already?
        bool isFinished = false;
        switch (outRequest.length.lenKind) {
        case LengthKind::Bytes:
            if (result.length.lenKind == LengthKind::None) {
                result.length.lenKind = LengthKind::Bytes;
                result.length.lenBytes = 0;
            }
            if (result.length.lenKind != LengthKind::Bytes) {
                QString errMsg;
                QDebug(&errMsg) << "Splitter: Output result length kind expected to be bytes, but was" << result.length.lenKind;
                qFatal("%s", qPrintable(errMsg));
            }
            if (!(result.length.lenBytes < outRequest.length.lenBytes))
                isFinished = true;
            break;
        case LengthKind::Packets:
            if (result.length.lenKind == LengthKind::None) {
                result.length.lenKind = LengthKind::Packets;
                result.length.lenPackets = 0;
            }
            if (result.length.lenKind != LengthKind::Packets) {
                QString errMsg;
                QDebug(&errMsg) << "Splitter: Output result length kind expected to be packets, but was" << result.length.lenKind;
                qFatal("%s", qPrintable(errMsg));
            }
            if (!(result.length.lenPackets < outRequest.length.lenPackets))
                isFinished = true;
            break;
        case LengthKind::DiscontinuitySegments:
            if (result.length.lenKind == LengthKind::None) {
                result.length.lenKind = LengthKind::DiscontinuitySegments;
                result.length.lenDiscontSegments = 0;
            }
            if (result.length.lenKind != LengthKind::DiscontinuitySegments) {
                QString errMsg;
                QDebug(&errMsg) << "Splitter: Output result length kind expected to be discontinuity segments, but was" << result.length.lenKind;
                qFatal("%s", qPrintable(errMsg));
            }
            if (!(result.length.lenDiscontSegments < outRequest.length.lenDiscontSegments))
                isFinished = true;
            break;
        default:
        {
            QString exMsg;
            QDebug(&exMsg) << "Splitter: Unsupported output length kind" << outRequest.length.lenKind;
            throw std::runtime_error(exMsg.toStdString());
        }
        }
        if (isFinished) {
            if (outputFile.isOpen()) {
                if (verbose >= 0) {
                    qInfo().nospace()
                        << "[" << packetOffset << "] "
                        << "Closing output file " << outputFile.fileName()
                        << "...";
                }

                outputFile.close();
            }

            // For efficiency, don't iterate over this output request anymore (but remove it).
            removeRequestIndices.push(i);

            continue;
        }

        SplitterImpl::_writerPtr_type writerPtr;
        if (!outputFile.isOpen()) {
            if (outputFile.exists())
                qFatal("Splitter: Output file exists: %s", qPrintable(outputFile.fileName()));

            if (verbose >= 0) {
                qInfo().nospace()
                    << "[" << packetOffset << "] "
                    << "Opening output file " << outputFile.fileName()
                    << "...";
            }

            if (!outputFile.open(QFile::WriteOnly)) {
                qFatal("Splitter: Error opening output file \"%s\": %s",
                       qPrintable(outputFile.fileName()),
                       qPrintable(outputFile.errorString()));
            }

            writerPtr = std::make_shared<TS::Writer>(&outputFile, this);
            _implPtr->_outputWriters.insert(&outputFile, writerPtr);
        }
        else {
            writerPtr = _implPtr->_outputWriters.value(&outputFile);
        }

        if (!writerPtr) {
            qFatal("Splitter: TS writer missing for output file \"%s\"",
                   qPrintable(outputFile.fileName()));
        }

        writerPtr->queueTSPacket(packet);
        writerPtr->writeData();

        switch (result.length.lenKind) {
        case LengthKind::Bytes:
            result.length.lenBytes += packet.bytes().length();
            break;
        case LengthKind::Packets:
            result.length.lenPackets++;
            break;
        default:
            // Ignore.
            break;
        }
    }
    while (!removeRequestIndices.isEmpty())
        outputRequests.removeAt(removeRequestIndices.pop());
}

void Splitter::handleDiscontEncountered(double pcrPrev)
{
    TS::Reader &reader(*_implPtr->_tsReaderPtr);
    const qint64 currentOffset = reader.tsPacketOffset();
    const double pcrLast       = reader.pcrLast();
    const int discontSegment   = reader.discontSegment();

    if (verbose >= 0) {
        qInfo().nospace()
            << "[" << currentOffset << "] "
            << "Discontinuity encountered "
            << "(" << pcrPrev << " -> " << pcrLast << "): "
            << "Input switches to segment " << discontSegment;
    }

    for (Output &outputResult : _implPtr->_outputResults) {
        // Only increase counts on output files that are currently open.
        if (!(outputResult.outputFile && outputResult.outputFile->isOpen()))
            continue;

        if (outputResult.length.lenKind == LengthKind::DiscontinuitySegments)
            outputResult.length.lenDiscontSegments++;
    }

    // Allow adding segment-based output files dynamically.
    for (OutputTemplate &outputTemplate : _implPtr->_outputTemplates) {
        switch (outputTemplate.outputFilesKind) {
        case TemplateKind::DiscontinuitySegments:
        {
            if (outputTemplate.filter.isEmpty()) {
                if (verbose >= 1) {
                    qInfo().nospace()
                        << "[" << currentOffset << "] "
                        << "Template " << outputTemplate.outputFilesFormatString
                        << " matches due to no filter present";
                }
            }
            else {
                bool found = false;
                for (const OutputTemplate::range_type &range : outputTemplate.filter) {
                    if (!(range.compare(discontSegment) == 0))
                        continue;

                    if (verbose >= 1) {
                        qInfo().nospace()
                            << "[" << currentOffset << "] "
                            << "Template " << outputTemplate.outputFilesFormatString
                            << " matches due to filter range " << range;
                    }

                    found = true;
                    break;
                }

                if (!found)
                    break;  // Break out of the switch, which will continue the for loop.
            }


            // Filter matches, so insert a dynamic output request.

            Output output;
            const QString fileName = QString::asprintf(qPrintable(outputTemplate.outputFilesFormatString), discontSegment);

            // Owned, to prevent resource leak.
            // The using code can get at the QFile from the output results
            // before this Splitter is destroyed, and can re-own the QFile
            // to prevent its automatic destruction.
            output.outputFile = new QFile(fileName, this);

            output.start.startKind = StartKind::DiscontinuitySegment;
            output.start.startDiscontSegment = discontSegment;
            output.length.lenKind = LengthKind::DiscontinuitySegments;
            output.length.lenDiscontSegments = 1;

            if (verbose >= 1) {
                qInfo().nospace()
                    << "[" << currentOffset << "] "
                    << "Adding dynamic output request for discontinuity segment " << discontSegment
                    << ": " << fileName;
            }

            _implPtr->_outputRequests.append(output);

            break;
        }
        default:
        {
            QString msg;
            QDebug(&msg) << "Splitter: Unimplemented output template kind" << outputTemplate.outputFilesKind;
            qFatal("%s", qPrintable(msg));
        }
        }
    }
}

void Splitter::handleEOFEncountered()
{
    qInfo() << "EOF";

    qApp->exit();
}

void Splitter::handleErrorEncountered(TS::Reader::ErrorKind errorKind, QString errorMessage)
{
    switch (errorKind) {
    case TS::Reader::ErrorKind::IO:
        qFatal("Splitter: IO error: %s", qPrintable(errorMessage));
    case TS::Reader::ErrorKind::TS:
        qWarning() << "Splitter: Ignoring TS error:" << errorMessage;
        break;
    }
}

QDebug operator<<(QDebug debug, const Splitter::Start &start)
{
    QDebugStateSaver saver(debug);
    debug.nospace();
    debug << "Splitter::Start(";
    debug << start.startKind;
    switch (start.startKind) {
    case Splitter::StartKind::None:
        break;
    case Splitter::StartKind::Offset:
        debug << " startOffset=" << start.startOffset;
        break;
    case Splitter::StartKind::Packet:
        debug << " startPacket=" << start.startPacket;
        break;
    case Splitter::StartKind::DiscontinuitySegment:
        debug << " startDiscontSegment=" << start.startDiscontSegment;
        break;
    }
    debug << ")";

    return debug;
}

QDebug operator<<(QDebug debug, const Splitter::Length &length)
{
    QDebugStateSaver saver(debug);
    debug.nospace();
    debug << "Splitter::Length(";
    debug << length.lenKind;
    switch (length.lenKind) {
    case Splitter::LengthKind::None:
        break;
    case Splitter::LengthKind::Bytes:
        debug << " lenBytes=" << length.lenBytes;
        break;
    case Splitter::LengthKind::Packets:
        debug << " lenPackets=" << length.lenPackets;
        break;
    case Splitter::LengthKind::DiscontinuitySegments:
        debug << " lenDiscontSegments=" << length.lenDiscontSegments;
        break;
    }
    debug << ")";

    return debug;
}

QDebug operator<<(QDebug debug, const Splitter::Output &output)
{
    QDebugStateSaver saver(debug);
    debug.nospace();
    debug << "Splitter::Output(";
    debug <<        output.start;
    debug << " " << output.length;
    if (!output.outputFile) {
        debug << " noFile";
    }
    else {
        debug << " fileName=" << output.outputFile->fileName();
    }
    debug << ")";

    return debug;
}

QDebug operator<<(QDebug debug, const Splitter::OutputTemplate &outTemplate)
{
    QDebugStateSaver saver(debug);
    debug.nospace();
    debug << "Splitter::OutputTemplate(";
    debug << outTemplate.outputFilesKind;
    debug << " filter=" << outTemplate.filter;
    debug << " formatString=" << outTemplate.outputFilesFormatString;
    debug << ")";

    return debug;
}
