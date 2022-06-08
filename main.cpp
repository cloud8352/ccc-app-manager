#include "mainwindow.h"

#include <DApplication>
#include <DWidgetUtil>
#include <DApplicationSettings>

#include <QObject>

DWIDGET_USE_NAMESPACE

int main(int argc, char *argv[])
{
    DApplication a(argc, argv);
    a.setAttribute(Qt::AA_UseHighDpiPixmaps);
    a.setOrganizationName("ccc");
    a.setApplicationName("ccc-app-manager");
    a.setApplicationVersion(DApplication::buildVersion("0.0.5")); //change version here
    a.loadTranslator();
    a.setApplicationDisplayName(QObject::tr("App Manager"));
    a.setStyle("chameleon");
    a.setWindowIcon(QIcon(":/icons/deepin/builtin/icons/grid_48px.svg"));
    a.setProductIcon(QIcon(":/icons/deepin/builtin/icons/grid_48px.svg"));
    a.setApplicationDescription("ccc-app-manager是一款方便的第三方应用管理工具\n支持应用的安装，卸载，安装包提取等功能\n感谢yzzi开发了好用的oh-my-dde系统配置工具，我把工具的启动入口加到了本项目中");
    a.setQuitOnLastWindowClosed(true);//关闭主窗口后关闭关于

    // 保存窗口主题设置
    DApplicationSettings settings;

    MainWindow w;
    w.show();

    Dtk::Widget::moveToCenter(&w);

    return a.exec();
}
