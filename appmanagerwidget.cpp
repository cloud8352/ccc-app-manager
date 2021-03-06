#include "appmanagerwidget.h"
#include "pkgdownloaddlg.h"
#include "appmanagermodel.h"

#include <DTitlebar>
#include <DListView>
#include <DFrame>
#include <DBlurEffectWidget>
#include <DApplicationHelper>
#include <DSuggestButton>
#include <DButtonBox>
#include <DSpinner>
#include <DDialog>

#include <QPushButton>
#include <QLineEdit>
#include <QHBoxLayout>
#include <QStandardItem>
#include <QTextEdit>
#include <QFile>
#include <QSettings>
#include <QTextCodec>
#include <QFileInfo>
#include <QLabel>
#include <QProcess>
#include <QDir>
#include <QThread>
#include <QDesktopServices>
#include <QSplitter>

using namespace AM;

AppManagerWidget::AppManagerWidget(QWidget *parent)
    : QWidget(parent)
    , m_model(nullptr)
    , m_waitingSpinner(nullptr)
    , m_contentWidget(nullptr)
    , m_searchLineEdit(nullptr)
    , m_filterMenu(nullptr)
    , m_showAllAppAction(nullptr)
    , m_showInstalledAppAction(nullptr)
    , m_showGuiAppAction(nullptr)
    , m_showSearchedAppAction(nullptr)
    , m_appListModel(nullptr)
    , m_appListView(nullptr)
    , m_appCountLabel(nullptr)
    , m_appAbstractLabel(nullptr)
    , m_appNameLable(nullptr)
    , m_infoBtn(nullptr)
    , m_filesBtn(nullptr)
    , m_infoSwitchBtn(nullptr)
    , m_appInfoTextEdit(nullptr)
    , m_appFileListTextEdit(nullptr)
{
    setFocusPolicy(Qt::FocusPolicy::ClickFocus);

    m_model = new AppManagerModel(this);

    QHBoxLayout *mainLayout = new QHBoxLayout;
    mainLayout->setContentsMargins(0, 0, 0, 0);
    setLayout(mainLayout);

    // 等待界面
    m_waitingSpinner = new DSpinner(this);
    m_waitingSpinner->setFixedSize(30, 30);
    m_waitingSpinner->hide();
    mainLayout->addWidget(m_waitingSpinner);

    // 内容界面
    QHBoxLayout *contentLayout = new QHBoxLayout;
    contentLayout->setContentsMargins(0, 0, 0, 0);

    m_contentWidget = new QWidget(this);
    m_contentWidget->setContentsMargins(0, 0, 0, 0);
    m_contentWidget->setLayout(contentLayout);
    mainLayout->addWidget(m_contentWidget);

    QSplitter *contentSplitter = new QSplitter(this);
    contentLayout->addWidget(contentSplitter);

    QVBoxLayout *leftGuideLayout = new QVBoxLayout;
    QWidget *leftGuideFrame = new QWidget(this);
    leftGuideFrame->setLayout(leftGuideLayout);
    // 添加到分割器
    contentSplitter->addWidget(leftGuideFrame);

    leftGuideLayout->addSpacing(5);
    QHBoxLayout *guideOperatingLayout = new QHBoxLayout;
    leftGuideLayout->addLayout(guideOperatingLayout);

    guideOperatingLayout->addSpacing(5);
    QPushButton *reloadBtn = new QPushButton(this);
    reloadBtn->setFlat(true);
    reloadBtn->setToolTip("重载所有应用信息");
    reloadBtn->setIcon(QIcon::fromTheme("ukuu"));
    reloadBtn->setIconSize(QSize(30, 30));
    reloadBtn->setFixedSize(QSize(30, 30));
    guideOperatingLayout->addWidget(reloadBtn);

    m_searchLineEdit = new QLineEdit(this);
    m_searchLineEdit->setPlaceholderText("搜索");
    m_searchLineEdit->setClearButtonEnabled(true);
    guideOperatingLayout->addWidget(m_searchLineEdit);

    QPushButton *filterBtn = new QPushButton(this);
    filterBtn->setToolTip("展示类别");
    filterBtn->setFixedWidth(30);
    guideOperatingLayout->addWidget(filterBtn);

    m_filterMenu = new QMenu(this);
    m_showAllAppAction = new QAction("全部应用");
    m_showAllAppAction->setCheckable(true);
    m_filterMenu->addAction(m_showAllAppAction);
    m_showInstalledAppAction = new QAction("已安装应用");
    m_showInstalledAppAction->setCheckable(true);
    m_filterMenu->addAction(m_showInstalledAppAction);
    m_showGuiAppAction = new QAction("界面应用");
    m_showGuiAppAction->setCheckable(true);
    m_filterMenu->addAction(m_showGuiAppAction);
    m_showSearchedAppAction = new QAction("搜索结果");
    m_showSearchedAppAction->setCheckable(true);
    m_filterMenu->addAction(m_showSearchedAppAction);
    filterBtn->setMenu(m_filterMenu);

    // 应用列表
    m_appListModel = new QStandardItemModel(this);

    m_appListView = new DListView(this);
    m_appListView->setSpacing(0);
    m_appListView->setItemSpacing(1);
    m_appListView->setTextElideMode(Qt::TextElideMode::ElideMiddle);
    m_appListView->setEditTriggers(DListView::EditTrigger::NoEditTriggers);
    m_appListView->setAutoFillBackground(true);
    m_appListView->setItemSize(QSize(80, 48));
    m_appListView->setModel(m_appListModel);
    leftGuideLayout->addWidget(m_appListView, 1);

    // 应用个数标签
    m_appCountLabel = new QLabel("", this);
    m_appCountLabel->setText("共999个应用");
    m_appCountLabel->setAlignment(Qt::AlignLeft);
    leftGuideLayout->addWidget(m_appCountLabel);

    DFrame *infoFrame = new DFrame(this);
    infoFrame->setContentsMargins(0, 0, 0, 0);
    infoFrame->setBackgroundRole(DPalette::ItemBackground);
    // 添加到分割器
    contentSplitter->addWidget(infoFrame);

    QVBoxLayout *infoFrameLayout = new QVBoxLayout;
    infoFrameLayout->setContentsMargins(0, 0, 0, 0);
    infoFrameLayout->setSpacing(0);
    infoFrame->setLayout(infoFrameLayout);

    // 应用概要区域
    QHBoxLayout *appAbstractLayout = new QHBoxLayout;
    appAbstractLayout->setContentsMargins(0, 0, 0, 0);
    appAbstractLayout->setSpacing(0);
    infoFrameLayout->addLayout(appAbstractLayout);

    m_appAbstractLabel = new QLabel(this);
    m_appAbstractLabel->setContentsMargins(5, 5, 5, 5);
    m_appAbstractLabel->setPixmap(QIcon::fromTheme("").pixmap(40, 40));
    appAbstractLayout->addWidget(m_appAbstractLabel);

    m_appNameLable = new QLabel(this);
    appAbstractLayout->addWidget(m_appNameLable);
    appAbstractLayout->addStretch(1);

    DSuggestButton *openBtn = new DSuggestButton(this);
    openBtn->setText("打开");
    appAbstractLayout->addWidget(openBtn);
    appAbstractLayout->addSpacing(10);

    // switch btn area
    DFrame *switchAreaTopSep = new DFrame(this);
    switchAreaTopSep->setBackgroundRole(DPalette::ItemBackground);
    switchAreaTopSep->setFixedHeight(2);
    infoFrameLayout->addWidget(switchAreaTopSep);

    infoFrameLayout->addSpacing(5);
    QList<DButtonBoxButton *> switchBtnList;
    m_infoBtn = new DButtonBoxButton("信息", this);
    m_infoBtn->setMinimumWidth(100);
    switchBtnList.append(m_infoBtn);

    m_filesBtn = new DButtonBoxButton("文件", this);
    m_filesBtn->setMinimumWidth(100);
    switchBtnList.append(m_filesBtn);

    m_infoSwitchBtn = new DButtonBox(this);
    m_infoSwitchBtn->setButtonList(switchBtnList, true);
    infoFrameLayout->addWidget(m_infoSwitchBtn, 0, Qt::AlignLeft);

    // -------------- 信息展示区
    m_appInfoTextEdit = new QTextEdit(this);
    m_appInfoTextEdit->setLineWidth(0);
    m_appInfoTextEdit->setReadOnly(true);
    DPalette pa = DApplicationHelper::instance()->palette(m_appInfoTextEdit);
    pa.setColor(DPalette::ColorRole::Base, Qt::transparent);
    DApplicationHelper::instance()->setPalette(m_appInfoTextEdit, pa);
    infoFrameLayout->addWidget(m_appInfoTextEdit);

    m_appFileListTextEdit = new QTextEdit(this);
    m_appFileListTextEdit->setLineWidth(0);
    m_appFileListTextEdit->setReadOnly(true);
    pa = DApplicationHelper::instance()->palette(m_appFileListTextEdit);
    pa.setColor(DPalette::ColorRole::Base, Qt::transparent);
    DApplicationHelper::instance()->setPalette(m_appFileListTextEdit, pa);
    infoFrameLayout->addWidget(m_appFileListTextEdit);

    // 信息展示去底部第一行
    infoFrameLayout->addSpacing(5);
    QHBoxLayout *firstLineBottomLayout = new QHBoxLayout;
    firstLineBottomLayout->setContentsMargins(0, 0, 0, 0);
    firstLineBottomLayout->setSpacing(0);
    infoFrameLayout->addLayout(firstLineBottomLayout);

    firstLineBottomLayout->addSpacing(10);
    QPushButton *uninstallBtn = new QPushButton("卸载", this);
    uninstallBtn->setMaximumWidth(170);
    firstLineBottomLayout->addWidget(uninstallBtn);

    firstLineBottomLayout->addSpacing(10);
    QPushButton *gotoAppStoreBtn = new QPushButton("在应用商店中查看", this);
    gotoAppStoreBtn->setMaximumWidth(170);
    firstLineBottomLayout->addWidget(gotoAppStoreBtn);

    firstLineBottomLayout->addSpacing(10);
    QPushButton *getPkgFromSrvBtn = new QPushButton("在线获取安装包", this);
    getPkgFromSrvBtn->setMaximumWidth(170);
    firstLineBottomLayout->addWidget(getPkgFromSrvBtn);

    firstLineBottomLayout->addSpacing(10);
    QPushButton *getPkgFromLocalBtn = new QPushButton("离线获取安装包", this);
    getPkgFromLocalBtn->setMaximumWidth(170);
    firstLineBottomLayout->addWidget(getPkgFromLocalBtn);
    firstLineBottomLayout->addSpacing(10);

    // 左置顶
    firstLineBottomLayout->addStretch(1);

    // 底部留10px空隙
    infoFrameLayout->addSpacing(10);

    // connection
    // 搜索框
    connect(m_searchLineEdit, &QLineEdit::editingFinished, this, &AppManagerWidget::onSearchEditingFinished);

    // 过滤菜单
    connect(m_filterMenu, &QMenu::triggered, this, [=](QAction *action) {
        m_showAllAppAction->setChecked(false);
        m_showInstalledAppAction->setChecked(false);
        m_showGuiAppAction->setChecked(false);
        m_showSearchedAppAction->setChecked(false);
        if (m_showAllAppAction == action) {
            m_showAllAppAction->setChecked(true);
            this->showAllAppInfoList();
        } else if (m_showInstalledAppAction == action) {
            m_showInstalledAppAction->setChecked(true);
            this->onlyShowInstalledAppInfoList();
        } else if (m_showGuiAppAction == action) {
            m_showGuiAppAction->setChecked(true);
            this->onlyShowUIAppInfoList();
        } else if (m_showSearchedAppAction == action) {
            m_showSearchedAppAction->setChecked(true);
            this->showSearchedAppInfoList();
        }
    });

    connect(reloadBtn, &QPushButton::clicked, this, [this] {
        this->setLoading(true);
        Q_EMIT m_model->notifyThreadreloadAppInfos();
    });

    connect(m_appListView, &DListView::clicked, this, [this](const QModelIndex &index) {
        int row = index.row();
        if (row > (m_appInfoList.size() - 1)) {
            qDebug() << Q_FUNC_INFO << "列表越界";
            return;
        }

        showAppInfo(m_showingInfoList[row]);
    });

    connect(openBtn, &QPushButton::clicked, this, [this](bool) {
        QString cmd = m_showingAppInfo.desktopInfo.exec;
        QProcess proc;
        proc.startDetached(cmd);
    });

    connect(m_infoSwitchBtn, &DButtonBox::buttonClicked, this, [this](QAbstractButton *btn) {
        if (m_infoBtn == btn) {
            this->showAppInfo(m_showingAppInfo);
        } else if (m_filesBtn == btn) {
            this->showAppFileList(m_showingAppInfo);
        }
    });

    // 卸载
    connect(uninstallBtn, &QPushButton::clicked, this, [this](bool) {
        this->m_model->notifyThreadUninstallPkg(m_showingAppInfo.pkgName);
    });

    // 跳转到商店
    connect(gotoAppStoreBtn, &QPushButton::clicked, this, [this](bool) {
        this->m_model->openStoreAppDetailPage(m_showingAppInfo.pkgName);
    });

    // 在线获取安装包
    connect(getPkgFromSrvBtn, &QPushButton::clicked, this, [this](bool) {
        PkgDownloadDlg *dlg = new PkgDownloadDlg(m_model, this);
        dlg->show();
    });

    // 离线获取安装包
    connect(getPkgFromLocalBtn, &QPushButton::clicked, this, [this](bool) {
        Q_EMIT this->m_model->notifyThreadBuildPkg(m_showingAppInfo);
        DDialog *dlg = new DDialog(this);
        dlg->setCloseButtonVisible(false);
        QString title = QString("%1安装包构建中...").arg(m_showingAppInfo.pkgName);
        dlg->setTitle(title);
        // 构建目录
        QLabel *buildDirLabel = new QLabel(this);
        QString buildDirLabelStr = QString("生成目录: %1").arg(m_model->getPkgBuildDirPath());
        buildDirLabel->setText(buildDirLabelStr);
        dlg->addContent(buildDirLabel);
        // 构建目录打开按钮
        QPushButton *dirOpenBtn = new QPushButton(this);
        dirOpenBtn->setText("打开目录");
        dlg->addContent(dirOpenBtn);
        connect(dirOpenBtn, &QPushButton::clicked, this, [this](bool checked) {
            Q_UNUSED(checked);
            QDesktopServices::openUrl(m_model->getPkgBuildDirPath());
        });
        // 确定按钮
        dlg->addButton("确定", true);

        // 构建完成
        connect(this->m_model, &AppManagerModel::buildPkgTaskFinished, dlg, [this, dlg](bool successed, const AM::AppInfo info) {
            dlg->setEnabled(true);
            QString resultTitle;
            if (successed) {
                resultTitle = QString("%1安装包构建成功").arg(m_showingAppInfo.pkgName);
            } else {
                resultTitle = QString("%1安装包构建失败").arg(m_showingAppInfo.pkgName);
            }
            dlg->setTitle(resultTitle);
        });

        dlg->setEnabled(false);
        dlg->exec();
        // 释放内存
        dlg->deleteLater();
        dlg = nullptr;
    });

    // model信号连接
    connect(m_model, &AppManagerModel::loadAppInfosFinished, this, [this] {
        m_appInfoList = m_model->getAppInfosList();
        // 显示所有应用
        Q_EMIT m_filterMenu->triggered(m_showAllAppAction);
        this->setLoading(false);
    });

    connect(m_model, &AppManagerModel::searchTaskFinished, this, &AppManagerWidget::onSearchTaskFinished);
    connect(m_model, &AppManagerModel::createListViewModeFinished, this, &AppManagerWidget::onCreateListViewModeFinished);

    setLoading(true);
}

