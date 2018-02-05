#include <QCoreApplication>

#include "streamserver.h"

#include <QDateTime>
#include <QTextStream>
#include <QCommandLineParser>

int verbose = 0;  // Normal output.

int debug_level = 0;  // No debugging.

namespace {
    enum class LogTimestamping {
        None,
        Date,
        Time,
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

    // Save last logging timestamp for comparison on next logging.
    logLast = now;

    // Fatal messages shall be fatal to the program execution.
    if (is_fatal_msg) {
        if (debug_level > 0)
            abort();

        exit(3);
    }
}

int main(int argc, char *argv[])
{
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
        { { "logts", "log-timestamping" }, "How to timestamp log messages",
          "mode" },
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
        else
            qFatal("Invalid log timestamping mode \"%s\"", qPrintable(logTsStr));
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
        errout << a.applicationName() << ": Invalid port number \""
               << listenPortStr << "\"" << endl;
        return 2;
    }


    QStringList args = parser.positionalArguments();
    if (args.length() != 1) {
        errout << a.applicationName() << ": Invalid number of arguments"
               << ": Need exactly one positional argument, the input file" << endl;
        return 2;
    }

    QString inputFilePath = args.at(0);


    StreamServer server(std::make_unique<QFile>(inputFilePath), listenPort);

    try {
        server.initInput();
    }
    catch (std::exception &ex) {
        errout << a.applicationName() << ": Error initializing stream server input"
               << ": " << ex.what() << endl;
        return 1;
    }

    return a.exec();
}
