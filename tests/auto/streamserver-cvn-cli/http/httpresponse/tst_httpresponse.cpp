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
    QCOMPARE(resp.body(), QByteArray());

    HTTP::Response otherResp(HTTP::SC_400_BadRequest, "Bad Request-est", "HTTP/1.1");
    QCOMPARE(otherResp.httpVersion(), QString("HTTP/1.1"));
    QCOMPARE(static_cast<int>(otherResp.statusCode()), 400);
    QCOMPARE(otherResp.statusMsg(), QString("Bad Request-est"));
    QCOMPARE(otherResp.body(), QByteArray());
}

void TestHTTPResponse::getSetCheck()
{
    HTTP::Response resp;
    resp.setHttpVersion("HTTP/1.1");
    QCOMPARE(resp.httpVersion(), QString("HTTP/1.1"));
    resp.setStatusCode(HTTP::SC_404_NotFound);
    QCOMPARE(static_cast<int>(resp.statusCode()), 404);
    resp.setStatusMsg("Not Found Here");
    QCOMPARE(resp.statusMsg(), QString("Not Found Here"));
    resp.setBody("ABCdef.");
    QCOMPARE(resp.body(), QByteArray("ABCdef."));
    // Headers will need a more specific test.
}

QTEST_APPLESS_MAIN(TestHTTPResponse)
#include "tst_httpresponse.moc"
