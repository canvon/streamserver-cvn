#ifndef SPLITTER_H
#define SPLITTER_H

#include <QObject>

#include "tsreader.h"

#include <memory>
#include <QString>

class QFile;
class TSPacket;

class SplitterImpl;

class Splitter : public QObject
{
    Q_OBJECT
    std::unique_ptr<SplitterImpl>  _implPtr;

public:
    explicit Splitter(QObject *parent = 0);
    ~Splitter();

    TS::Reader *tsReader();
    const TS::Reader *tsReader() const;

signals:

public slots:
    void openInput(QFile *inputFile);
    void handleTSPacketReady(const TSPacket &packet);
    void handleEOFEncountered();
    void handleErrorEncountered(TS::Reader::ErrorKind errorKind, QString errorMessage);
};

#endif // SPLITTER_H
