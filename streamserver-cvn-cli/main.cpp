#include <QCoreApplication>

#include "log_backend.h"
#include "streamserver.h"
#include "demangle.h"

#include <string.h>
#include <signal.h>

#include <memory>
#include <QPointer>
#include <QTextStream>
#include <QCommandLineParser>
#include <QSettings>

using log::debug_level;
using log::verbose;

sig_atomic_t lastSigNum = 0;

QPointer<StreamServer> server;

namespace {
    QTextStream out(stdout), errout(stderr);
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
    try {
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
                << "Uncaught exception of type " << DEMANGLE_TYPENAME(typeid(ex).name())
                << ": " << ex.what();
        }
        catch (...) {
            qCritical() << "Uncaught exception of unknown type";
        }

        if (debug_level > 0) {
            qCritical() << "Uncaught exception handling: Debugging turned on,"
                        << "aborting before graceful server shutdown"
                        << "in the hopes that the actual problem may be examined";
            abort();
        }

        if (server) {
            if (!server->isShuttingDown()) {
                if (verbose >= 0)
                    qInfo() << "Uncaught exception handling: Trying to shut down server gracefully...";
                server->shutdown();

                if (qApp) {
                    // Process events explicitly, as we won't be able to return to event loop. (?)
                    if (verbose >= 1)
                        qInfo() << "Uncaught exception handling: Processing events... (We can't return to event loop.)";
                    //qApp->flush();  // This was not enough.
                    qApp->processEvents();

                    if (verbose >= 1)
                        qInfo() << "Uncaught exception handling: Done processing events";
                    if (verbose >= -1)
                        qInfo() << "Uncaught exception handling: Exiting with code general error";
                    exit(1);
                }
                else
                    qFatal("Uncaught exception handling: No application object! Can't flush event loop");
            }
            else
                qFatal("Uncaught exception handling: Server was already shutting down");
        }
        else
            qFatal("Uncaught exception handling: No stream server!");
    }
    catch (const std::exception &ex) {
        fprintf(stderr, "<3>Exception of type %s in uncaught exception handler: %s\n",
                DEMANGLE_TYPENAME(typeid(ex).name()), ex.what());
        abort();
    }
    catch (...) {
        fprintf(stderr, "<3>Exception in uncaught exception handler!\n");
        abort();
    }
}
}

