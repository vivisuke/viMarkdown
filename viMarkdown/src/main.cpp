#include <QFileInfo>
#include <QStringList>
#include "MainWindow.h"
#include <QtWidgets/QApplication>
#include <QSettings>
#include <QLocale>
#include <QTranslator>
#include <QLibraryInfo>

extern Global g;

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QStringList argFilePaths;
    const QStringList args = app.arguments();
    for (int i = 1; i < args.size(); ++i) {
        if (!args.at(i).isEmpty()) {
            argFilePaths.append(QFileInfo(args.at(i)).absoluteFilePath());
        }
    }
    const QString exeDir = QCoreApplication::applicationDirPath();
    app.setWindowIcon(QIcon(":/MainWindow/images/viMarkdown.png"));
    QCoreApplication::setOrganizationName("VisualSoftLab");
    QCoreApplication::setApplicationName("viMarkdown");

    QSettings settings;
    int lang = settings.value(KEY_LANGUAGE, SystemDefault).toInt();
    if (lang == SystemDefault) {
        QLocale systemLocale = QLocale::system();
        if (systemLocale.language() == QLocale::Japanese) { // 日本語環境の場合
            lang = Japanese;
        }
    }

    QTranslator qtTranslator;
    if (lang == Japanese) {
        if (qtTranslator.load("qtbase_ja", exeDir + "/translations")
         || qtTranslator.load("qtbase_ja", QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
            app.installTranslator(&qtTranslator); // Qt 標準ボタン等を日本語化
        }
    }

    QTranslator translator;
    if (lang == Japanese && translator.load(":/MainWindow/viMarkdown_ja.qm")) {
        app.installTranslator(&translator);
        g.m_japanese = true;
    } else {
        g.m_japanese = false;
    }

    MainWindow window;
    for (const QString &path : argFilePaths) {
        window.do_open("", path);
    }
    window.show();
    return app.exec();
}
