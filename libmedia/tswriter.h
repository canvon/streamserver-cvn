#ifndef TSWRITER_H
#define TSWRITER_H

#include <QObject>

#include <memory>
#include <QIODevice>

class TSPacket;

namespace TS {

namespace impl {
class WriterImpl;
}

class Writer : public QObject
{
    Q_OBJECT
    std::unique_ptr<impl::WriterImpl>  _implPtr;

public:
    explicit Writer(QIODevice *dev, QObject *parent = 0);
    ~Writer();

    bool tsStripAdditionalInfo() const;
    void setTSStripAdditionalInfo(bool strip);

signals:
    void errorEncountered(QString errorMessage);

public slots:
    void queueTSPacket(const TSPacket &packet);
    void writeData();
};

}  // namespace TS

#endif // TSWRITER_H