int main(int argc, char *argv[])
{
    log::backend::updateIsSystemdJournal();
    if (log::backend::isSystemdJournal_stderr) {
        // With systemd journal, always use fancy output.
        log::backend::logStarting = false;

        // ..but don't duplicate timestamps.
        log::backend::logTs = log::backend::LogTimestamping::None;
    }

    std::set_terminate(&handleTerminate);

    log::backend::logoutPtr = &errout;
    qInstallMessageHandler(&log::backend::msgHandler);
    QCoreApplication a(argc, argv);

    a.setOrganizationName("streamserver-cvn");
    a.setOrganizationDomain("streamserver-cvn.canvon.de");
    a.setApplicationName("streamserver-cvn-cli");
    const QString settingsGroupName = "streamserver-cvn-cli";
    QSettings settingsBase;
    settingsBase.beginGroup(settingsGroupName);
    std::unique_ptr<QSettings> settingsConfigfilePtr;

    QCommandLineParser parser;
    parser.setApplicationDescription("Media streaming server from MPEG-TS to HTTP clients");
    parser.addHelpOption();
    parser.addOptions({
        { { "c", "config-file" }, "Defaults that override the global config, but can be overridden by command-line arguments",
          "file_path" },
        { { "v", "verbose" }, "Increase verbose level" },
        { { "q", "quiet"   }, "Decrease verbose level" },
        { { "d", "debug"   }, "Enable debugging. (Increase debug level.)" },
        { "verbose-level", "Set verbose level",
          "level_number" },
        { "debug-level", "Set debug level",
          "level_number" },
        { { "l", "listen-port" }, "Port to listen on for HTTP streaming client connections",
          "port" },
        { "server-host-whitelist", "HTTP server host names to require (e.g., \"foo:8000,bar:8000\")",
          "whitelist" },
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

    // Determine per-run configuration.
    {
        QString valueStr = parser.value("config-file");
        if (!valueStr.isNull()) {
            if (!QFile::exists(valueStr)) {
                qCritical() << "Invalid per-run config-file: File does not exist:" << valueStr;
                return 2;
            }
            settingsConfigfilePtr = std::make_unique<QSettings>(valueStr, QSettings::IniFormat);
            settingsConfigfilePtr->beginGroup(settingsGroupName);
        }
    }

    // Prepare helper for accessing multiple configuration sources successively.
    auto effectiveValue = [&](const QString &key) {
        QVariant valueVar;

        // Base config.
        {
            QVariant valueBaseVar = settingsBase.value(key);
            if (valueBaseVar.isValid())
                valueVar = valueBaseVar;
        }

        // Per-run config file.
        if (settingsConfigfilePtr) {
            QVariant valueConfigfileVar = settingsConfigfilePtr->value(key);
            if (valueConfigfileVar.isValid())
                valueVar = valueConfigfileVar;
        }

        // Command-line argument.
        {
            QString valueCliStr = parser.value(key);
            if (!valueCliStr.isNull())
                valueVar = QVariant(valueCliStr);
        }

        return valueVar;
    };

    // Set up log timestamping mode.
    QVariant logTsVar = effectiveValue("log-timestamping");
    if (logTsVar.isValid()) {
        QString logTsStr = logTsVar.toString();
        if (logTsStr == "none")
            log::backend::logTs = log::backend::LogTimestamping::None;
        else if (logTsStr == "date")
            log::backend::logTs = log::backend::LogTimestamping::Date;
        else if (logTsStr == "time")
            log::backend::logTs = log::backend::LogTimestamping::Time;
        else if (logTsStr == "timess" || logTsStr == "timesubsecond")
            log::backend::logTs = log::backend::LogTimestamping::TimeSubsecond;
        else if (logTsStr == "help") {
            qInfo() << "Available log timestamping modes:"
                    << "none, date, time, timess/timesubsecond";
            return 0;
        }
        else {
            qCritical() << "Invalid log timestamping mode:" << logTsVar;
            return 2;
        }
    }

    // Prepare start values to be changed by incremental options.
    {
        QVariant valueVar = effectiveValue("verbose-level");
        if (valueVar.isValid()) {
            bool ok = false;
            verbose = valueVar.toInt(&ok);
            if (!ok) {
                qCritical() << "Invalid verbose-level: Can't convert to number:" << valueVar;
                return 2;
            }
        }
    }
    {
        QVariant valueVar = effectiveValue("debug-level");
        if (valueVar.isValid()) {
            bool ok = false;
            debug_level = valueVar.toInt(&ok);
            if (!ok) {
                qCritical() << "Invalid debug-level: Can't convert to number:" << valueVar;
                return 2;
            }
#ifdef QT_NO_DEBUG_OUTPUT
            if (debug_level > 0)
                qFatal("No debug output compiled in, can't initialize debug-level to %d!", debug_level);
#endif
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


    quint16 listenPort = StreamServer::listenPort_default;
    {
        QVariant valueVar = effectiveValue("listen-port");
        if (valueVar.isValid()) {
            bool ok = false;
            listenPort = valueVar.toUInt(&ok);
            if (!ok) {
                qCritical() << "Invalid listen port number: Can't convert to number:" << valueVar;
                return 2;
            }
        }
    }

    std::unique_ptr<QStringList> serverHostWhitelistPtr;
    {
        QVariant valueVar = effectiveValue("server-host-whitelist");
        if (valueVar.isValid()) {
            if (valueVar.type() == QVariant::String)
                serverHostWhitelistPtr = std::make_unique<QStringList>(valueVar.toString().split(','));
            else
                serverHostWhitelistPtr = std::make_unique<QStringList>(valueVar.toStringList());
        }
    }

    std::unique_ptr<qint64> tsPacketSizePtr;
    {
        QVariant valueVar = effectiveValue("ts-packet-size");
        if (valueVar.isValid()) {
            bool ok = false;
            tsPacketSizePtr = std::make_unique<qint64>(valueVar.toLongLong(&ok));
            if (!ok) {
                tsPacketSizePtr.reset();
                qCritical() << "Invalid TS packet size: Can't convert to number:" << valueVar;
                return 2;
            }
        }
    }

    std::unique_ptr<bool> tsStripAdditionalInfoPtr;
    {
        QVariant valueVar = effectiveValue("ts-strip-additional-info");
        if (valueVar.isValid()) {
            QString valueStr = valueVar.toString();
            if (valueStr == "0" || valueStr == "false" || valueStr == "no")
                tsStripAdditionalInfoPtr = std::make_unique<bool>(false);
            else if (valueStr == "1" || valueStr == "true" || valueStr == "yes")
                tsStripAdditionalInfoPtr = std::make_unique<bool>(true);
            else {
                qCritical() << "Invalid TS strip additional info flag: Can't convert to boolean:" << valueVar;
                return 2;
            }
        }
    }

    std::unique_ptr<StreamServer::BrakeType> brakeTypePtr;
    {
        QVariant valueVar = effectiveValue("brake");
        if (valueVar.isValid()) {
            QString valueStr = valueVar.toString();
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
                qCritical() << "Invalid brake type:" << valueVar;
                return 2;
            }
        }
    }

    std::unique_ptr<bool> inputFileOpenNonblockingPtr;
    {
        QVariant valueVar = effectiveValue("input-open-nonblock");
        if (valueVar.isValid()) {
            QString valueStr = valueVar.toString();
            if (valueStr == "0" || valueStr == "false" || valueStr == "no")
                inputFileOpenNonblockingPtr = std::make_unique<bool>(false);
            else if (valueStr == "1" || valueStr == "true" || valueStr == "yes")
                inputFileOpenNonblockingPtr = std::make_unique<bool>(true);
            else {
                qCritical() << "Invalid input open non-block flag: Can't convert to boolean:" << valueVar;
                return 2;
            }
        }
    }

    std::unique_ptr<int> inputFileReopenTimeoutMillisecPtr;
    {
        QVariant valueVar = effectiveValue("input-reopen-timeout");
        if (valueVar.isValid()) {
            bool ok = false;
            inputFileReopenTimeoutMillisecPtr = std::make_unique<int>(valueVar.toUInt(&ok));
            if (!ok) {
                inputFileReopenTimeoutMillisecPtr.reset();
                qCritical() << "Input reopen timeout: Can't convert to number:" << valueVar;
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


    log::backend::logStarting = false;


    StreamServer server(std::make_unique<QFile>(inputFilePath), listenPort);
    ::server = &server;

    try {
        if (serverHostWhitelistPtr)
            server.setServerHostWhitelist(*serverHostWhitelistPtr);

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
