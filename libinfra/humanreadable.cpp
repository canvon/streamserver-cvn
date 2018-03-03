#include "humanreadable.h"

#include <cmath>
#include <utility>
#include <stdexcept>
#include <QList>

namespace {

bool hasOtherThan(QChar hay, const QByteArray &haystack)
{
    bool found = false;
    for (QChar c : haystack) {
        if (c != hay) {
            found = true;
            break;
        }
    }
    return found;
}

}  // namespace

QString HumanReadable::byteCount(quint64 count, bool base1000, bool base1024)
{
    bool both = (base1000 && base1024);
    QList<QChar> unitChars { 'K', 'M', 'G', 'T' };
    QString ret;

    auto forBase = [&](quint64 base, QString infix = "") {
        QString unitName = "B";
        double unitValue = 1;
        double unitCount = count;
        for (QChar nextUnitChar : unitChars) {
            if (unitCount < base)
                break;

            unitName = nextUnitChar + infix + "B";
            unitValue *= base;
            unitCount /= base;
        }
        ret.append(QString::number(unitCount, 'f', 2) + " " + unitName);
    };

    if (base1000)
        forBase(1000);
    if (both)
        ret.append(" / ");
    if (base1024)
        forBase(1024, "i");

    return ret;
}

QString HumanReadable::timeDuration(qint64 msec, bool exact)
{
    QList<std::pair<QString, qint64>> units {
        std::make_pair("s",   1000),
        std::make_pair("min",   60),
        std::make_pair("h",     60),
        std::make_pair("d",     24),
    };
    qint64 unit = msec, rest = 0;
    QString prevUnitName = "ms";
    QString ret;

    if (!exact)
        throw std::runtime_error("Human readable time duration: non-exact mode not implemented, yet");

    for (auto pair : units) {
        QString unitName = pair.first;
        qint64 unitSize = pair.second;

        qint64 newUnit = unit / unitSize;
        rest           = unit % unitSize;
        unit           = newUnit;

        if (!ret.isEmpty())
            ret.prepend(" ");
        ret.prepend(QString::number(rest) + prevUnitName);

        prevUnitName = unitName;

        if (unit == 0)
            break;
    }

    if (unit != 0) {
        if (!ret.isEmpty())
            ret.prepend(" ");
        ret.prepend(QString::number(unit) + prevUnitName);
    }

    if (ret.isEmpty())
        ret = "0ms";

    return ret;
}

HumanReadable::Hexdump &HumanReadable::Hexdump::enableByteCount()
{
    byteCount = true;
    return *this;
}

HumanReadable::Hexdump &HumanReadable::Hexdump::enableCompressTrailing()
{
    compressTrailing = true;
    return *this;
}

HumanReadable::Hexdump &HumanReadable::Hexdump::enableAll()
{
    hex = true;
    ascii = true;
    byteCount = true;
    compressAllOneBits = true;
    compressAllZeroBits = true;
    compressTrailing = true;

    return *this;
}

QDebug operator<<(QDebug debug, const HumanReadable::Hexdump &dump)
{
    QDebugStateSaver saver(debug);
    debug.nospace();

    if (dump.compressAllOneBits && !hasOtherThan('\xff', dump.data))
        debug << dump.data.length() << "x\"ff\"";
    else if (dump.compressAllZeroBits && !hasOtherThan('\x00', dump.data))
        debug << dump.data.length() << "x\"00\"";
    else {
        if (dump.byteCount)
            debug << "(" << dump.data.length() << ")";

        QByteArray mainData = dump.data;
        int mainDataLen = mainData.length();
        int trailingCount = 0;
        char trailingByte;
        if (dump.compressTrailing && mainDataLen >= 3) {
            trailingByte = mainData.at(mainDataLen - 1);
            if (mainData.at(mainDataLen - 2) == trailingByte &&
                mainData.at(mainDataLen - 3) == trailingByte)
            {
                while (mainData.endsWith(trailingByte)) {
                    mainData.chop(1);
                    trailingCount++;
                }
            }
        }

        if (dump.hex) {
            if (trailingCount)
                debug << "(";
            debug << mainData.toHex();
            if (trailingCount)
                debug << "+" << trailingCount << "x" << QByteArray(1, trailingByte).toHex() << ")";
        }
        if (dump.hex && dump.ascii)
            debug << "/";
        if (dump.ascii) {
            if (trailingCount)
                debug << "(";
            debug << mainData;
            if (trailingCount)
                debug << "+" << trailingCount << "x" << QByteArray(1, trailingByte) << ")";
        }
    }

    return debug;
}

bool HumanReadable::FlagConverter::flagToBool(const QVariant &flag, bool *ok) const
{
    if (ok)
        *ok = false;

    if (flag.type() == QVariant::Bool) {
        if (ok)
            *ok = true;
        return flag.toBool();
    }
    else if (flag.canConvert(QVariant::String)) {
        QString flagStr = flag.toString();
        if (falseFlags.contains(flagStr, Qt::CaseInsensitive)) {
            if (ok)
                *ok = true;
            return false;
        }
        else if (trueFlags.contains(flagStr, Qt::CaseInsensitive)) {
            if (ok)
                *ok = true;
            return true;
        }
        else {
            // (ok stays at false.)
            return false;
        }
    }

    // Fallback. (ok stays at false.)
    return false;
}

QStringList HumanReadable::FlagConverter::flagPairs() const
{
    int falseLen = falseFlags.length();
    int trueLen  = trueFlags.length();

    QStringList ret;
    for (int i = 0; i < std::max(falseLen, trueLen); i++) {
        QStringList thePair {
            i < falseLen ? falseFlags.at(i) : "(unknown)",
            i < trueLen  ? trueFlags.at(i)  : "(unknown)",
        };
        ret.append(thePair.join('/'));
    }

    return ret;
}
