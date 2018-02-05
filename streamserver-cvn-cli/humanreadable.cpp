#include "humanreadable.h"

#include <utility>
#include <stdexcept>
#include <QList>

#if 0
HumanReadable::HumanReadable()
{

}
#endif

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