AppManagerWidget::~AppManagerWidget()
{
}

void AppManagerWidget::showAppInfo(const AppInfo &info)
{
    m_showingAppInfo = info;
    m_model->setShowingAppInfo(m_showingAppInfo);

    const QString themeIconName = m_showingAppInfo.desktopInfo.themeIconName;
    if (!themeIconName.isEmpty()) {
        m_appAbstractLabel->setPixmap(QIcon::fromTheme(themeIconName).pixmap(40, 40));
    } else {
        m_appAbstractLabel->setPixmap(QIcon::fromTheme(APP_THEME_ICON_DEFAULT).pixmap(40, 40));
    }
    m_appNameLable->setText(m_showingAppInfo.desktopInfo.appName);

    m_infoBtn->setChecked(true);

    m_appInfoTextEdit->setText(formateAppInfo(m_showingAppInfo));
    m_appInfoTextEdit->show();
    m_appFileListTextEdit->hide();
}

void AppManagerWidget::showAppFileList(const AppInfo &info)
{
    m_showingAppInfo = info;

    const QString themeIconName = m_showingAppInfo.desktopInfo.themeIconName;
    if (!themeIconName.isEmpty()) {
        m_appAbstractLabel->setPixmap(QIcon::fromTheme(themeIconName).pixmap(40, 40));
    } else {
        m_appAbstractLabel->setPixmap(QIcon::fromTheme(APP_THEME_ICON_DEFAULT).pixmap(40, 40));
    }
    m_appNameLable->setText(m_showingAppInfo.desktopInfo.appName);

    m_filesBtn->setChecked(true);

    m_appInfoTextEdit->hide();
    QString fileListText;
    for (const QString &filePath : m_showingAppInfo.installedPkgInfo.installedFileList) {
        fileListText += QString("%1\n").arg(filePath);
    }
    m_appFileListTextEdit->setText(fileListText);
    m_appFileListTextEdit->show();
}

