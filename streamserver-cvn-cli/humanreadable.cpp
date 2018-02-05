#include "humanreadable.h"

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
