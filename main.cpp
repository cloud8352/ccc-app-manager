#include "mainwindow.h"

#include <DApplication>
#include <DWidgetUtil>

#include <QObject>

DWIDGET_USE_NAMESPACE

int main(int argc, char *argv[])
{
    DApplication a(argc, argv);
    a.setAttribute(Qt::AA_UseHighDpiPixmaps);
    a.setOrganizationName("ccc");
    a.setApplicationName("ccc-app-manager");
    a.setApplicationVersion(DApplication::buildVersion("0.0.1"));
    a.loadTranslator();
    a.setApplicationDisplayName(QObject::tr("App Manager"));
    a.setStyle("chameleon");
    a.setWindowIcon(QIcon::fromTheme("chromium-app-list"));
    a.setProductIcon(QIcon::fromTheme("chromium-app-list"));
    a.setApplicationDescription("app manager");
    a.setQuitOnLastWindowClosed(false);
    MainWindow w;
    w.show();

    Dtk::Widget::moveToCenter(&w);

    return a.exec();
}
