#include <QCoreApplication>

#include "streamserver.h"

#include <QTextStream>
#include <QCommandLineParser>

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    QTextStream out(stdout), errout(stderr);

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
