#ifndef SPLITTER_H
#define SPLITTER_H

#include <QObject>

#include "tsreader.h"

#include <memory>
#include <QString>
#include <QList>
#include <QDebug>

class QFile;
class TSPacket;

class SplitterImpl;

class Splitter : public QObject
{
    Q_OBJECT
    std::unique_ptr<SplitterImpl>  _implPtr;

public:
    enum class StartKind {
        None = 0,
        Offset,
        Packet,
        DiscontinuitySegment,
    };
    Q_ENUM(StartKind)

    struct Start {
        StartKind  startKind = StartKind::None;
        union {
            qint64  startOffset;
            qint64  startPacket;
            int     startDiscontSegment;
        };
    };

    enum class LengthKind {
        None = 0,
        Bytes,
        Packets,
        DiscontinuitySegments,
    };
    Q_ENUM(LengthKind)

    struct Length {
        LengthKind  lenKind = LengthKind::None;
        union {
            qint64  lenBytes;
            qint64  lenPackets;
            int     lenDiscontSegments;
        };
    };

    struct Output {
        QFile  *outputFile;
        Start   start;
        Length  length;
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
    void handleDiscontEncountered(double pcrPrev);
    void handleEOFEncountered();
    void handleErrorEncountered(TS::Reader::ErrorKind errorKind, QString errorMessage);
};

QDebug operator<<(QDebug debug, const Splitter::Start &start);
QDebug operator<<(QDebug debug, const Splitter::Length &length);
QDebug operator<<(QDebug debug, const Splitter::Output &output);

#endif // SPLITTER_H
