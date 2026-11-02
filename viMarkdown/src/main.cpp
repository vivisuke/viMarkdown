#include <QDir>
#include "MainWindow.h"
#include <QtWidgets/QApplication>
#include <QSettings>
#include <QLocale>
#include <QTranslator>

extern Global g;

int main(int argc, char *argv[])
{
	QString exeDir = QCoreApplication::applicationDirPath();
	QDir::setCurrent(exeDir);
    QApplication app(argc, argv);
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

    QTranslator translator;
    if( lang == Japanese && translator.load(":/MainWindow/viMarkdown_ja.qm")) {
        app.installTranslator(&translator);
	    g.m_japanese = true;
    } else
	    g.m_japanese = false;
    
    MainWindow window;
    if (app.arguments().count() > 1) {
        QString filePath = app.arguments().at(1);
        window.do_open("", filePath);
    }
    window.show();
    qDebug() << "OK";
    return app.exec(); 
}
