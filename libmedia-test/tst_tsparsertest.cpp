#include <QString>
#include <QtTest>

#include "tsparser.h"
#include "tspacket.h"

#include <QTextStream>

class TSParserTest : public QObject
{
    Q_OBJECT

public:
    TSParserTest();

private Q_SLOTS:
    void bitStreamBitwiseRead();
    void bslbf1Assign();
    void bslbfAdaptationFieldControl();
};

TSParserTest::TSParserTest()
{
}

void TSParserTest::bitStreamBitwiseRead()
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

void TSParserTest::bslbf1Assign()
{
    TS::bslbf1 testBslbf1;
    testBslbf1.value = true;
}

void TSParserTest::bslbfAdaptationFieldControl()
{
    TS::bslbf<2, TSPacket::AdaptationFieldControlType> afc;
    TS::BitStream tsBits(QByteArray(1, 0x80));
    tsBits >> afc;
    QVERIFY(afc.value == TSPacket::AdaptationFieldControlType::AdaptationFieldOnly);

    TS::BitStream tsBits2(QByteArray(1, 0x40));
    tsBits2 >> afc;
    QVERIFY(afc.value == TSPacket::AdaptationFieldControlType::PayloadOnly);

    TS::BitStream tsBits3(QByteArray(1, 0xc0));
    tsBits3 >> afc;
    QVERIFY(afc.value == TSPacket::AdaptationFieldControlType::AdaptationFieldThenPayload);
}

QTEST_APPLESS_MAIN(TSParserTest)

#include "tst_tsparsertest.moc"
