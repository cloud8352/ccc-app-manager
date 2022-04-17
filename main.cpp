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
    a.setApplicationVersion(DApplication::buildVersion("0.0.2")); //change version here
    a.loadTranslator();
    a.setApplicationDisplayName(QObject::tr("App Manager"));
    a.setStyle("chameleon");
    a.setWindowIcon(QIcon::fromTheme("chromium-app-list"));
    a.setProductIcon(QIcon::fromTheme("chromium-app-list"));
    a.setApplicationDescription("ccc-app-manager是一款方便的第三方应用管理工具\n支持应用的安装，卸载，安装包提取等功能");
    a.setQuitOnLastWindowClosed(true);//关闭主窗口后关闭关于
    MainWindow w;
    w.show();

    Dtk::Widget::moveToCenter(&w);

    return a.exec();
}
