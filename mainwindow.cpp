#include "mainwindow.h"
#include "appmanagerwidget.h"

#include <DTitlebar>
#include <DFrame>
#include <DBlurEffectWidget>
#include <DApplicationHelper>
#include <DSysInfo>

#include <QHBoxLayout>
#include <QGSettings/QGSettings>

MainWindow::MainWindow(QWidget *parent)
    : DMainWindow(parent)
    , m_centralWidgetBlurBg(nullptr)
    , m_isDeepin(false)
{
    setMinimumSize(1000, 600);
    // 设置背景
    setTitlebarShadowEnabled(false);
    setFocusPolicy(Qt::FocusPolicy::ClickFocus);

    titlebar()->setIcon(QIcon::fromTheme("chromium-app-list"));
    titlebar()->setTitle("应用管理器");
    titlebar()->setFixedHeight(40);
    titlebar()->setBackgroundTransparent(true);

    m_centralWidgetBlurBg = new DBlurEffectWidget(this);
    m_centralWidgetBlurBg->setBlendMode(DBlurEffectWidget::BlendMode::BehindWindowBlend);
    m_centralWidgetBlurBg->setMaskAlpha(100);
    m_centralWidgetBlurBg->setFixedSize(size());
    m_centralWidgetBlurBg->lower();

    // 判断系统是否是deepin
    m_isDeepin =  DSysInfo::isDeepin();

    DFrame *centralWidget = new DFrame(this);
    QGSettings deepinSettings("com.deepin.xsettings");
    int dtkWindowRadius = deepinSettings.get("dtk-window-radius").toInt();
    int internalPix =  dtkWindowRadius < 9 ? 0 : (dtkWindowRadius - 8) / 2;
    centralWidget->setContentsMargins(4 + internalPix, 0, 4 + internalPix, 4 + internalPix);
    setCentralWidget(centralWidget);

    QHBoxLayout *mainLayout = new QHBoxLayout;
    mainLayout->setContentsMargins(0, 0, 0, 0);
    centralWidget->setLayout(mainLayout);

    AppManagerWidget *appManagerWidget = new AppManagerWidget(this);
    mainLayout->addWidget(appManagerWidget);

    // post init
    if (m_isDeepin) {
        setAttribute(Qt::WA_TranslucentBackground, true);
        m_centralWidgetBlurBg->setVisible(true);
        titlebar()->setBackgroundTransparent(true);
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
