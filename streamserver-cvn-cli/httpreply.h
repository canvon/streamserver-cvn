#ifndef HTTPREPLY_H
#define HTTPREPLY_H

#include <utility>
#include <QString>
#include <QList>

// An HTTP request reply being constructed by the program.
class HTTPReply
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
    explicit HTTPReply(int statusCode = 200, const QString &statusMsg = "OK", const QString &httpVersion = "HTTP/1.0");

    const QByteArray lineSep = "\r\n";
    const QByteArray fieldSepStatusLine = " ";
    const QByteArray fieldSepHeader = ": ";

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

#endif // HTTPREPLY_H
