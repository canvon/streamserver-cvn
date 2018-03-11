#include "tswriter.h"

#include "tspacket.h"

#include <stdexcept>
#include <QByteArray>
#include <QPointer>
#include <QFile>
#include <QSocketNotifier>

namespace TS {

namespace impl {
class WriterImpl {
    QPointer<QIODevice>               _devPtr;
    std::unique_ptr<QSocketNotifier>  _notifierPtr;
    QByteArray                        _buf;
    bool                              _tsStripAdditionalInfo = false;
    friend Writer;

public:
    explicit WriterImpl(QIODevice *dev) : _devPtr(dev)
    {

    }
};
}  // namespace TS::impl

Writer::Writer(QIODevice *dev, QObject *parent) :
    QObject(parent), _implPtr(std::make_unique<impl::WriterImpl>(dev))
{
    if (!dev)
        throw std::invalid_argument("TS writer ctor: Device can't be null");

    // Set up signals.
    auto filePtr = dynamic_cast<QFile *>(dev);
    if (filePtr) {
        _implPtr->_notifierPtr = std::make_unique<QSocketNotifier>(filePtr->handle(), QSocketNotifier::Write, this);
        connect(_implPtr->_notifierPtr.get(), &QSocketNotifier::activated, this, &Writer::writeData);
    }
    else {
        connect(dev, &QIODevice::bytesWritten, this, &Writer::writeData);
    }
}

Writer::~Writer()
{

}

bool Writer::tsStripAdditionalInfo() const
{
    return _implPtr->_tsStripAdditionalInfo;
}

void Writer::setTSStripAdditionalInfo(bool strip)
{
    _implPtr->_tsStripAdditionalInfo = strip;
}

void Writer::queueTSPacket(const TSPacket &packet)
{
    _implPtr->_buf.append(_implPtr->_tsStripAdditionalInfo ?
        packet.toBasicPacketBytes() :
        packet.bytes()
    );

    // TODO: Have a maximum amount of data that can be queued.

    if (_implPtr->_notifierPtr)
        _implPtr->_notifierPtr->setEnabled(true);
}

void Writer::writeData()
{
    if (!_implPtr->_devPtr)
        std::runtime_error("TS writer: Write data: Device is gone!");
    QIODevice  &dev(*_implPtr->_devPtr);
    QByteArray &buf(_implPtr->_buf);

    while (!buf.isEmpty()) {
        // Try to send.
        qint64 writeResult = dev.write(buf);
        if (writeResult < 0) {
            emit errorEncountered(dev.errorString());
            return;
        }
        else if (writeResult == 0) {
            return;
        }
        //else if (writeResult < buf.length()) {
        //    // Short write. Guess all we can do is return... (?)
        //    buf.remove(0, writeResult);
        //    return;
        //}

        buf.remove(0, writeResult);
    }

    // Buffer now is empty.
    if (_implPtr->_notifierPtr)
        _implPtr->_notifierPtr->setEnabled(false);
}

}  // namespace TS