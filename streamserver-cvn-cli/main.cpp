#include <QCoreApplication>

#include "streamserver.h"

#include <QTextStream>
#include <QCommandLineParser>

int verbose = 6;  // Everything but debug messages.

namespace {
    QTextStream out(stdout), errout(stderr);
}

static void msgHandler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg) {
    int sd_info = 5;  // SD_NOTICE

    switch (type) {
    case QtDebugMsg:
        sd_info = 7;  // SD_DEBUG
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
        break;
    default:
        errout << "<4>Warning: Log message handler got unrecognized message type " << type << endl;
        break;
    }

    errout << "<" << sd_info << ">";
    if (ctx.category && strcmp(ctx.category, "default") != 0) {
        errout << "[" << ctx.category << "] ";
    }
    if (verbose >= 7) {
        if (ctx.file) {
            errout << ctx.file;
            if (ctx.line) {
                errout << ":" << ctx.line;
            }
            if (ctx.function) {
                errout << ": " << ctx.function;
            }
            errout << ": ";
        }
    }
    errout << msg << endl;
}

int main(int argc, char *argv[])
{
    qInstallMessageHandler(&msgHandler);
    QCoreApplication a(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription("Media streaming server from MPEG-TS to HTTP clients");
    parser.addHelpOption();
    parser.addOptions({
        { { "l", "listen" }, "Port to listen on for HTTP streaming client connections",
          "listen_port", "8000" },
    });
    parser.addPositionalArgument("input", "Input file name");
    parser.process(a);


    QString listenPortStr = parser.value("listen");
    bool ok = false;
    qint16 listenPort = listenPortStr.toUShort(&ok);
    if (!ok) {
        errout << a.applicationName() << ": Invalid port number \""
               << listenPortStr << "\"" << endl;
        return 2;
    }

    out << "Listen port number: " << listenPort << endl;


    QStringList args = parser.positionalArguments();
    if (args.length() != 1) {
        errout << a.applicationName() << ": Invalid number of arguments"
               << ": Need exactly one positional argument, the input file" << endl;
        return 2;
    }

    QString inputFilePath = args.at(0);
    out << "Input file path: " << inputFilePath << endl;


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
