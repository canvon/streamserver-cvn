#ifndef LOG_BACKEND_H
#define LOG_BACKEND_H

#include "log.h"

#include <QtMessageHandler>
#include <QTextStream>

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


}  // namespace log::backend
}  // namespace log

#endif // LOG_BACKEND_H
