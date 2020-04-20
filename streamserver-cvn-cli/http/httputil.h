#ifndef HTTPUTIL_H
#define HTTPUTIL_H

#include <QByteArray>

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
};


}  // namespace SSCvn::HTTP
}  // namespace SSCvn

#endif // HTTPUTIL_H
