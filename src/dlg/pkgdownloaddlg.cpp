#include "pkgdownloaddlg.h"
#include "../appmanagermodel.h"

#include <DTitlebar>
#include <DBlurEffectWidget>
#include <DApplicationHelper>
#include <DProgressBar>

#include <QPushButton>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QDesktopServices>

#define LEFT_MARGIN 10
#define RIGHT_MARGIN 10

using namespace AM;

PkgDownloadDlg::PkgDownloadDlg(const AM::AppInfo &appInfo, AppManagerModel *model, QWidget *parent)
    : DFrame(parent)
    , m_model(model)
    , m_showingAppInfo(appInfo)
    , m_centralWidgetBlurBg(nullptr)
    , m_titlebar(nullptr)
    , m_versionSelectMenu(nullptr)
    , m_versionSelectBtn(nullptr)
    , m_downloadBtn(nullptr)
    , m_isDownloading(false)
    , m_openDirBtn(nullptr)
    , m_pkgSizeLable(nullptr)
    , m_progressBar(nullptr)
    , m_infoEdit(nullptr)
{
    setMinimumSize(600, 400);
    setWindowFlag(Qt::WindowType::Dialog);
    setWindowModality(Qt::WindowModality::ApplicationModal);
    setFocusPolicy(Qt::FocusPolicy::ClickFocus);

    // 设置背景
    setAttribute(Qt::WA_TranslucentBackground);

    m_centralWidgetBlurBg = new DBlurEffectWidget(this);
    m_centralWidgetBlurBg->setBlendMode(DBlurEffectWidget::BlendMode::BehindWindowBlend);
    m_centralWidgetBlurBg->setFixedSize(size());
    m_centralWidgetBlurBg->lower();

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->setContentsMargins(0, 0, 0, 0);
    setLayout(mainLayout);

    m_titlebar = new DTitlebar(this);
    m_titlebar->setIcon(QIcon::fromTheme("ccc-app-manager"));
    m_titlebar->setTitle("应用管理器");
    m_titlebar->setFixedHeight(40);
    m_titlebar->setBackgroundTransparent(true);
    mainLayout->addWidget(m_titlebar, 0, Qt::AlignmentFlag::AlignTop);

    QHBoxLayout *pkgDownloadLayout = new QHBoxLayout;
    pkgDownloadLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addLayout(pkgDownloadLayout);

    pkgDownloadLayout->addSpacing(LEFT_MARGIN);
    QLabel *tipLabel = new QLabel("选择版本：", this);
    pkgDownloadLayout->addWidget(tipLabel);

    m_versionSelectMenu = new QMenu(this);
    // 获取版本列表
    for (const PkgInfo &pkgInfo : m_showingAppInfo.pkgInfoList) {
        QString versionStr = pkgInfo.version;
        versionStr.append(QString("（%1）").arg(pkgInfo.arch));
        if (m_showingAppInfo.installedPkgInfo.version == pkgInfo.version
            && m_showingAppInfo.installedPkgInfo.arch == pkgInfo.arch) {
            versionStr.append("（已安装）");
        }
        QAction *action = new QAction(versionStr, this);
        action->setCheckable(true);
        m_versionSelectMenu->addAction(action);
        m_versionActionList.append(action);
    }
    m_versionSelectBtn = new QPushButton(this);
    m_versionSelectBtn->setMinimumWidth(200);
    m_versionSelectBtn->setFlat(false);
    m_versionSelectBtn->setMenu(m_versionSelectMenu);
    pkgDownloadLayout->addWidget(m_versionSelectBtn);

    m_downloadBtn = new QPushButton(this);
    m_downloadBtn->setText("下载");
    pkgDownloadLayout->addWidget(m_downloadBtn);
    pkgDownloadLayout->addStretch(1);
    pkgDownloadLayout->addSpacing(RIGHT_MARGIN);

    // 保存位置
    QHBoxLayout *savedlocationLayout = new QHBoxLayout;
    savedlocationLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addLayout(savedlocationLayout);

    savedlocationLayout->addSpacing(LEFT_MARGIN);
    QLabel *savedlocationTitleLable = new QLabel("保存位置：");
    savedlocationLayout->addWidget(savedlocationTitleLable);

    QLineEdit *savedlocationLineEdit = new QLineEdit(this);
    savedlocationLineEdit->setReadOnly(true);
    savedlocationLineEdit->setText(m_model->getDownloadDirPath());
    savedlocationLayout->addWidget(savedlocationLineEdit);

    m_openDirBtn = new QPushButton(this);
    m_openDirBtn->setText("打开目录");
    savedlocationLayout->addWidget(m_openDirBtn);
    savedlocationLayout->addSpacing(RIGHT_MARGIN);

    // 包大小
    QHBoxLayout *pkgSizeLayout = new QHBoxLayout;
    pkgSizeLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addLayout(pkgSizeLayout);

    pkgSizeLayout->addSpacing(LEFT_MARGIN);
    QLabel *pkgSizeTitleLable = new QLabel("已下载/总大小：");
    pkgSizeLayout->addWidget(pkgSizeTitleLable);

    m_pkgSizeLable = new QLabel("500KB/900KB");
    pkgSizeLayout->addWidget(m_pkgSizeLable);
    pkgSizeLayout->addStretch(1);
    pkgSizeLayout->addSpacing(RIGHT_MARGIN);

    // 进度条
    QHBoxLayout *progressLayout = new QHBoxLayout;
    progressLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addLayout(progressLayout);

    progressLayout->addSpacing(LEFT_MARGIN);
    m_progressBar = new DProgressBar(this);
    m_progressBar->setMaximum(100);
    m_progressBar->setValue(50);
    m_progressBar->setTextVisible(true);
    progressLayout->addWidget(m_progressBar);
    progressLayout->addSpacing(RIGHT_MARGIN);

    // 包信息
    QHBoxLayout *pkgInfoLayout = new QHBoxLayout;
    pkgInfoLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addLayout(pkgInfoLayout);

    pkgInfoLayout->addSpacing(LEFT_MARGIN);
    m_infoEdit = new QTextEdit(this);
    m_infoEdit->setReadOnly(true);
    m_infoEdit->setText("info info");
    pkgInfoLayout->addWidget(m_infoEdit, 1);
    pkgInfoLayout->addSpacing(RIGHT_MARGIN);

    mainLayout->addSpacing(10);

    // init connection
    initConnection();

    if (!m_versionActionList.isEmpty()) {
        Q_EMIT m_versionSelectMenu->triggered(m_versionActionList.first());
    }
}

