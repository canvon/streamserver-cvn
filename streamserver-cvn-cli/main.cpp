#include <QCoreApplication>

#include "streamserver.h"

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>

#include <memory>
#include <QPointer>
#include <QDateTime>
#include <QTextStream>
#include <QCommandLineParser>

int verbose = 0;  // Normal output.

int debug_level = 0;  // No debugging.

bool isSystemdJournal_stdout = false;
bool isSystemdJournal_stderr = false;
const char systemdJournalEnvVarName[] = "JOURNAL_STREAM";

sig_atomic_t lastSigNum = 0;

QPointer<StreamServer> server;

namespace {
    bool logStarting = true;
    enum class LogTimestamping {
        None,
        Date,
        Time,
        TimeSubsecond,
    };
    LogTimestamping logTs = LogTimestamping::Time;
    QDateTime logLast;

    QTextStream out(stdout), errout(stderr);
}

static void msgHandler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg) {
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

QString signalNumberToString(int signum)
{
    switch (signum) {
    case SIGINT:
        return "SIGINT/^C";
    case SIGTERM:
        return "SIGTERM/kill";
    default:
        return "(unrecognized signal number " + QString::number(signum) + ")";
    }
}

static void handleSignalShutdown(int sigNum)
{
    const QString sigStr = signalNumberToString(sigNum);

    int theLastSigNum = lastSigNum;
    if (theLastSigNum) {
        const QString lastSigStr = signalNumberToString(theLastSigNum);
        qFatal("Handle signal %s: Already handling signal %s",
               qPrintable(sigStr), qPrintable(lastSigStr));
    }
    lastSigNum = sigNum;

    StreamServer *theServer = ::server;
    if (theServer) {
        if (!QMetaObject::invokeMethod(theServer, "shutdown", Qt::QueuedConnection,
                                       Q_ARG(int, sigNum), Q_ARG(QString, sigStr)))
            qFatal("Handle signal %s: Invoking shutdown on stream server failed",
                   qPrintable(sigStr));
    }
    else
        qFatal("Handle signal %s: No stream server!", qPrintable(sigStr));
}

void setupSignals()
{
    struct ::sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = &handleSignalShutdown;

    if (sigaction(SIGINT, &act, nullptr) != 0)
        throw std::system_error(errno, std::generic_category(), "Can't set signal handler for " + signalNumberToString(SIGINT).toStdString());
    if (sigaction(SIGTERM, &act, nullptr) != 0)
        throw std::system_error(errno, std::generic_category(), "Can't set signal handler for " + signalNumberToString(SIGTERM).toStdString());
}

namespace {
void handleTerminate() {
    auto exPtr = std::current_exception();
    if (!exPtr) {
        qDebug() << "Handle terminate called with no exception handling in progress";
        return;
    }

    try {
        std::rethrow_exception(exPtr);
    }
    catch (const std::exception &ex) {
        qCritical().nospace()
            << "Uncaught exception of type " << typeid(ex).name()
            << ": " << ex.what();
    }
    catch (...) {
        qCritical() << "Uncaught exception of unknown type";
    }

    if (server) {
        if (!server->isShuttingDown()) {
            if (verbose >= 0)
                qInfo() << "Uncaught exception handling: Trying to shut down server gracefully...";
            server->shutdown();
        }
        else
            qFatal("Uncaught exception handling: Server was already shutting down");
    }
    else
        qFatal("Uncaught exception handling: No stream server!");
}
}

int main(int argc, char *argv[])
{
    updateIsSystemdJournal();
    if (isSystemdJournal_stderr) {
        // With systemd journal, always use fancy output.
        logStarting = false;

        // ..but don't duplicate timestamps.
        logTs = LogTimestamping::None;
    }

    std::set_terminate(&handleTerminate);

    qInstallMessageHandler(&msgHandler);
    QCoreApplication a(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription("Media streaming server from MPEG-TS to HTTP clients");
    parser.addHelpOption();
    parser.addOptions({
        { { "v", "verbose" }, "Increase verbose level" },
        { { "q", "quiet"   }, "Decrease verbose level" },
        { { "d", "debug"   }, "Enable debugging. (Increase debug level.)" },
        { { "l", "listen" }, "Port to listen on for HTTP streaming client connections",
          "listen_port", "8000" },
        { { "logts", "log-timestamping" }, "How to timestamp log messages: "
          "none, date, time, timess/timesubsecond",
          "mode" },
        { { "s", "ts-packet-size" }, "MPEG-TS packet size (e.g., 188 bytes)",
          "size" },
        { "ts-strip-additional-info", "Strip additional info beyond 188 bytes basic packet size "
          "from TS packets",
          "flag" },
        { "brake", "Set brake type to use to slow down input that is coming in too fast: "
          "none, pcrsleep (default)",
          "type" },
        { "input-open-nonblock", "Open input in non-blocking mode",
          "flag" },
        { "input-reopen-timeout", "Timeout before reopening input after EOF",
          "timeMillisec" },
    });
    parser.addPositionalArgument("input", "Input file name");
    parser.process(a);

    // Set up log timestamping mode.
    const QString logTsStr = parser.value("logts");
    if (!logTsStr.isNull()) {
        if (logTsStr == "none")
            logTs = LogTimestamping::None;
        else if (logTsStr == "date")
            logTs = LogTimestamping::Date;
        else if (logTsStr == "time")
            logTs = LogTimestamping::Time;
        else if (logTsStr == "timess" || logTsStr == "timesubsecond")
            logTs = LogTimestamping::TimeSubsecond;
        else if (logTsStr == "help") {
            qInfo() << "Available log timestamping modes:"
                    << "none, date, time, timess/timesubsecond";
            return 0;
        }
        else {
            qCritical() << "Invalid log timestamping mode" << logTsStr;
            return 2;
        }
    }

    // Apply incremental options.
    for (QString opt : parser.optionNames()) {
        if (opt == "v" || opt == "verbose")
            verbose++;
        else if (opt == "q" || opt == "quiet")
            verbose--;
        else if (opt == "d" || opt == "debug")
#ifdef QT_NO_DEBUG_OUTPUT
            qFatal("No debug output compiled in, can't enable debugging!");
#else
            debug_level++;
#endif
    }


    QString listenPortStr = parser.value("listen");
    bool ok = false;
    qint16 listenPort = listenPortStr.toUShort(&ok);
    if (!ok) {
        qCritical() << "Invalid port number" << listenPortStr;
        return 2;
    }

    std::unique_ptr<qint64> tsPacketSizePtr;
    {
        QString valueStr = parser.value("ts-packet-size");
        if (!valueStr.isNull()) {
            bool ok = false;
            tsPacketSizePtr = std::make_unique<qint64>(valueStr.toLongLong(&ok));
            if (!ok) {
                tsPacketSizePtr.reset();
                qCritical() << "TS packet size: Conversion to number failed for" << valueStr;
                return 2;
            }
        }
    }

    std::unique_ptr<bool> tsStripAdditionalInfoPtr;
    {
        QString valueStr = parser.value("ts-strip-additional-info");
        if (!valueStr.isNull()) {
            if (valueStr == "0" || valueStr == "false" || valueStr == "no")
                tsStripAdditionalInfoPtr = std::make_unique<bool>(false);
            else if (valueStr == "1" || valueStr == "true" || valueStr == "yes")
                tsStripAdditionalInfoPtr = std::make_unique<bool>(true);
            else {
                qCritical() << "TS strip additional info: Invalid flag value" << valueStr;
                return 2;
            }
        }
    }

    std::unique_ptr<StreamServer::BrakeType> brakeTypePtr;
    {
        QString valueStr = parser.value("brake");
        if (!valueStr.isNull()) {
            if (valueStr == "none")
                brakeTypePtr = std::make_unique<StreamServer::BrakeType>(StreamServer::BrakeType::None);
            else if (valueStr == "pcrsleep")
                brakeTypePtr = std::make_unique<StreamServer::BrakeType>(StreamServer::BrakeType::PCRSleep);
            else if (valueStr == "help") {
                qInfo() << "Available brake types:"
                        << "none, pcrsleep (default)";
                return 0;
            }
            else {
                qCritical() << "Invalid brake type" << valueStr;
                return 2;
            }
        }
    }

    std::unique_ptr<bool> inputFileOpenNonblockingPtr;
    {
        QString valueStr = parser.value("input-open-nonblock");
        if (!valueStr.isNull()) {
            if (valueStr == "0" || valueStr == "false" || valueStr == "no")
                inputFileOpenNonblockingPtr = std::make_unique<bool>(false);
            else if (valueStr == "1" || valueStr == "true" || valueStr == "yes")
                inputFileOpenNonblockingPtr = std::make_unique<bool>(true);
            else {
                qCritical() << "Input open non-block: Invalid flag value" << valueStr;
                return 2;
            }
        }
    }

    std::unique_ptr<int> inputFileReopenTimeoutMillisecPtr;
    {
        QString valueStr = parser.value("input-reopen-timeout");
        if (!valueStr.isNull()) {
            bool ok = false;
            inputFileReopenTimeoutMillisecPtr = std::make_unique<int>(valueStr.toUInt(&ok));
            if (!ok) {
                inputFileReopenTimeoutMillisecPtr.reset();
                qCritical() << "Input reopen timeout: Conversion to number failed for" << valueStr;
                return 2;
            }
        }
    }


    QStringList args = parser.positionalArguments();
    if (args.length() != 1) {
        qCritical().nospace()
            << "Invalid number of arguments " << args.length()
            << ": Need exactly one positional argument, the input file";
        return 2;
    }

    QString inputFilePath = args.at(0);


    logStarting = false;


    StreamServer server(std::make_unique<QFile>(inputFilePath), listenPort);
    ::server = &server;

    try {
        if (tsPacketSizePtr) {
            server.setTSPacketSize(*tsPacketSizePtr);
            server.setTSPacketAutosize(false);
        }

        if (tsStripAdditionalInfoPtr)
            server.setTSStripAdditionalInfoDefault(*tsStripAdditionalInfoPtr);

        if (brakeTypePtr)
            server.setBrakeType(*brakeTypePtr);

        if (inputFileOpenNonblockingPtr)
            server.setInputFileOpenNonblocking(*inputFileOpenNonblockingPtr);

        if (inputFileReopenTimeoutMillisecPtr)
            server.setInputFileReopenTimeoutMillisec(*inputFileReopenTimeoutMillisecPtr);

        server.initInput();
    }
    catch (std::exception &ex) {
        qCritical() << "Error initializing stream server:" << ex.what();
        return 1;
    }

    try {
        setupSignals();
    }
    catch (std::exception &ex) {
        qCritical() << "Error setting up signals:" << ex.what();
        return 1;
    }

    return a.exec();
}
