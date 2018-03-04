#ifndef SPLITTER_H
#define SPLITTER_H

#include <QObject>

#include "tsreader.h"

#include <memory>
#include <QString>
#include <QList>

class QFile;
class TSPacket;

class SplitterImpl;

class Splitter : public QObject
{
    Q_OBJECT
    std::unique_ptr<SplitterImpl>  _implPtr;

public:
    struct Output {
        QFile  *outputFile;
        qint64  startOffset;
        int     lenPackets;
    };

    explicit Splitter(QObject *parent = 0);
    ~Splitter();

    TS::Reader *tsReader();
    const TS::Reader *tsReader() const;
    const QList<Output> &outputs() const;
    void setOutputs(const QList<Output> &outs);

signals:

public slots:
    void openInput(QFile *inputFile);
    void handleTSPacketReady(const TSPacket &packet);
    void handleEOFEncountered();
    void handleErrorEncountered(TS::Reader::ErrorKind errorKind, QString errorMessage);
};

#endif // SPLITTER_H
