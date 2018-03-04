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
        if (!(theOut.startOffset >= 0))
            throw std::invalid_argument("Splitter: Set outputs: Start offset must be positive or zero");
        if (!(theOut.lenPackets >= 0))
            throw std::invalid_argument("Splitter: Set outputs: Length in packets must be positive or zero");
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
        if (!(theOut.startOffset <= currentOffset))
            continue;

        // Finished, already?
        if (!(currentOffset < theOut.startOffset + theOut.lenPackets * packetSize)) {
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
