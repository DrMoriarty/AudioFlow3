#include "mainwindow.h"
#include "src/audioflow.h"
#include "src/fileutils/config.h"

#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include <QStandardPaths>
#include <QDir>
#include <QIcon>
#include <QMessageBox>
#include <QPushButton>
#include <QDesktopServices>
#include <QUrl>

extern "C" void setupDockReopen(QMainWindow *window);

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QApplication::setStyle("Fusion");
    QApplication::setApplicationName("AudioFlow3");
    QApplication::setApplicationVersion(APP_VERSION);
    QApplication::setWindowIcon(QIcon(":/icon1024.png"));
    QApplication::setQuitOnLastWindowClosed(false);

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "AudioFlow3_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            a.installTranslator(&translator);
            break;
        }
    }

    QString configPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(configPath);
    QString configFile = configPath + "/config.json";

    if (!initialize(configFile.toStdString())) {
        QMessageBox dialog(QMessageBox::Warning, "AudioFlow3",
            QCoreApplication::translate("main",
                "The application requires the BlackHole audio driver to be installed.\n\n"
                "Without it, the application cannot function."),
            QMessageBox::NoButton);
        QPushButton *downloadBtn = dialog.addButton(
            QCoreApplication::translate("main", "Download BlackHole"), QMessageBox::AcceptRole);
        dialog.exec();
        if (dialog.clickedButton() == downloadBtn)
            QDesktopServices::openUrl(QUrl("http://existential.audio/blackhole/"));
        return 1;
    }

    QObject::connect(&a, &QCoreApplication::aboutToQuit, []() { cleanup(); });

    MainWindow w(getConfig());
    setupDockReopen(&w);
    w.show();
    return QApplication::exec();
}
