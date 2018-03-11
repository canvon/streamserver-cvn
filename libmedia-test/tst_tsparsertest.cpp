#include <QString>
#include <QtTest>

#include "tsparser.h"

#include <QTextStream>

class TSParserTest : public QObject
{
    Q_OBJECT

public:
    TSParserTest();

private Q_SLOTS:
    void testCase1();
};

TSParserTest::TSParserTest()
{
}

void TSParserTest::testCase1()
{
    QByteArray res;
    QTextStream resStream(&res);

    TS::BitStream tsBits(" 0");
    while (!tsBits.atEnd()) {
        TS::bslbf1 bit;
        tsBits >> bit;
        if (bit)
            resStream << "1";
        else
            resStream << "0";
    }
    resStream << flush;

    QCOMPARE(res, QByteArray("00100000") + QByteArray("00110000"));
}

QTEST_APPLESS_MAIN(TSParserTest)

#include "tst_tsparsertest.moc"
