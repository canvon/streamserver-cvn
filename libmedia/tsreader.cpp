#include "tsreader.h"

#include "tspacket.h"

#include <stdexcept>
#include <QByteArray>
#include <QPointer>
#include <QFile>
#include <QSocketNotifier>

namespace TS {

namespace impl {
class ReaderImpl {
    QPointer<QIODevice>               _devPtr;
    std::unique_ptr<QSocketNotifier>  _notifierPtr;
    QByteArray                        _buf;
    qint64                            _tsPacketSize = TSPacket::lengthBasic;
    qint64                            _tsPacketOffset = 0;
    friend Reader;

public:
    explicit ReaderImpl(QIODevice *dev) : _devPtr(dev)
    {

    }
};
}  // namespace TS::impl

Reader::Reader(QIODevice *dev, QObject *parent) :
    QObject(parent), _implPtr(std::make_unique<impl::ReaderImpl>(dev))
{
    if (!dev)
        throw std::invalid_argument("TS reader ctor: Device can't be null");

    // Set up signals.
    auto filePtr = dynamic_cast<QFile *>(dev);
    if (filePtr) {
        _implPtr->_notifierPtr = std::make_unique<QSocketNotifier>(filePtr->handle(), QSocketNotifier::Read, this);
        connect(_implPtr->_notifierPtr.get(), &QSocketNotifier::activated, this, &Reader::readData);
    }
    else {
        connect(dev, &QIODevice::readyRead, this, &Reader::readData);
    }
}

Reader::~Reader()
{

}

qint64 Reader::tsPacketSize() const
{
    return _implPtr->_tsPacketSize;
}

void Reader::setTSPacketSize(qint64 size)
{
    if (!(size >= TSPacket::lengthBasic))
        throw std::invalid_argument("TS reader: Set TS packet size: Invalid size " + std::to_string(size));

    _implPtr->_tsPacketSize = size;
}

qint64 Reader::tsPacketOffset() const
{
    return _implPtr->_tsPacketOffset;
}

void Reader::readData()
{
    if (!_implPtr->_devPtr) {
        emit errorEncountered(ErrorKind::IO, "Device is gone");
        return;
    }
    QIODevice &dev(*_implPtr->_devPtr);
    QByteArray &buf(_implPtr->_buf);

    do {
        int bufLenPrev   = buf.length();
        int bufLenTarget = _implPtr->_tsPacketSize;
        if (bufLenPrev < bufLenTarget) {
            buf.resize(bufLenTarget);
            qint64 readResult = dev.read(buf.data() + bufLenPrev, bufLenTarget - bufLenPrev);
            if (readResult < 0) {
                buf.resize(bufLenPrev);
                emit errorEncountered(ErrorKind::IO, dev.errorString());
                return;
            }
            else if (readResult == 0) {
                buf.resize(bufLenPrev);
                emit eofEncountered();
                return;
            }
            else if (bufLenPrev + readResult < bufLenTarget) {
                // Short read. Guess all we can do is return...
                buf.resize(bufLenPrev + readResult);
                return;
            }

            // A full read!
        }

        // Try to interpret as TS packet.
        TSPacket packet(buf);
        buf.clear();

        const QString errMsg = packet.errorMessage();
        if (!errMsg.isNull()) {
            emit errorEncountered(ErrorKind::TS, errMsg);
        }

        emit tsPacketReady(packet);

        _implPtr->_tsPacketOffset += packet.bytes().length();
    } while (true);
}

}  // namespace TS
