#include <QString>
#include <QtTest>

#include "tsprimitive.h"
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
    void uimsbf13();
    void tcimsbfTest();

    void sinkTest();
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

void TSParserTest::uimsbf13()
{
    TS::uimsbf<13, quint16> myInt;
    TS::BitStream tsBits(QByteArray(1, 0x00) + QByteArray(1, 23));

    // Throw away the first 3 bits, so the 13-bit uint read is right-aligned
    // to a byte boundary.
    TS::bslbf<3, quint8> foo;
    tsBits >> foo;

    tsBits >> myInt;
    QCOMPARE(myInt.value, (quint16)23);
}

void TSParserTest::tcimsbfTest()
{
    TS::tcimsbf<7, qint8> mySignedInt;
    TS::BitStream tsBits9(QByteArray(1, 9));
    TS::BitStream tsBitsMinus7(QByteArray(1, -7));
    TS::BitStream tsBitsMinus1Indirectly(QByteArray(1, 127));

    // Throw away one bit.
    TS::bslbf1 foo;
    tsBits9                >> foo;
    tsBitsMinus7           >> foo;
    tsBitsMinus1Indirectly >> foo;

    tsBits9 >> mySignedInt;
    QCOMPARE(mySignedInt.value, (qint8)9);

    tsBitsMinus7 >> mySignedInt;
    QCOMPARE(mySignedInt.value, (qint8)-7);

    tsBitsMinus1Indirectly >> mySignedInt;
    QCOMPARE(mySignedInt.value, (qint8)-1);
}

void TSParserTest::sinkTest()
{
    TS::BitStream bs(QByteArray(1, 0xf4));

    bs.putBit(false);  // Clear MSB, 0xf -> 0x7
    TS::bslbf<3, quint8> dummy;
    bs >> dummy;  // Skip rest of nibble.

    // Set bits 3 and 1, clear 2 and 0 => 0x8 + 0x2 == 0xa
    bs.putBit(true);
    bs.putBit(false);
    bs.putBit(true);
    bs.putBit(false);

    QCOMPARE(bs.bytes().at(0), (char)0x7a);
}

QTEST_APPLESS_MAIN(TSParserTest)

#include "tst_tsparsertest.moc"
