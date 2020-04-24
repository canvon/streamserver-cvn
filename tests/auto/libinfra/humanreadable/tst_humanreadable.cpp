#include <QtTest>

#include "humanreadable.h"

#include <QDebug>

using namespace HumanReadable;

class TestHumanReadable : public QObject
{
    Q_OBJECT

private slots:
    void hexdumpEmpty();
};

void TestHumanReadable::hexdumpEmpty()
{
    QString result;
    QDebug(&result).nospace() << Hexdump { QByteArray() };
    QCOMPARE(result, QString("(empty)"));
}

QTEST_APPLESS_MAIN(TestHumanReadable)
#include "tst_humanreadable.moc"
