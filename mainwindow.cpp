#include "mainwindow.h"

#include <DTitlebar>
#include <DFrame>
#include <DBlurEffectWidget>
#include <DApplicationHelper>
#include <DSysInfo>
#include <DDialog>
#include <QTextEdit>
#include <QProcess>

#include <QHBoxLayout>
#include <QGSettings/QGSettings>

MainWindow::MainWindow(QWidget *parent)
    : DMainWindow(parent)
    , m_centralWidgetBlurBg(nullptr)
    , m_isDeepin(false)
    , m_appManagerModel(nullptr)
    , m_appManagerWidget(nullptr)
{
    setMinimumSize(500, 300);
    resize(1000, 600);
    // 设置背景
    setTitlebarShadowEnabled(false);
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

    QAction *openOhMyDDEAction = new QAction(this);
    openOhMyDDEAction->setText("打开oh-my-dde");
    menu->addAction(openOhMyDDEAction);

    QAction *openProInfoWindowAction = new QAction(this);
    openProInfoWindowAction->setText("打开进程信息窗口");
    menu->addAction(openProInfoWindowAction);

    m_centralWidgetBlurBg = new DBlurEffectWidget(this);
    m_centralWidgetBlurBg->setBlendMode(DBlurEffectWidget::BlendMode::BehindWindowBlend);
    m_centralWidgetBlurBg->setMaskAlpha(100);
    m_centralWidgetBlurBg->setFixedSize(size());
    m_centralWidgetBlurBg->lower();

    // 判断系统是否是deepin
    m_isDeepin =  DSysInfo::isDeepin();

    DFrame *centralWidget = new DFrame(this);
    int dtkWindowRadius = 8;
    if (QGSettings::isSchemaInstalled("com.deepin.xsettings")) {
        QGSettings deepinSettings("com.deepin.xsettings");
        dtkWindowRadius = deepinSettings.get("dtk-window-radius").toInt();
    }
    int internalPix =  dtkWindowRadius < 9 ? 0 : (dtkWindowRadius - 8) / 2;
    centralWidget->setContentsMargins(4 + internalPix, 0, 4 + internalPix, 4 + internalPix);
    setCentralWidget(centralWidget);

    QHBoxLayout *mainLayout = new QHBoxLayout;
    mainLayout->setContentsMargins(0, 0, 0, 0);
    centralWidget->setLayout(mainLayout);

    // 应用管理
    m_appManagerModel = new AppManagerModel(this);
    m_appManagerWidget = new AppManagerWidget(m_appManagerModel, this);
    mainLayout->addWidget(m_appManagerWidget);

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
            QProcess::startDetached("dex", {m_appManagerModel->getAppInfo(OH_MY_DDE_PKG_NAME).desktopInfo.desktopPath});
        } else {
            // 安装
            DDialog dlg;
            dlg.setMessage("是否安装oh-my-dde?");
            dlg.addButton("是", true, DDialog::ButtonType::ButtonRecommend);
            dlg.addButton("否", false, DDialog::ButtonType::ButtonNormal);
            int ret = dlg.exec();
            if (0 == ret) {
                Q_EMIT m_appManagerModel->notifyThreadInstallOhMyDDE();
            }
        }
    });

    connect(openProInfoWindowAction, &QAction::triggered, this, [this](bool checked) {
        Q_UNUSED(checked);
        if (m_appManagerModel->isPkgInstalled(PROC_INFO_PLUGIN_PKG_NAME)) {
            QProcess::startDetached("dex", {m_appManagerModel->getAppInfo(PROC_INFO_PLUGIN_PKG_NAME).desktopInfo.desktopPath});
        } else {
            // 安装
            DDialog dlg;
            dlg.setMessage("是否安装dde进程信息插件?");
            dlg.addButton("是", true, DDialog::ButtonType::ButtonRecommend);
            dlg.addButton("否", false, DDialog::ButtonType::ButtonNormal);
            int ret = dlg.exec();
            if (0 == ret) {
                Q_EMIT m_appManagerModel->notifyThreadInstallProcInfoPlugin();
            }
        }
    });
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
        setAttribute(Qt::WA_TranslucentBackground, true);
        m_centralWidgetBlurBg->setVisible(true);
        titlebar()->setBackgroundTransparent(true);
    } else {
        setAttribute(Qt::WA_TranslucentBackground, false);
        m_centralWidgetBlurBg->setVisible(false);
        titlebar()->setBackgroundTransparent(false);
    }

    updateUIByRunningStatus();
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

void MainWindow::updateUIByRunningStatus()
{
    bool enable = (RunningStatus::Normal == m_appManagerModel->getRunningStatus());
    titlebar()->menu()->setEnabled(enable);
    m_appManagerWidget->setEnabled(enable);
}
