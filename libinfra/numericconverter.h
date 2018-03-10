#ifndef NUMERICCONVERTER_H
#define NUMERICCONVERTER_H

namespace HumanReadable {

template <typename I> I numericConverter(const QString &s, bool *ok = nullptr);

template <> inline int    numericConverter<int>   (const QString &s, bool *ok) { return s.toInt(ok); }
template <> inline qint64 numericConverter<qint64>(const QString &s, bool *ok) { return s.toLongLong(ok); }

}  // namespace HumanReadable

#endif // NUMERICCONVERTER_H
