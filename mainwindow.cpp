#include "mainwindow.h"
#include "appmanagerwidget.h"

#include <DTitlebar>
#include <DFrame>
#include <DBlurEffectWidget>
#include <DApplicationHelper>

#include <QHBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : DMainWindow(parent)
//    , m_centralWidgetBlurBg(nullptr)
// 在非dde下无法正常blur导致标题栏难以看清
{
    setMinimumSize(1000, 600);
    // 设置背景
    setAttribute(Qt::WA_TranslucentBackground);
    setTitlebarShadowEnabled(false);
    setFocusPolicy(Qt::FocusPolicy::ClickFocus);

    titlebar()->setIcon(QIcon::fromTheme("chromium-app-list"));
    titlebar()->setTitle("应用管理器");
    titlebar()->setFixedHeight(40);
    titlebar()->setBackgroundTransparent(false);//透明就彻底啥也看不见了

//    m_centralWidgetBlurBg = new DBlurEffectWidget(this);
//    m_centralWidgetBlurBg->setBlendMode(DBlurEffectWidget::BlendMode::BehindWindowBlend);
//    m_centralWidgetBlurBg->setMaskAlpha(30);
//    m_centralWidgetBlurBg->setFixedSize(size());
//    m_centralWidgetBlurBg->lower();

// 透明+blur确实很好看，但是在非dde上是一场灾难。。。

    DFrame *centralWidget = new DFrame(this);
    centralWidget->setContentsMargins(3, 0, 3, 3);
    setCentralWidget(centralWidget);

    QHBoxLayout *mainLayout = new QHBoxLayout;
    mainLayout->setContentsMargins(0, 0, 0, 0);
    centralWidget->setLayout(mainLayout);

    AppManagerWidget *appManagerWidget = new AppManagerWidget(this);
    mainLayout->addWidget(appManagerWidget);
}

MainWindow::~MainWindow()
{
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    DMainWindow::resizeEvent(event);

//    m_centralWidgetBlurBg->setFixedSize(size());
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    DMainWindow::closeEvent(event);
    qApp->exit();
}
