#include <QtTest>

#include "http/httpresponse.h"

using namespace SSCvn;

class TestHTTPResponse : public QObject
{
    Q_OBJECT

private slots:
    void constructing();
    void getSetCheck();
};

void TestHTTPResponse::constructing()
{
    HTTP::Response resp;
    QCOMPARE(resp.httpVersion(), QString("HTTP/1.0"));
    QCOMPARE(static_cast<int>(resp.statusCode()), 200);
    QCOMPARE(resp.statusMsg(), QString("OK"));
}

void TestHTTPResponse::getSetCheck()
{
    HTTP::Response resp;
    resp.setHttpVersion("HTTP/1.1");
    QCOMPARE(resp.httpVersion(), QString("HTTP/1.1"));
    // ...
}

QTEST_APPLESS_MAIN(TestHTTPResponse)
#include "tst_httpresponse.moc"
