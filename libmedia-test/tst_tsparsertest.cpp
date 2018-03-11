#include <QString>
#include <QtTest>

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
    QVERIFY2(true, "Failure");
}

QTEST_APPLESS_MAIN(TSParserTest)

#include "tst_tsparsertest.moc"
