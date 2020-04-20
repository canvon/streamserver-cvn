#include "httputil.h"

#include "log.h"

#include <QByteArray>
#include <QString>
#include <QDebug>

namespace SSCvn {
namespace HTTP {  // namespace SSCvn::HTTP


const QByteArray lineSep = "\r\n";  // CR-LF
const QByteArray fieldSepStartLine = " ";  // in Request-Line, Status-Line
const QByteArray fieldSepHeaderParse = ":";
const QByteArray fieldSepHeaderGenerate = ": ";


QByteArray simplifiedLinearWhiteSpace(const QByteArray &bytes)
{
    QByteArray ret;
    QByteArray lws;
    enum { Null, Ret, LWS } dir;
    for (const char &c : bytes) {
        dir = Null;
        switch (c) {
        case '\r':  // CR. (LWS optionally starts with CR-LF.)
            if (lws.isEmpty())
                dir = LWS;
            else
                dir = Ret;
            break;
        case '\n':  // LF. (LWS optionally starts with CR-LF.)
            if (lws == "\r")
                dir = LWS;
            else
                dir = Ret;
            break;
        case ' ':  // SP (space)
        case '\t':  // HT (horizontal-tab)
            dir = LWS;
            break;
        default:
            dir = Ret;
            break;
        }

        switch (dir) {
        case Null:
        case Ret:
            if (ret.isEmpty()) {
                // Any leading LWS just gets removed.
            }
            else {
                if (!lws.isEmpty())
                    // Transform a sequence of LWS to a single SP.
                    ret.append(' ');
            }
            lws.clear();

            ret.append(c);
            break;
        case LWS:
            lws.append(c);
            break;
        }
    }

    // Any trailing LWS just gets removed.
    // (By ignoring the final value of the lws variable.)

    return ret;
}

QString statusMsgFromStatusCode(StatusCode statusCode)
{
    switch (statusCode) {
    case SC_200_OK:
        return "OK";
    case SC_400_BadRequest:
        return "Bad Request";
    case SC_404_NotFound:
        return "Not Found";
    default:
        if (log::verbose >= 0) {
            qWarning() << "Unrecognized HTTP status code" << statusCode
                       << "-- status message missing!";
        }
        return "(status message missing)";
    }
}


}  // namespace SSCvn::HTTP
}  // namespace SSCvn
