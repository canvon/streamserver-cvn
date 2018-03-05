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
    QList<Splitter::Output>      _outputs;
    typedef std::shared_ptr<TS::Writer>  _writerPtr_type;
    QHash<QFile *, _writerPtr_type>      _outputWriters;
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

const QList<Splitter::Output> &Splitter::outputs() const
{
    return _implPtr->_outputs;
}

void Splitter::setOutputs(const QList<Splitter::Output> &outs)
{
    for (const Output &theOut : outs) {
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

    _implPtr->_outputs = outs;
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
    const qint64 packetSize    = _implPtr->_tsReaderPtr->tsPacketSize();
    const qint64 currentOffset = _implPtr->_tsReaderPtr->tsPacketOffset();

    // Dump.
    if (verbose >= 2) {
        qInfo().nospace()
            << "[" << currentOffset << "] "
            << "Packet: " << packet;
    }

    // Conditionally forward to output files.
    for (Output &theOut : _implPtr->_outputs) {
        QFile &outputFile(*theOut.outputFile);

        // Started, yet?
        qint64 startOffset;
        switch (theOut.start.startKind) {
        case StartKind::Offset:
            startOffset = theOut.start.startOffset;
            break;
        default:
        {
            QString exMsg;
            QDebug(&exMsg) << "Splitter: Unsupported output start kind" << theOut.start.startKind;
            throw std::runtime_error(exMsg.toStdString());
        }
        }
        if (!(startOffset <= currentOffset))
            continue;

        // Finished, already?
        qint64 pastEndOffset;
        switch (theOut.length.lenKind) {
        case LengthKind::Packets:
            pastEndOffset = startOffset + theOut.length.lenPackets * packetSize;
            break;
        default:
        {
            QString exMsg;
            QDebug(&exMsg) << "Splitter: Unsupported output length kind" << theOut.length.lenKind;
            throw std::runtime_error(exMsg.toStdString());
        }
        }
        if (!(currentOffset < pastEndOffset)) {
            if (outputFile.isOpen()) {
                if (verbose >= 0) {
                    qInfo().nospace()
                        << "[" << currentOffset << "] "
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
                    << "[" << currentOffset << "] "
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
    }
}

void Splitter::handleDiscontEncountered()
{
    TS::Reader &reader(*_implPtr->_tsReaderPtr);
    const qint64 currentOffset = reader.tsPacketOffset();

    if (verbose >= 0) {
        qInfo().nospace()
            << "[" << currentOffset << "] "
            << "Discontinuity encountered: Input switches to segment " << reader.discontSegment();
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
