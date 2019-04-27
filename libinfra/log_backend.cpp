#include "log_backend.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <QtGlobal>
#include <QDateTime>
#include <QCoreApplication>


namespace SSCvn {
namespace log {

int verbose = 0;  // Normal output.

int debug_level = 0;  // No debugging.


namespace backend {

bool isSystemdJournal_stdout = false;
bool isSystemdJournal_stderr = false;
const char systemdJournalEnvVarName[] = "JOURNAL_STREAM";

bool logStarting = true;
LogTimestamping logTs = LogTimestamping::Time;
namespace {
QDateTime logLast;
}

QTextStream *logoutPtr = nullptr;


void msgHandler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg) {
    if (!logoutPtr)
        qFatal("Log message handler: Missing output setup!");
    QTextStream &errout(*logoutPtr);

    QDateTime now = QDateTime::currentDateTime();
    int sd_info = 5;  // SD_NOTICE
    bool is_fatal_msg = false;
    QString prefix;

    switch (type) {
    case QtDebugMsg:
        sd_info = 7;  // SD_DEBUG
        // Only at --debug.
        if (!(debug_level > 0))
            return;
        prefix = "DEBUG: ";
        break;
    case QtInfoMsg:
        sd_info = 6;  // SD_INFO
        break;
    case QtWarningMsg:
        sd_info = 4;  // SD_WARNING
        break;
    case QtCriticalMsg:
        sd_info = 3;  // SD_ERR
        break;
    case QtFatalMsg:
        sd_info = 2;  // SD_CRIT
        is_fatal_msg = true;
        prefix = "Fatal: ";
        break;
    default:
        errout << "<4>Warning: Log message handler got unrecognized message type " << type << endl;
        break;
    }

    if (logStarting) {
        // During startup, use application name as prefix (if available).
        if (qApp)
            errout << qApp->applicationName() << ": ";
    }
    else {
        // Output date once every day, if appropriate.
        if (logTs >= LogTimestamping::Time && logLast.date() < now.date())
            errout << "<6>" << now.date().toString() << endl;

        // systemd-compatible message severity.
        errout << "<" << sd_info << ">";

        // Optional timestamp.
        switch (logTs) {
        case LogTimestamping::None:
            break;
        case LogTimestamping::Date:
            errout << now.date().toString() << " ";
            break;
        case LogTimestamping::Time:
            errout << now.time().toString() << " ";
            break;
        case LogTimestamping::TimeSubsecond:
            errout << now.time().toString("HH:mm:ss.zzz") << " ";
            break;
        }
    }

    // Optional category.
    if (ctx.category && strcmp(ctx.category, "default") != 0) {
        errout << "[" << ctx.category << "] ";
    }

    // Optional prefix.
    errout << prefix;

    // Optional debugging aids.
    if (debug_level > 0) {
        if (debug_level > 1 && ctx.file) {
            errout << ctx.file;
            if (ctx.line) {
                errout << ":" << ctx.line;
            }
            errout << ": ";
        }
        if (ctx.function) {
            errout << ctx.function << ": ";
        }
    }

    // The message.
    errout << msg << endl;

    if (!logStarting) {
        // Save last logging timestamp for comparison on next logging.
        logLast = now;
    }

    // Fatal messages shall be fatal to the program execution.
    if (is_fatal_msg) {
        if (debug_level > 0)
            abort();

        exit(3);
    }
}

void updateIsSystemdJournal() {
    if (!qEnvironmentVariableIsSet(systemdJournalEnvVarName))
        return;

    QByteArray deviceInodeBytes = qgetenv(systemdJournalEnvVarName);
    auto list = deviceInodeBytes.split(':');
    if (list.length() != 2)
        return;

    bool ok = false;
    dev_t device = list.at(0).toULong(&ok);
    if (!ok)
        return;

    ok = false;
    ino_t inode = list.at(1).toULongLong(&ok);
    if (!ok)
        return;

    struct ::stat buf;
    int fd;
    if ((fd = fileno(stdout)) >= 0 &&
        fstat(fd, &buf) == 0 &&
        buf.st_dev == device &&
        buf.st_ino == inode)
    {
        isSystemdJournal_stdout = true;
    }

    if ((fd = fileno(stderr)) >= 0 &&
        fstat(fd, &buf) == 0 &&
        buf.st_dev == device &&
        buf.st_ino == inode)
    {
        isSystemdJournal_stderr = true;
    }
}


}  // namespace SSCvn::log::backend
}  // namespace SSCvn::log
}  // namespace SSCvn
