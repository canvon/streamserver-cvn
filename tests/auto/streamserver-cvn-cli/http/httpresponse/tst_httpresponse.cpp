#include <QtTest>

#include "http/httpresponse.h"

using namespace SSCvn;

class TestHTTPResponse : public QObject
{
    Q_OBJECT

private slots:
    void constructing();
    void getSetCheck();
    void toBytes();
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

void TestHTTPResponse::toBytes()
{
    HTTP::Response resp(HTTP::SC_404_NotFound, "Not Found-ound");
    resp.setHeader("Content-Type", "text/plain");
    const char bodyChars[] = "Requested resource not found.\n";
    resp.setBody(bodyChars);
    QByteArray expectedBytes(
        "HTTP/1.0 404 Not Found-ound\r\n"
        "Content-Type: text/plain\r\n");
    expectedBytes.append("Content-Length: ");
    expectedBytes.append(QString::number(sizeof(bodyChars) - 1));  // (think: terminating NUL byte)
    expectedBytes.append("\r\n\r\n");
    expectedBytes.append(bodyChars);
    QCOMPARE(resp.toBytes(), expectedBytes);
}

QTEST_APPLESS_MAIN(TestHTTPResponse)
#include "tst_httpresponse.moc"