void AppManagerWidget::onSearchEditingFinished()
{
    this->setFocus();
    QString inputText = m_searchLineEdit->text();
    Q_EMIT m_model->notifyThreadStartSearchTask(inputText);
}

void AppManagerWidget::onSearchTaskFinished()
{
    // 显示搜索的应用
    Q_EMIT m_filterMenu->triggered(m_showSearchedAppAction);
}

void AppManagerWidget::onCreateListViewModeFinished()
{
    QStandardItemModel *model = m_model->getListViewModel();
    m_appListView->setModel(model);

    if (nullptr != m_appListModel) {
        m_appListModel->deleteLater();
        m_appListModel = model;
    }

    m_showingInfoList = m_model->getShowingAppInfoList();

    if (!m_showingInfoList.isEmpty()) {
        m_appListView->setCurrentIndex(m_appListModel->index(0, 0));
        showAppInfo(m_showingInfoList[0]);
    }

    // 更新应用个数标签
    QString showingTypeStr;
    for (QAction *action : m_filterMenu->actions()) {
        if (action->isChecked()) {
            showingTypeStr = action->text();
            break;
        }
    }
    m_appCountLabel->setText(QString("%1：共%2个").arg(showingTypeStr).arg(m_showingInfoList.size()));
}

QString AppManagerWidget::formateAppInfo(const AppInfo &info)
{
    QString text;
    // 本地安装包信息
    text += "本地安装信息\n"
            "-----------------------------\n";
    if (info.isInstalled) {
        text += QString("应用名：%1\n包名：%2\n安装后占用：%3KB\n更新时间：%4\n维护者：%5\n"
                        "架构：%6\n版本：%7\n下载地址：%8\n安装包大小：%9B\n主页：%10\n"
                        "桌面文件：%11\n执行命令：%12\n执行路径：%13\n依赖：%14\n描述：%15\n\n")
                    .arg(info.desktopInfo.appName)
                    .arg(info.installedPkgInfo.pkgName)
                    .arg(info.installedPkgInfo.installedSize)
                    .arg(info.installedPkgInfo.updatedTime)
                    .arg(info.installedPkgInfo.maintainer)
                    .arg(info.installedPkgInfo.arch)
                    .arg(info.installedPkgInfo.version)
                    .arg(info.installedPkgInfo.downloadUrl)
                    .arg(info.installedPkgInfo.pkgSize)
                    .arg(info.installedPkgInfo.homepage)
                    .arg(info.desktopInfo.desktopPath)
                    .arg(info.desktopInfo.exec)
                    .arg(info.desktopInfo.execPath)
                    .arg(info.installedPkgInfo.depends)
                    .arg(info.installedPkgInfo.description);
    } else {
        text += "(未安装)\n\n";
    }

    // 仓库安装包信息
    text += "仓库安装包信息\n"
            "-----------------------------\n";
    for (const PkgInfo &pkgInfo : info.pkgInfoList) {
        text += m_model->formatePkgInfo(pkgInfo);
    }

    return text;
}

