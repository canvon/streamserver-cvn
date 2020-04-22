#ifndef HTTPUTIL_H
#define HTTPUTIL_H

class QByteArray;
class QString;

namespace SSCvn {
namespace HTTP {  // namespace SSCvn::HTTP


extern const QByteArray
    lineSep,  // CR-LF
    fieldSepStartLine,  // in Request-Line, Status-Line
    fieldSepHeaderParse,
    fieldSepHeaderGenerate;

QByteArray simplifiedLinearWhiteSpace(const QByteArray &bytes);


enum StatusCode {
    SC_200_OK         = 200,
    SC_400_BadRequest = 400,
    SC_404_NotFound   = 404,
    SC_500_InternalServerError = 500,
};

QString statusMsgFromStatusCode(StatusCode statusCode);


}  // namespace SSCvn::HTTP
}  // namespace SSCvn

#endif // HTTPUTIL_H
