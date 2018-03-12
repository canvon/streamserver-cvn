#include "tspacket2.h"

#include "humanreadable.h"
#include "exceptionbuilder.h"
#include <stdexcept>

namespace TS {


Packet2::Packet2()
{

}

bool Packet2::isSyncByteFixedValue() const
{
    return syncByte.value == syncByteFixedValue;
}

bool Packet2::isNullPacket() const
{
    return pid.value == pidNullPacket;
}

QDebug operator<<(QDebug debug, const Packet2 &packet)
{
    QDebugStateSaver saver(debug);
    debug.nospace() << "TS::Packet2(";

    if (packet.isSyncByteFixedValue())
        debug << "syncByte";
    else
        debug << "syncByte=" << packet.syncByte.value;

    debug << " transportErrorIndicator="   << packet.transportErrorIndicator.value;
    debug << " payloadUnitStartIndicator=" << packet.payloadUnitStartIndicator.value;
    debug << " transportPriority="         << packet.transportPriority.value;
    debug << " PID="                       << packet.pid.value;

    if (packet.isNullPacket()) {
        debug << " NullPacket";
    }
    else {
        debug << " " << packet.transportScramblingControl.value;
        debug << " " << packet.adaptationFieldControl.value;
        debug << " continuityCounter=" << packet.continuityCounter.value;

        if (packet.adaptationFieldControl.value == TS::Packet2::AdaptationFieldControlType::AdaptationFieldOnly ||
            packet.adaptationFieldControl.value == TS::Packet2::AdaptationFieldControlType::AdaptationFieldThenPayload)
        {
            // TODO: Add dump of AdaptationField.
            debug << " TODO=adaptationField";
        }
    }

    debug << ")";
    return debug;
}


namespace impl {
class Packet2ParserImpl {
    int  _tsPacketSize = Packet2::sizeBasic;

    friend Packet2Parser;
};
}

Packet2Parser::Packet2Parser() :
    _implPtr(std::make_unique<impl::Packet2ParserImpl>())
{

}

Packet2Parser::~Packet2Parser()
{

}

int Packet2Parser::tsPacketSize() const
{
    return _implPtr->_tsPacketSize;
}

void Packet2Parser::setTSPacketSize(int size)
{
    // FIXME: Implement
    throw std::runtime_error("TS packet v2 parser: Setting TS packet size not implemented, yet");
}

bool Packet2Parser::parse(const QByteArray &bytes, Packet2Parser::Parse *output)
{
    if (!output)
        throw std::invalid_argument("TS packet v2 parser: Output can't be null");

    output->errorMessage.clear();
    output->bytes = bytes;
    BitStream bitSource(output->bytes);
    Packet2 &packet(output->packet);

    if (bytes.length() != _implPtr->_tsPacketSize) {
        QDebug(&output->errorMessage)
            << "Expected TS packet size" << _implPtr->_tsPacketSize
            << "but got" << bytes.length();
        return false;
    }

    try {
        bitSource >> packet.syncByte;
        if (packet.isSyncByteFixedValue())
            throw static_cast<std::runtime_error>(ExceptionBuilder()
                << "No sync byte" << HumanReadable::Hexdump { QByteArray(1, Packet2::syncByteFixedValue) }
                << "-- starts with" << HumanReadable::Hexdump { output->bytes.left(8) }.enableAll());
    }
    catch (std::exception &ex) {
        QDebug(&output->errorMessage)
            << "Error at sync byte:" << ex.what();
        return false;
    }

    try {
        bitSource
            >> packet.transportErrorIndicator
            >> packet.payloadUnitStartIndicator
            >> packet.transportPriority
            >> packet.pid;

        if (packet.isNullPacket())
            // Stop parsing here. Rest can be arbitrarily invalid.
            return true;
    }
    catch (std::exception &ex) {
        QDebug(&output->errorMessage)
            << "Error between transportErrorIndicator and PID:" << ex.what();
        return false;
    }

    try {
        bitSource
            >> packet.transportScramblingControl
            >> packet.adaptationFieldControl
            >> packet.continuityCounter;
    }
    catch (std::exception &ex) {
        QDebug(&output->errorMessage)
            << "Error between transportScramblingControl and continuityCounter:" << ex.what();
        return false;
    }

    if (packet.adaptationFieldControl.value == Packet2::AdaptationFieldControlType::AdaptationFieldOnly ||
        packet.adaptationFieldControl.value == Packet2::AdaptationFieldControlType::AdaptationFieldThenPayload)
    {
        // TODO: Parse AdaptationField, too.
        output->errorMessage = "Parsing AdaptationField not implemented, yet";
        return false;
    }

    if (packet.adaptationFieldControl.value == Packet2::AdaptationFieldControlType::PayloadOnly ||
        packet.adaptationFieldControl.value == Packet2::AdaptationFieldControlType::AdaptationFieldThenPayload)
    {
        // TODO: Somehow return payload data.
        output->errorMessage = "Payload data not supported, yet";
        return false;
    }

    return true;
}


}  // namespace TS
