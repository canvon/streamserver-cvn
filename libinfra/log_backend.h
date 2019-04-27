#ifndef LOG_BACKEND_H
#define LOG_BACKEND_H

#include "log.h"

#include <QtMessageHandler>
#include <QTextStream>

namespace SSCvn {
namespace log {
namespace backend {

extern bool isSystemdJournal_stdout;
extern bool isSystemdJournal_stderr;
extern const char systemdJournalEnvVarName[];

extern bool logStarting;

enum class LogTimestamping {
    None,
    Date,
    Time,
    TimeSubsecond,
};
extern LogTimestamping logTs;

extern QTextStream *logoutPtr;


void msgHandler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg);

void updateIsSystemdJournal();


}  // namespace SSCvn::log::backend
}  // namespace SSCvn::log
}  // namespace SSCvn

#endif // LOG_BACKEND_H