PkgDownloadDlg::~PkgDownloadDlg()
{
}

void PkgDownloadDlg::resizeEvent(QResizeEvent *event)
{
    DFrame::resizeEvent(event);

    m_centralWidgetBlurBg->setFixedSize(size());
}

void PkgDownloadDlg::closeEvent(QCloseEvent *event)
{
    DFrame::closeEvent(event);
    this->hide();
    this->deleteLater();
}

void PkgDownloadDlg::onVerSelectMenuTrigered(QAction *trigeredAction)
{
    if (m_showingAppInfo.pkgInfoList.size() != m_versionActionList.size()) {
        qDebug() << Q_FUNC_INFO << "version list init error!";
        return;
    }

    for (QAction *action : m_versionActionList) {
        action->setChecked(false);
    }

    for (int i = 0; i < m_versionActionList.size(); ++i) {
        if (m_versionActionList[i] == trigeredAction) {
            trigeredAction->setChecked(true);
            m_selectedPkgInfo = m_showingAppInfo.pkgInfoList[i];
            break;
        }
    }

    // 版本按钮
    m_versionSelectBtn->setText(trigeredAction->text());
    // 包大小
    m_pkgSizeLable->setText(QString("%1B/%2B").arg(0).arg(m_selectedPkgInfo.pkgSize));
    // 进度条
    m_progressBar->setMaximum(100);
    m_progressBar->setValue(0);
    // 信息
    const QString infoText = m_model->formatePkgInfo(m_selectedPkgInfo);
    m_infoEdit->setText(infoText);
}

void PkgDownloadDlg::onFileDownloadProgressChanged(const AM::PkgInfo &info, qint64 bytesRead, qint64 totalBytes)
{
    if (m_selectedPkgInfo.pkgName != info.pkgName) {
        return;
    }

    m_pkgSizeLable->setText(QString("%1B/%2B").arg(bytesRead).arg(totalBytes));

    double downloadPercent = double(bytesRead) / totalBytes;
    m_progressBar->setValue(int(100 * downloadPercent));
}

void PkgDownloadDlg::initConnection()
{
    connect(m_versionSelectMenu, &QMenu::triggered, this, &PkgDownloadDlg::onVerSelectMenuTrigered);
    connect(m_downloadBtn, &QPushButton::clicked, this, [this](bool) {
        this->m_model->notifyThreadDownloadPkgFile(m_selectedPkgInfo);
        this->m_isDownloading = true;
        this->updateUI();
    });

    // 打开目录
    connect(m_openDirBtn, &QPushButton::clicked, this, [this](bool) {
        QString fileName = QString(PKG_NAME_FORMAT_STR)
                .arg(m_selectedPkgInfo.pkgName)
                .arg(m_selectedPkgInfo.version)
                .arg(m_selectedPkgInfo.arch);

        QString path = QString("%1/%2")
                .arg(m_model->getDownloadDirPath())
                .arg(fileName);
        m_model->showFileItemInFileManager(path);
    });

    connect(m_model, &AppManagerModel::pkgFileDownloadProgressChanged, this, &PkgDownloadDlg::onFileDownloadProgressChanged);
    connect(m_model, &AppManagerModel::pkgFileDownloadFinished, this, [this](const AM::PkgInfo &info) {
        qInfo() << Q_FUNC_INFO << info.downloadUrl << "downloaded";
        this->m_isDownloading = false;
        this->updateUI();
    });
    connect(m_model, &AppManagerModel::pkgFileDownloadFailed, this, [this](const AM::PkgInfo &info) {
        qInfo() << Q_FUNC_INFO << info.downloadUrl << "download failed!";
        this->m_isDownloading = false;
        this->updateUI();
    });
}

void PkgDownloadDlg::updateUI()
{
    if (m_isDownloading) {
        m_titlebar->setEnabled(false);
        m_versionSelectBtn->setEnabled(false);
        m_downloadBtn->setEnabled(false);
    } else {
        m_titlebar->setEnabled(true);
        m_versionSelectBtn->setEnabled(true);
        m_downloadBtn->setEnabled(true);
    }
}
