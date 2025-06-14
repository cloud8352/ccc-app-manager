#include "mainwindow.h"

#include <DTitlebar>
#include <DFrame>
#include <DBlurEffectWidget>
#include <DApplicationHelper>
#include <DSysInfo>
#include <DDialog>

#include <QMenuBar>
#include <QLabel>
#include <QTextEdit>
#include <QProcess>
#include <QHBoxLayout>
#include <QGSettings/QGSettings>
#include <QScreen>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_mainMenu(nullptr)
    , m_centralWidgetBlurBg(nullptr)
    , m_isDeepin(false)
    , m_appManagerModel(nullptr)
    , m_appManagerWidget(nullptr)
{
    setMinimumSize(500, 300);
    QRect primaryScreenGeometry = qApp->primaryScreen()->geometry();
    int resizedWidth = int(primaryScreenGeometry.width() * 0.65);
    int resizedHeight = int(resizedWidth * 3 / 5);
    resize(resizedWidth, resizedHeight);
    // 设置背景
    // setTitlebarShadowEnabled(false);
    setFocusPolicy(Qt::FocusPolicy::ClickFocus);

    m_appManagerModel = new AppManagerModel(this);

    QMenuBar *menuBar = this->menuBar();
    m_mainMenu = new QMenu(this);
    m_mainMenu->setTitle("打开");
    menuBar->addMenu(m_mainMenu);

    QAction *watchGuiAppListAction = new QAction(this);
    watchGuiAppListAction->setText("复制启动器中所有应用包名");
    m_mainMenu->addAction(watchGuiAppListAction);

    QAction *openOhMyDDEAction = new QAction(this);
    openOhMyDDEAction->setText("打开oh-my-dde");
    m_mainMenu->addAction(openOhMyDDEAction);

    QAction *openProInfoWindowAction = new QAction(this);
    openProInfoWindowAction->setText("打开进程信息窗口");
    m_mainMenu->addAction(openProInfoWindowAction);

    m_centralWidgetBlurBg = new DBlurEffectWidget(this);
    m_centralWidgetBlurBg->setBlendMode(DBlurEffectWidget::BlendMode::BehindWindowBlend);
    m_centralWidgetBlurBg->setMaskAlpha(100);
    m_centralWidgetBlurBg->setFixedSize(size());
    m_centralWidgetBlurBg->lower();

    // 判断系统是否是deepin
    m_isDeepin = DSysInfo::isDeepin();
    if (m_appManagerModel->IsInGxdeOs()) {
        m_isDeepin = true;
    }

    DFrame *centralWidget = new DFrame(this);
    QPalette pa = centralWidget->palette();
    pa.setColor(QPalette::ColorRole::Base, Qt::transparent);
    centralWidget->setPalette(pa);
    setCentralWidget(centralWidget);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    centralWidget->setLayout(mainLayout);

    DTitlebar *titleBar = new DTitlebar(this);
    titleBar->setIcon(QIcon(":/icons/ccc-app-manager_56px.svg"));
    titleBar->setTitle("应用管理器");
    // titleBar->setFixedHeight(40);
    titleBar->setBackgroundTransparent(true);
    titleBar->setMenu(m_mainMenu);
    mainLayout->addWidget(titleBar, 0, Qt::AlignTop);

    QHBoxLayout *contentLayout = new QHBoxLayout;
    contentLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addLayout(contentLayout, 1);

    // 应用管理
    m_appManagerWidget = new AppManagerWidget(m_appManagerModel, this);
    contentLayout->addWidget(m_appManagerWidget);

    // init connection
    // 运行状态改变
    connect(m_appManagerModel, &AppManagerModel::runningStatusChanged, this, [this](RunningStatus) {
        this->updateUIByRunningStatus();
    });

    connect(watchGuiAppListAction, &QAction::triggered, this, [this](bool checked) {
        Q_UNUSED(checked);
        QString uiAppPkgNameList;
        for (const AM::AppInfo &info : m_appManagerModel->getAppInfosList()) {
            if (info.desktopInfo.desktopPath.isEmpty()) {
                continue;
            }
            uiAppPkgNameList.append(QString("%1 ").arg(info.pkgName));
        }

        QTextEdit *edit = new QTextEdit(this);
        edit->setText(uiAppPkgNameList);
        edit->setReadOnly(true);
        QPalette pa = edit->palette();
        pa.setColor(QPalette::ColorRole::Base, Qt::transparent);
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
    connect(openOhMyDDEAction, &QAction::triggered, this, [this](bool checked) {
        Q_UNUSED(checked);
        if (m_appManagerModel->isPkgInstalled(OH_MY_DDE_PKG_NAME)) {
            m_appManagerModel->startDetachedDesktopExec(m_appManagerModel->getAppInfo(OH_MY_DDE_PKG_NAME).desktopInfo.exec);
        } else {
            // 安装
            DDialog dlg;
            dlg.setMessage("是否安装oh-my-dde?");
            dlg.addButton("否", false, DDialog::ButtonType::ButtonNormal);
            dlg.addButton("是", true, DDialog::ButtonType::ButtonRecommend);
            int ret = dlg.exec();
            if (1 == ret) {
                Q_EMIT m_appManagerModel->notifyThreadInstallOhMyDDE();
            }
        }
    });

    connect(openProInfoWindowAction, &QAction::triggered, this, [this](bool checked) {
        Q_UNUSED(checked);
        if (m_appManagerModel->isPkgInstalled(PROC_INFO_PLUGIN_PKG_NAME)) {
            m_appManagerModel->startDetachedDesktopExec(m_appManagerModel->getAppInfo(PROC_INFO_PLUGIN_PKG_NAME).desktopInfo.exec);
        } else {
            // 安装
            DDialog dlg;
            dlg.setMessage("是否安装dde进程信息插件?");
            dlg.addButton("否", false, DDialog::ButtonType::ButtonNormal);
            dlg.addButton("是", true, DDialog::ButtonType::ButtonRecommend);
            int ret = dlg.exec();
            if (1 == ret) {
                m_appManagerModel->openSpkStoreAppDetailPage(PROC_INFO_PLUGIN_PKG_NAME);
            }
        }
    });


    // 安装完成时
    connect(m_appManagerModel, &AppManagerModel::installOhMyDDEFinished, this, &MainWindow::onPkgInstallFinished);
    connect(m_appManagerModel, &AppManagerModel::notifyOpenSparkStoreNeedBeInstallDlg,
            this, &MainWindow::openSparkStoreNeedBeInstallDlg);

    // 下载失败
    connect(m_appManagerModel, &AppManagerModel::pkgFileDownloadFailed, this, [this](const PkgInfo &info) {
        qInfo() << Q_FUNC_INFO << info.downloadUrl << "download failed!";
        DDialog *dlg = new DDialog(this);
        QString tip = QString("下载失败，请尝试在终端使用apt download %1命令下载").arg(info.pkgName);
        dlg->setMessage("下载失败，请尝试在终端使用以下命令下载");
        QTextEdit *cmdEdit = new QTextEdit(this);
        cmdEdit->setText(QString("apt download %1").arg(info.pkgName));
        cmdEdit->setReadOnly(true);
        dlg->addContent(cmdEdit);

        dlg->exec();
        dlg->deleteLater();
    });

    // post init
    if (m_isDeepin) {
        menuBar->setVisible(false);
        setAttribute(Qt::WA_TranslucentBackground, true);
        m_centralWidgetBlurBg->setVisible(true);
        titleBar->setBackgroundTransparent(true);
        setWindowFlag(Qt::WindowType::FramelessWindowHint, true);
    } else {
        setAttribute(Qt::WA_TranslucentBackground, false);
        m_centralWidgetBlurBg->setVisible(false);
        titleBar->setBackgroundTransparent(false);
        titleBar->setVisible(false);

        mainLayout->removeWidget(titleBar);
        titleBar->deleteLater();
    }

    updateUIByRunningStatus();
}

MainWindow::~MainWindow()
{
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    m_centralWidgetBlurBg->setFixedSize(size());
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    QMainWindow::closeEvent(event);
    qApp->exit();
}

void MainWindow::onPkgInstallFinished(bool successed, const QString &err)
{
    qInfo() << Q_FUNC_INFO << successed << err;
    if (successed) {
        return;
    }

    DDialog *dlg = new DDialog(this);
    QTextEdit *errEdit = new QTextEdit(this);
    errEdit->setText(err);
    errEdit->setReadOnly(true);
    dlg->addContent(errEdit);

    dlg->exec();
    dlg->deleteLater();
}

void MainWindow::openSparkStoreNeedBeInstallDlg()
{
    DDialog *dlg = new DDialog(this);
    dlg->setIcon(QIcon::fromTheme("dialog-warning"));

    QLabel *tipLabel = new QLabel(dlg);
    tipLabel->setWordWrap(true);
    tipLabel->setOpenExternalLinks(true);
    tipLabel->setText("本功能需要使用星火应用商店，请前往下面的网址下载安装星火应用商店后，再重试此功能");
    dlg->addContent(tipLabel);

    //超链接标签
    QLabel *linkLabel = new QLabel(dlg);
    linkLabel->setWordWrap(true);
    linkLabel->setOpenExternalLinks(true);//设置点击连接自动打开（（跳转到浏览器）
    linkLabel->setTextInteractionFlags(Qt::TextInteractionFlag::TextBrowserInteraction);
    const QString websiteLink = "https://gitee.com/deepin-community-store/spark-store/releases/latest";
    QString text = QString("<a href=\"%1\">%2</a>").arg(websiteLink).arg(websiteLink);
    linkLabel->setText(text);
    dlg->addContent(linkLabel);

    dlg->exec();
    dlg->deleteLater();
}

void MainWindow::updateUIByRunningStatus()
{
    bool enable = (RunningStatus::Normal == m_appManagerModel->getRunningStatus());
    m_mainMenu->setEnabled(enable);
    m_appManagerWidget->setEnabled(enable);
}
