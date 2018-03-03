#include "splitter.h"

#include "tspacket.h"

#include <stdexcept>
#include <QPointer>
#include <QFile>
#include <QDebug>
#include <QCoreApplication>

class SplitterImpl {
    QPointer<QFile>              _inputFilePtr;
    std::unique_ptr<TS::Reader>  _tsReaderPtr;
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

void Splitter::openInput(QFile *inputFile)
{
    if (!inputFile)
        throw std::invalid_argument("Splitter: Open input: Input file can't be null");

    _implPtr->_inputFilePtr = inputFile;

    if (!inputFile->open(QIODevice::ReadOnly))
        qFatal("Splitter: Error opening input file: %s", qPrintable(inputFile->errorString()));

    _implPtr->_tsReaderPtr = std::make_unique<TS::Reader>(inputFile, this);
    TS::Reader &reader(*_implPtr->_tsReaderPtr);

    // Set up signals.
    connect(&reader, &TS::Reader::tsPacketReady, this, &Splitter::handleTSPacketReady);
    connect(&reader, &TS::Reader::eofEncountered, this, &Splitter::handleEOFEncountered);
    connect(&reader, &TS::Reader::errorEncountered, this, &Splitter::handleErrorEncountered);
}

void Splitter::handleTSPacketReady(const TSPacket &packet)
{
    // Dump, for now.
    qDebug() << packet;

    // TODO: Conditionally forward to output files.
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
