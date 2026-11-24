#include <QDir>
#include <QFileInfo>
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
    QString argFilePath;
    if (app.arguments().count() > 1)
        argFilePath = QFileInfo(app.arguments().at(1)).absoluteFilePath();
    QString exeDir = QCoreApplication::applicationDirPath();
    QDir::setCurrent(exeDir);
    app.setWindowIcon(QIcon(":/MainWindow/images/viMarkdown.png"));
    QCoreApplication::setOrganizationName("VisualSoftLab");
    QCoreApplication::setApplicationName("viMarkdown");

#if 0
    test
#endif

	QSettings settings;
	int lang = settings.value(KEY_LANGUAGE, SystemDefault).toInt();
	if( lang == SystemDefault ) {
	    QLocale systemLocale = QLocale::system();
	    if (systemLocale.language() == QLocale::Japanese)		//	日本語環境の場合
	    	lang = Japanese;
	}

    QTranslator qtTranslator;
    if( lang == Japanese && qtTranslator.load("qtbase_ja", QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        app.installTranslator(&qtTranslator);	//	Qt 標準ボタン等を日本語化
    }

    QTranslator translator;
    if( lang == Japanese && translator.load(":/MainWindow/viMarkdown_ja.qm")) {
        app.installTranslator(&translator);
	    g.m_japanese = true;
    } else
	    g.m_japanese = false;
    
    MainWindow window;
    if (!argFilePath.isEmpty()) {
        window.do_open("", argFilePath);
    }
    window.show();
    qDebug() << "OK";
    return app.exec(); 
}
