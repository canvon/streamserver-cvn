#ifndef HTTPRESPONSE_H
#define HTTPRESPONSE_H

#include <utility>
#include <QString>
#include <QList>

namespace SSCvn {
namespace HTTP {  // namespace SSCvn::HTTP

// An HTTP request response(/reply) being constructed by the program.
class Response
{
    QString  _httpVersion;
    int      _statusCode;
    QString  _statusMsg;
public:
    typedef QList<std::pair<QString, QString>>  header_type;
private:
    header_type  _header;
    QByteArray   _body;

public:
    explicit Response(int statusCode = 200, const QString &statusMsg = "OK", const QString &httpVersion = "HTTP/1.0");

    const QString &httpVersion() const;
    void setHttpVersion(const QString &version);

    int statusCode() const;
    void setStatusCode(int status);

    const QString &statusMsg() const;
    void setStatusMsg(const QString &msg);

    const header_type &header() const;
    void setHeader(const QString &fieldName, const QString &fieldValue);

    const QByteArray &body() const;
    void setBody(const QByteArray &body);

    QByteArray toBytes() const;
};

}  // namespace SSCvn::HTTP
}  // namespace SSCvn

#endif // HTTPRESPONSE_H
