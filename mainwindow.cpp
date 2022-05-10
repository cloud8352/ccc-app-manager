#include "mainwindow.h"
#include "appmanagerwidget.h"

#include <DTitlebar>
//#include <DFrame>
#include <DtkWidgets>
#include <DBlurEffectWidget>
//#include <DApplicationHelper>
#include <DSysInfo>
//#include <DDialog>
#include <QTextEdit>

#include <QHBoxLayout>
#include <QGSettings/QGSettings>

MainWindow::MainWindow(QWidget *parent)
    : DMainWindow(parent)
    , m_centralWidgetBlurBg(nullptr)
    , m_isDeepin(false)
{
    setMinimumSize(1000, 600);
    // 设置背景
//    setTitlebarShadowEnabled(false);
    setFocusPolicy(Qt::FocusPolicy::ClickFocus);

    titlebar()->setIcon(QIcon(":/icons/deepin/builtin/icons/grid_48px.svg"));
    titlebar()->setTitle("应用管理器");
    titlebar()->setFixedHeight(40);
    titlebar()->setBackgroundTransparent(true);

    QMenu *menu = new QMenu(this);
    titlebar()->setMenu(menu);

    QAction *watchGuiAppListAction = new QAction(this);
    watchGuiAppListAction->setText("复制启动器中所有应用包名");
    menu->addAction(watchGuiAppListAction);

    m_centralWidgetBlurBg = new DBlurEffectWidget(this);
    m_centralWidgetBlurBg->setBlendMode(DBlurEffectWidget::BlendMode::BehindWindowBlend);
    m_centralWidgetBlurBg->setBlurEnabled(true);
    m_centralWidgetBlurBg->setMaskAlpha(100);
    m_centralWidgetBlurBg->setFixedSize(size());
    m_centralWidgetBlurBg->lower();

    // 判断系统是否是deepin
    m_isDeepin =  DSysInfo::isDeepin();

    QWidget *centralWidget = new QWidget(this);
    int dtkWindowRadius = 8;
    if (QGSettings::isSchemaInstalled("com.deepin.xsettings")) {
        QGSettings deepinSettings("com.deepin.xsettings");
//        dtkWindowRadius = deepinSettings.get("dtk-window-radius").toInt();
    }
    int internalPix =  dtkWindowRadius < 9 ? 0 : (dtkWindowRadius - 8) / 2;
    centralWidget->setContentsMargins(4 + internalPix, 0, 4 + internalPix, 4 + internalPix);
    setCentralWidget(centralWidget);

    QHBoxLayout *mainLayout = new QHBoxLayout;
    mainLayout->setContentsMargins(0, 0, 0, 0);
    centralWidget->setLayout(mainLayout);

    // 应用管理
    AppManagerModel *appManagerModel = new AppManagerModel(this);
    AppManagerWidget *appManagerWidget = new AppManagerWidget(appManagerModel, this);
    mainLayout->addWidget(appManagerWidget);

    // init connection
    connect(watchGuiAppListAction, &QAction::triggered, this, [this, appManagerModel](bool checked) {
        Q_UNUSED(checked);
        QString uiAppPkgNameList;
        for (const AM::AppInfo &info : appManagerModel->getAppInfosList()) {
            if (info.desktopInfo.desktopPath.isEmpty()) {
                continue;
            }
            uiAppPkgNameList.append(QString("%1 ").arg(info.pkgName));
        }

        QTextEdit *edit = new QTextEdit(this);
        edit->setText(uiAppPkgNameList);
        edit->setReadOnly(true);
        QPalette pa = edit->palette();
//        pa.setColor(QPalette::ColorRole::Base, Qt::transparent);
        edit->setPalette(pa);

        DDialog *dlg = new DDialog(this);
        dlg->setOnButtonClickedClose(true);
        dlg->setTitle("启动器中所有应用包名");
        dlg->setMinimumWidth(800);
        dlg->addContent(edit);
        dlg->setWindowOpacity(0.7);


        dlg->exec();
        dlg->deleteLater();
    });

    // post init
    if (m_isDeepin) {
        setAttribute(Qt::WA_TranslucentBackground, false);
        m_centralWidgetBlurBg->setVisible(false);
        titlebar()->setBackgroundTransparent(false);
    } else {
        setAttribute(Qt::WA_TranslucentBackground, false);
        m_centralWidgetBlurBg->setVisible(false);
        titlebar()->setBackgroundTransparent(false);
    }
}

MainWindow::~MainWindow()
{
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    DMainWindow::resizeEvent(event);
    m_centralWidgetBlurBg->setFixedSize(size());
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    DMainWindow::closeEvent(event);
    qApp->exit();
}