void AppManagerWidget::showAllAppInfoList()
{
    Q_EMIT m_model->notifyThreadCreateListViewMode(m_appInfoList);
}

void AppManagerWidget::onlyShowInstalledAppInfoList()
{
    QList<AM::AppInfo> installedAppInfoList;
    for (const AppInfo &info : m_appInfoList) {
        if (!info.isInstalled) {
            continue;
        }
        installedAppInfoList.append(info);
    }

    Q_EMIT m_model->notifyThreadCreateListViewMode(installedAppInfoList);
}

void AppManagerWidget::onlyShowUIAppInfoList()
{
    QList<AM::AppInfo> uiAppInfoList;
    for (const AppInfo &info : m_appInfoList) {
        if (info.desktopInfo.desktopPath.isEmpty()) {
            continue;
        }
        uiAppInfoList.append(info);
    }

    Q_EMIT m_model->notifyThreadCreateListViewMode(uiAppInfoList);
}

void AppManagerWidget::showSearchedAppInfoList()
{
    QList<AM::AppInfo> searchedList = m_model->getSearchedAppInfoList();

    Q_EMIT m_model->notifyThreadCreateListViewMode(searchedList);
}

void AppManagerWidget::setLoading(bool loading)
{
    if (loading) {
        m_waitingSpinner->start();
        m_waitingSpinner->show();
        m_contentWidget->hide();
    } else {
        m_waitingSpinner->hide();
        m_waitingSpinner->stop();
        m_contentWidget->show();
    }
}
