#ifndef SPLITTER_H
#define SPLITTER_H

#include <QObject>

#include "conversionstore.h"
#include "tspacket_compat.h"
#include "tsreader.h"
#include "numericrange.h"

#include <memory>
#include <QString>
#include <QList>
#include <QDebug>

class QFile;

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

        void setStartOffsetOnce(qint64 offset);
        void setStartPacketOnce(qint64 packet);
        void setStartDiscontSegmentOnce(int segment);
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

        qint64 lenBytesOrDefault();
        qint64 lenPacketsOrDefault();
        int    lenDiscontSegmentsOrDefault();

        void setLenBytesOnce(qint64 len);
        void setLenPacketsOnce(qint64 len);
        void setLenDiscontSegmentsOnce(int len);
    };

    struct Output {
        QFile  *outputFile;
        Start   start;
        Length  length;
    };

    enum class TemplateKind {
        None = 0,
        DiscontinuitySegments,
    };
    Q_ENUM(TemplateKind)

    struct OutputTemplate {
        TemplateKind  outputFilesKind = TemplateKind::None;
        QString       outputFilesFormatString;
        typedef HumanReadable::NumericRange<int>  range_type;
        //QList<range_type>                         filter;
        HumanReadable::NumericRangeList<int>      filter;
    };

    explicit Splitter(QObject *parent = 0);
    ~Splitter();

    TS::Reader *tsReader();
    const TS::Reader *tsReader() const;
    const QList<Output> &outputRequests() const;
    void setOutputRequests(const QList<Output> &requests);
    void appendDiscontSegmentOutputRequest(int discontSegment, const QString &fileFormatString);
    const QList<OutputTemplate> &outputTemplates() const;
    void setOutputTemplates(const QList<OutputTemplate> &templates);
    const QList<Output> &outputResults() const;

signals:

public slots:
    void openInput(QFile *inputFile);
    void handleTSPacketReady(const Upconvert<QByteArray, TS::Packet> &packetUpconvert);
    void handleDiscontEncountered(double pcrPrev);
    void handleSegmentStarts();
    void handleEOFEncountered();
    void handleErrorEncountered(TS::Reader::ErrorKind errorKind, QString errorMessage);
};

QDebug operator<<(QDebug debug, const Splitter::Start &start);
QDebug operator<<(QDebug debug, const Splitter::Length &length);
QDebug operator<<(QDebug debug, const Splitter::Output &output);
QDebug operator<<(QDebug debug, const Splitter::OutputTemplate &outTemplate);

#endif // SPLITTER_H
