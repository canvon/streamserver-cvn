#include "splitter.h"

#include "tspacket.h"
#include "tsreader.h"
#include "tswriter.h"
#include "log.h"

#include <stdexcept>
#include <QPointer>
#include <QHash>
#include <QFile>
#include <QDebug>
#include <QCoreApplication>

using log::verbose;

class SplitterImpl {
    QPointer<QFile>              _inputFilePtr;
    std::unique_ptr<TS::Reader>  _tsReaderPtr;
    QList<Splitter::Output>      _outputRequests, _outputResults;
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
    for (const Output &theOut : requests) {
        if (!theOut.outputFile)
            throw std::invalid_argument("Splitter: Set outputs: Output file can't be null");
        switch (theOut.start.startKind) {
        case StartKind::None:
            throw std::invalid_argument("Splitter: Set outputs: Start kind can't be none");
        case StartKind::Offset:
            if (!(theOut.start.startOffset >= 0))
                throw std::invalid_argument("Splitter: Set outputs: Start offset must be positive or zero");
            break;
        case StartKind::Packet:
            if (!(theOut.start.startPacket >= 1))
                throw std::invalid_argument("Splitter: Set outputs: Start packet must be positive");
            break;
        case StartKind::DiscontinuitySegment:
            if (!(theOut.start.startDiscontSegment >= 1))
                throw std::invalid_argument("Splitter: Set outputs: Start discontinuity segment must be positive");
            break;
        default:
            throw std::invalid_argument("Splitter: Set outputs: Invalid start kind " + std::to_string((int)theOut.start.startKind));
        }

        switch (theOut.length.lenKind) {
        case LengthKind::None:
            throw std::invalid_argument("Splitter: Set outputs: Length kind can't be none");
        case LengthKind::Bytes:
            if (!(theOut.length.lenBytes >= 0))
                throw std::invalid_argument("Splitter: Set outputs: Length in bytes must be positive or zero");
            break;
        case LengthKind::Packets:
            if (!(theOut.length.lenPackets >= 0))
                throw std::invalid_argument("Splitter: Set outputs: Length in packets must be positive or zero");
            break;
        case LengthKind::DiscontinuitySegments:
            if (!(theOut.length.lenDiscontSegments >= 0))
                throw std::invalid_argument("Splitter: Set outputs: Length in discontinuity segments must be positive or zero");
            break;
        default:
            throw std::invalid_argument("Splitter: Set outputs: Invalid length kind " + std::to_string((int)theOut.length.lenKind));
        }
    }

    _implPtr->_outputRequests = requests;
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
    for (Output &theOut : _implPtr->_outputRequests) {
        QFile &outputFile(*theOut.outputFile);
        Output &result(_implPtr->findOrDefaultOutputResult(&outputFile));

        // Started, yet?
        bool isStarted = false;
        switch (theOut.start.startKind) {
        case StartKind::Offset:
            if (theOut.start.startOffset <= packetOffset)
                isStarted = true;
            break;
        case StartKind::Packet:
            if (theOut.start.startPacket <= packetCount)
                isStarted = true;
            break;
        case StartKind::DiscontinuitySegment:
            if (theOut.start.startDiscontSegment <= discontSegment)
                isStarted = true;
            break;
        default:
        {
            QString exMsg;
            QDebug(&exMsg) << "Splitter: Unsupported output start kind" << theOut.start.startKind;
            throw std::runtime_error(exMsg.toStdString());
        }
        }
        if (!isStarted)
            continue;

        // Finished, already?
        bool isFinished = false;
        switch (theOut.length.lenKind) {
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
            if (!(result.length.lenBytes < theOut.length.lenBytes))
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
            if (!(result.length.lenPackets < theOut.length.lenPackets))
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
            if (!(result.length.lenDiscontSegments < theOut.length.lenDiscontSegments))
                isFinished = true;
            break;
        default:
        {
            QString exMsg;
            QDebug(&exMsg) << "Splitter: Unsupported output length kind" << theOut.length.lenKind;
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
}

void Splitter::handleDiscontEncountered(double pcrPrev)
{
    TS::Reader &reader(*_implPtr->_tsReaderPtr);
    const qint64 currentOffset = reader.tsPacketOffset();
    const double pcrLast       = reader.pcrLast();

    if (verbose >= 0) {
        qInfo().nospace()
            << "[" << currentOffset << "] "
            << "Discontinuity encountered "
            << "(" << pcrPrev << " -> " << pcrLast << "): "
            << "Input switches to segment " << reader.discontSegment();
    }

    for (Output &outputResult : _implPtr->_outputResults) {
        if (outputResult.length.lenKind == LengthKind::DiscontinuitySegments)
            outputResult.length.lenDiscontSegments++;
    }

    // TODO: Allow adding segment-based output files dynamically.
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
