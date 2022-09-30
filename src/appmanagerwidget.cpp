#include "appmanagerwidget.h"
#include "dlg/pkgdownloaddlg.h"

#include <DTitlebar>
#include <DListView>
#include <DFrame>
#include <DBlurEffectWidget>
#include <DApplicationHelper>
#include <DSuggestButton>
#include <DButtonBox>
#include <DSpinner>
#include <DDialog>
#include <DFloatingButton>

#include <QPushButton>
#include <QLineEdit>
#include <QHBoxLayout>
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

Q_DECLARE_METATYPE(QMargins)
const QMargins ListViewItemMargin(5, 3, 5, 3);
const QVariant ListViewItemMarginVar = QVariant::fromValue(ListViewItemMargin);

AppManagerWidget::AppManagerWidget(AppManagerModel *model, QWidget *parent)
    : QWidget(parent)
    , m_model(model)
    , m_waitingSpinner(nullptr)
    , m_contentWidget(nullptr)
    , m_searchLineEdit(nullptr)
    , m_filterMenu(nullptr)
    , m_showAllAppAction(nullptr)
    , m_showInstalledAppAction(nullptr)
    , m_showGuiAppAction(nullptr)
    , m_showSearchedAppAction(nullptr)
    , m_showVerHeldAppAction(nullptr)
    , m_displayRangeType(DisplayRangeType::All)
    , m_appListModel(nullptr)
    , m_appListView(nullptr)
    , m_appCountLabel(nullptr)
    , m_appAbstractLabel(nullptr)
    , m_appNameLable(nullptr)
    , m_holdVerComboBox(nullptr)
    , m_infoBtn(nullptr)
    , m_filesBtn(nullptr)
    , m_infoSwitchBtn(nullptr)
    , m_findLineEdit(nullptr)
    , m_appInfoTextEdit(nullptr)
    , m_appFileListTextEdit(nullptr)
{
    setFocusPolicy(Qt::FocusPolicy::ClickFocus);

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
    reloadBtn->setIcon(QIcon::fromTheme("rotate"));
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
    m_showVerHeldAppAction = new QAction("已保持版本");
    m_showVerHeldAppAction->setCheckable(true);
    m_filterMenu->addAction(m_showVerHeldAppAction);
    filterBtn->setMenu(m_filterMenu);

    // 应用列表
    m_appListModel = new QStandardItemModel(this);

    m_appListView = new DListView(this);
    m_appListView->setSpacing(0);
    m_appListView->setItemSpacing(1);
    m_appListView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_appListView->setFrameShape(QFrame::Shape::NoFrame);
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

    // 是否保持版本
    m_holdVerComboBox = new QComboBox(this);
    m_holdVerComboBox->setToolTip("保持版本：不随系统更新而更新，dpkg --get-selections查询该安装包状态为hold\n"
                                "不保持版本：会随系统更新而更新，dpkg --get-selections查询该安装包状态为install");
    m_holdVerComboBox->setEditable(false);
    m_holdVerComboBox->insertItems(0, {"保持版本", "不保持版本"});
    m_holdVerComboBox->setCurrentIndex(1);
    appAbstractLayout->addWidget(m_holdVerComboBox);

    appAbstractLayout->addSpacing(20);
    DSuggestButton *openBtn = new DSuggestButton(this);
    openBtn->setText("打开");
    appAbstractLayout->addWidget(openBtn);
    appAbstractLayout->addSpacing(10);

    // switch btn area
    DFrame *switchAreaTopSep = new DFrame(this);
    switchAreaTopSep->setBackgroundRole(DPalette::ItemBackground);
    switchAreaTopSep->setFixedHeight(2);
    infoFrameLayout->addWidget(switchAreaTopSep);

    // 信息头部布局
    infoFrameLayout->addSpacing(5);
    QHBoxLayout *infoHeadLayout = new QHBoxLayout;
    infoFrameLayout->addLayout(infoHeadLayout);

    // 开关列表
    QList<DButtonBoxButton *> switchBtnList;
    m_infoBtn = new DButtonBoxButton("信息", this);
    m_infoBtn->setMinimumWidth(100);
    switchBtnList.append(m_infoBtn);

    m_filesBtn = new DButtonBoxButton("文件", this);
    m_filesBtn->setMinimumWidth(100);
    switchBtnList.append(m_filesBtn);

    m_infoSwitchBtn = new DButtonBox(this);
    m_infoSwitchBtn->setButtonList(switchBtnList, true);
    infoHeadLayout->addWidget(m_infoSwitchBtn, 0, Qt::AlignLeft);

    // 打开内容搜索按钮
    QPushButton *openFindToolBtn = new QPushButton(this);
    openFindToolBtn->setText("查找内容");
    infoHeadLayout->addWidget(openFindToolBtn, 0, Qt::AlignRight);
    infoHeadLayout->addSpacing(5);

    // -------------- 信息展示区
    // 查找工具
    infoFrameLayout->addSpacing(3);
    QHBoxLayout *findContentFrameLayout = new QHBoxLayout;
    DFrame *findContentFrame = new DFrame(this);
    findContentFrame->setLayout(findContentFrameLayout);
    infoFrameLayout->addWidget(findContentFrame);
    // 查找提示
    QLabel *findContentLable = new QLabel(this);
    findContentLable->setText("查找");
    findContentFrameLayout->addWidget(findContentLable);
    // 查找编辑栏
    m_findLineEdit = new QLineEdit(this);
    m_findLineEdit->setPlaceholderText("");
    findContentFrameLayout->addWidget(m_findLineEdit);
    // 取消搜索按钮
    DFloatingButton *cancelSearchBtn = new DFloatingButton(this);
    cancelSearchBtn->setIcon(DStyle::StandardPixmap::SP_CloseButton);
    cancelSearchBtn->setFixedSize(30, 30);
    cancelSearchBtn->setIconSize(QSize(30, 30));
    findContentFrameLayout->addWidget(cancelSearchBtn);

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
    QPushButton *gotoAppStoreBtn = new QPushButton("在深度应用商店中查看", this);
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

    firstLineBottomLayout->addSpacing(10);
    QPushButton *gotoSpkAppStoreBtn = new QPushButton("在星火应用商店中查看", this);
    gotoSpkAppStoreBtn->setMaximumWidth(170);
    firstLineBottomLayout->addWidget(gotoSpkAppStoreBtn);
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
        m_showVerHeldAppAction->setChecked(false);
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
        } else if (m_showVerHeldAppAction == action) {
            m_showVerHeldAppAction->setChecked(true);
            this->showVerHeldAppInfoList();
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
        m_model->startDetachedDesktopExec(m_showingAppInfo.desktopInfo.exec);
    });

    connect(m_holdVerComboBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [this](int index) {
        m_holdVerComboBox->blockSignals(true);
        m_holdVerComboBox->setCurrentIndex(m_showingAppInfo.installedPkgInfo.isHoldVersion ? 0 : 1);
        m_holdVerComboBox->blockSignals(false);
        Q_EMIT m_model->notigyThreadHoldPkgVersion(m_showingAppInfo.pkgName, 0 == index);
    });

    connect(m_infoSwitchBtn, &DButtonBox::buttonClicked, this, [this](QAbstractButton *btn) {
        if (m_infoBtn == btn) {
            this->showAppInfo(m_showingAppInfo);
        } else if (m_filesBtn == btn) {
            this->showAppFileList(m_showingAppInfo);
        }
    });

    connect(openFindToolBtn, &QPushButton::clicked, this, [findContentFrame, openFindToolBtn] {
        bool visibleNeedChange = !findContentFrame->isVisible();
        findContentFrame->setVisible(visibleNeedChange);
        openFindToolBtn->setDown(visibleNeedChange);
    });

    connect(m_findLineEdit, &QLineEdit::editingFinished, this, &AppManagerWidget::updateHighlightText);
    connect(cancelSearchBtn, &DFloatingButton::clicked, this, [findContentFrame, openFindToolBtn, this] {
        findContentFrame->setVisible(false);
        openFindToolBtn->setDown(false);
        // 清空文本搜索内容
        m_findLineEdit->setText("");
        this->updateHighlightText();
    });

    // 卸载
    connect(uninstallBtn, &QPushButton::clicked, this, [this](bool) {
        this->m_model->notifyThreadUninstallPkg(m_showingAppInfo.pkgName);
    });

    // 跳转到深度商店
    connect(gotoAppStoreBtn, &QPushButton::clicked, this, [this](bool) {
        this->m_model->openStoreAppDetailPage(m_showingAppInfo.pkgName);
    });
    //跳转到星火商店
    connect(gotoSpkAppStoreBtn, &QPushButton::clicked, this, [this](bool) {
        this->m_model->openSpkStoreAppDetailPage(m_showingAppInfo.pkgName);
    });

    // 在线获取安装包
    connect(getPkgFromSrvBtn, &QPushButton::clicked, this, [this](bool) {
        PkgDownloadDlg *dlg = new PkgDownloadDlg(m_showingAppInfo, m_model, this);
        dlg->show();
    });

    // 离线获取安装包
    connect(getPkgFromLocalBtn, &QPushButton::clicked, this, [this](bool) {
        // 确认窗口
        DDialog confirmDlg;
        confirmDlg.setMessage("是否开始离线获取安装包？");

        // 取消按钮
        confirmDlg.addButton("取消", false);
        // 确定按钮
        confirmDlg.addButton("确定", true);
        int ret = confirmDlg.exec();
        if (1 == ret) {
            confirmDlg.deleteLater();
        } else {
            return;
        }

        Q_EMIT this->m_model->notifyThreadBuildPkg(m_showingAppInfo);
        DDialog *dlg = new DDialog(this);
        dlg->setCloseButtonVisible(false);
        QString title = QString("%1安装包构建中...").arg(m_showingAppInfo.pkgName);
        dlg->setTitle(title);

        // 窗口内容控件
        QWidget *contentWidget = new QWidget(dlg);
        QVBoxLayout *contentLayout = new QVBoxLayout;
        contentWidget->setLayout(contentLayout);
        dlg->addContent(contentWidget);

        // 构建目录
        QLabel *buildDirLabel = new QLabel(dlg);
        QString buildDirLabelStr = QString("生成目录: %1").arg(m_model->getPkgBuildDirPath());
        buildDirLabel->setText(buildDirLabelStr);
        contentLayout->addWidget(buildDirLabel);
        // 构建目录打开按钮
        QPushButton *dirOpenBtn = new QPushButton(dlg);
        dirOpenBtn->setText("打开目录");
        contentLayout->addWidget(dirOpenBtn);
        connect(dirOpenBtn, &QPushButton::clicked, this, [this](bool checked) {
            Q_UNUSED(checked);
            QDesktopServices::openUrl(m_model->getPkgBuildDirPath());
        });
        // 确定按钮
        QPushButton *okBtn = new QPushButton(dlg);
        okBtn->setText("确定");
        connect(okBtn, &QPushButton::clicked, dlg, &DDialog::close);
        contentLayout->addWidget(okBtn);

        // 构建完成
        connect(this->m_model, &AppManagerModel::buildPkgTaskFinished, dlg, [this, contentWidget, dlg](bool successed, const AM::AppInfo info) {
            contentWidget->setEnabled(true);
            QString resultTitle;
            if (successed) {
                resultTitle = QString("%1安装包构建成功").arg(m_showingAppInfo.pkgName);
            } else {
                resultTitle = QString("%1安装包构建失败").arg(m_showingAppInfo.pkgName);
            }
            dlg->setTitle(resultTitle);
        });

        contentWidget->setEnabled(false);
        dlg->exec();
        // 释放内存
        dlg->deleteLater();
        dlg = nullptr;
    });

    // model信号连接
    connect(m_model, &AppManagerModel::loadAppInfosFinished, this, [this] {
        m_appInfoList = m_model->getAppInfosList();
        // 默认显示界面应用
        Q_EMIT m_filterMenu->triggered(m_showGuiAppAction);
        this->setLoading(false);
    });

    connect(m_model, &AppManagerModel::searchTaskFinished, this, &AppManagerWidget::onSearchTaskFinished);

    // 包安装变动
    connect(m_model, &AppManagerModel::appInstalled, this, &AppManagerWidget::onAppInstalled);
    connect(m_model, &AppManagerModel::appUpdated, this, &AppManagerWidget::onAppUpdated);
    connect(m_model, &AppManagerModel::appUninstalled, this, &AppManagerWidget::onAppUninstalled);

    // post init
    findContentFrame->setVisible(false);
    setLoading(true);
}

AppManagerWidget::~AppManagerWidget()
{
}

void AppManagerWidget::showAppInfo(const AppInfo &info)
{
    m_showingAppInfo = info;

    // 拓展仓库应用信息
    for (PkgInfo &srvPkgInfo : m_showingAppInfo.pkgInfoList) {
        if (!m_model->extendPkgInfo(srvPkgInfo)) {
            continue;
        }

        // 根据版本找到候选包中对应的包大小和下载地址
        if (m_showingAppInfo.installedPkgInfo.version == srvPkgInfo.version) {
            m_showingAppInfo.installedPkgInfo.pkgSize = srvPkgInfo.pkgSize;
            m_showingAppInfo.installedPkgInfo.downloadUrl = srvPkgInfo.downloadUrl;
        }
    }

    const QString themeIconName = m_showingAppInfo.desktopInfo.themeIconName;
    if (!themeIconName.isEmpty()) {
        m_appAbstractLabel->setPixmap(QIcon::fromTheme(themeIconName).pixmap(40, 40));
    } else {
        m_appAbstractLabel->setPixmap(QIcon::fromTheme(APP_THEME_ICON_DEFAULT).pixmap(40, 40));
    }

    QString appName = m_showingAppInfo.desktopInfo.appName;
    if (appName.isEmpty()) {
        appName = m_showingAppInfo.pkgName;
    }
    m_appNameLable->setText(appName);

    // 更新是否保持版本
    m_holdVerComboBox->blockSignals(true);
    m_holdVerComboBox->setCurrentIndex(m_showingAppInfo.installedPkgInfo.isHoldVersion ? 0 : 1);
    m_holdVerComboBox->blockSignals(false);

    m_infoBtn->setChecked(true);

    m_appInfoTextEdit->setText(formateAppInfo(m_showingAppInfo));
    m_appInfoTextEdit->show();
    m_appFileListTextEdit->hide();
}

void AppManagerWidget::showAppFileList(const AppInfo &info)
{
    Q_UNUSED(info);
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

void AppManagerWidget::onAppInstalled(const AM::AppInfo &appInfo)
{
    for (QList<AppInfo>::iterator iter = m_appInfoList.begin();
         iter != m_appInfoList.end(); ++iter) {
        if (appInfo.pkgName == iter->pkgName) {
            *iter = appInfo;
            break;
        }
    }

    switch (m_displayRangeType) {
    case All:
    case Searched: {
        for (QList<AppInfo>::iterator iter = m_showingInfoList.begin();
             iter != m_showingInfoList.end(); ++iter) {
            if (appInfo.pkgName == iter->pkgName) {
                *iter = appInfo;
                break;
            }
        }

        // 更新界面数据
        for (int i = 0; i < m_appListModel->rowCount(); ++i) {
            QStandardItem *item = m_appListModel->item(i, 0);
            if (appInfo.pkgName == item->data(AM_LIST_VIEW_ITEM_DATA_ROLE_PKG_NAME).toString()) {
                updateItemFromAppInfo(item, appInfo);
                break;
            }
        }
        break;
    }
    case Installed: {
        m_showingInfoList.insert(0, appInfo);
        m_appListModel->insertRow(0, createViewItemList(appInfo));
        break;
    }
    case Gui: {
        // 有desktop文件
        if (!appInfo.desktopInfo.desktopPath.isEmpty()) {
            m_showingInfoList.insert(0, appInfo);
            m_appListModel->insertRow(0, createViewItemList(appInfo));
        }
        break;
    }
    case VerHeld:
        if (appInfo.installedPkgInfo.isHoldVersion) {
            m_showingInfoList.insert(0, appInfo);
            m_appListModel->insertRow(0, createViewItemList(appInfo));
        }
        break;
    }

    // 刷新右侧显示内容
    if (appInfo.pkgName == m_showingAppInfo.pkgName) {
        // 更改后，显示列表当前选中应用
        AppInfo currentAppInfo;
        if (0 <= m_appListView->currentIndex().row() &&
                m_appListView->currentIndex().row() < m_showingInfoList.size()) {
            currentAppInfo = m_showingInfoList.at(m_appListView->currentIndex().row());
        }
        showAppInfo(currentAppInfo);
    }
    // 更新应用个数标签
    updateAppCountLabel();
}

void AppManagerWidget::onAppUpdated(const AM::AppInfo &appInfo)
{
    for (QList<AppInfo>::iterator iter = m_appInfoList.begin();
         iter != m_appInfoList.end(); ++iter) {
        if (appInfo.pkgName == iter->pkgName) {
            *iter = appInfo;
            break;
        }
    }

    switch (m_displayRangeType) {
    case All:
    case Searched:
    case Installed: {
        for (QList<AppInfo>::iterator iter = m_showingInfoList.begin();
             iter != m_showingInfoList.end(); ++iter) {
            if (appInfo.pkgName == iter->pkgName) {
                *iter = appInfo;
                break;
            }
        }

        // 更新界面数据
        for (int i = 0; i < m_appListModel->rowCount(); ++i) {
            QStandardItem *item = m_appListModel->item(i, 0);
            if (appInfo.pkgName == item->data(AM_LIST_VIEW_ITEM_DATA_ROLE_PKG_NAME).toString()) {
                updateItemFromAppInfo(item, appInfo);
                break;
            }
        }
        break;
    }
    case Gui: {
        int operateType = 0; // 0 - add, 1 - remove, 2 - update
        for (QList<AppInfo>::iterator iter = m_showingInfoList.end() - 1;
             iter != m_showingInfoList.begin() - 1; --iter) {
            if (appInfo.pkgName != iter->pkgName) {
                continue;
            }

            if (appInfo.desktopInfo.desktopPath.isEmpty()) {
                operateType = 1;
                m_showingInfoList.removeOne(*iter);
            } else {
                operateType = 2;
                *iter = appInfo;
            }
            break;
        }

        // 根据操作类型，更新界面和缓存列表
        if (0 == operateType) {
            m_showingInfoList.insert(0, appInfo);
            m_appListModel->insertRow(0, createViewItemList(appInfo));
        } else if (1 == operateType) {
            for (int i = m_appListModel->rowCount() - 1; i >= 0 ; --i) {
                QStandardItem *item = m_appListModel->item(i, 0);
                if (appInfo.pkgName == item->data(AM_LIST_VIEW_ITEM_DATA_ROLE_PKG_NAME).toString()) {
                    m_appListModel->removeRow(i);
                    break;
                }
            }
        } else if (2 == operateType) {
            for (int i = m_appListModel->rowCount() - 1; i >= 0 ; --i) {
                QStandardItem *item = m_appListModel->item(i, 0);
                if (appInfo.pkgName == item->data(AM_LIST_VIEW_ITEM_DATA_ROLE_PKG_NAME).toString()) {
                    updateItemFromAppInfo(item, appInfo);
                    break;
                }
            }
        }
        break;
    }
    case VerHeld: {
        int operateType = 0; // 0 - add, 1 - remove, 2 - update
        for (QList<AppInfo>::iterator iter = m_showingInfoList.end() - 1;
             iter != m_showingInfoList.begin() - 1; --iter) {
            if (appInfo.pkgName != iter->pkgName) {
                continue;
            }

            if (!appInfo.installedPkgInfo.isHoldVersion) {
                operateType = 1;
                m_showingInfoList.removeOne(*iter);
            } else {
                operateType = 2;
                *iter = appInfo;
            }
            break;
        }

        // 根据操作类型，更新界面和缓存列表
        if (0 == operateType) {
            m_showingInfoList.insert(0, appInfo);
            m_appListModel->insertRow(0, createViewItemList(appInfo));
        } else if (1 == operateType) {
            for (int i = m_appListModel->rowCount() - 1; i >= 0 ; --i) {
                QStandardItem *item = m_appListModel->item(i, 0);
                if (appInfo.pkgName == item->data(AM_LIST_VIEW_ITEM_DATA_ROLE_PKG_NAME).toString()) {
                    m_appListModel->removeRow(i);
                    break;
                }
            }
        } else if (2 == operateType) {
            for (int i = m_appListModel->rowCount() - 1; i >= 0 ; --i) {
                QStandardItem *item = m_appListModel->item(i, 0);
                if (appInfo.pkgName == item->data(AM_LIST_VIEW_ITEM_DATA_ROLE_PKG_NAME).toString()) {
                    updateItemFromAppInfo(item, appInfo);
                    break;
                }
            }
        }
        break;
    }
    }

    // 刷新右侧显示内容
    if (appInfo.pkgName == m_showingAppInfo.pkgName) {
        // 更改后，显示列表当前选中应用
        AppInfo currentAppInfo;
        if (0 <= m_appListView->currentIndex().row() &&
                m_appListView->currentIndex().row() < m_showingInfoList.size()) {
            currentAppInfo = m_showingInfoList.at(m_appListView->currentIndex().row());
        }
        showAppInfo(currentAppInfo);
    }
    // 更新应用个数标签
    updateAppCountLabel();
}

void AppManagerWidget::onAppUninstalled(const AM::AppInfo &appInfo)
{
    for (QList<AppInfo>::iterator iter = m_appInfoList.begin();
         iter != m_appInfoList.end(); ++iter) {
        if (appInfo.pkgName == iter->pkgName) {
            *iter = appInfo;
            break;
        }
    }

    switch (m_displayRangeType) {
    case All:
    case Searched: {
        for (QList<AppInfo>::iterator iter = m_showingInfoList.begin();
             iter != m_showingInfoList.end(); ++iter) {
            if (appInfo.pkgName == iter->pkgName) {
                *iter = appInfo;
                break;
            }
        }

        // 更新界面数据
        for (int i = 0; i < m_appListModel->rowCount(); ++i) {
            QStandardItem *item = m_appListModel->item(i, 0);
            if (appInfo.pkgName == item->data(AM_LIST_VIEW_ITEM_DATA_ROLE_PKG_NAME).toString()) {
                updateItemFromAppInfo(item, appInfo);
                break;
            }
        }
        break;
    }
    case Installed:
    case Gui:
    case VerHeld: {
        for (QList<AppInfo>::iterator iter = m_showingInfoList.end() - 1;
             iter != m_showingInfoList.begin() - 1; --iter) {
            if (appInfo.pkgName == iter->pkgName) {
                m_showingInfoList.removeOne(*iter);
                break;
            }
        }
        // 更新界面数据
        for (int i = m_appListModel->rowCount() - 1; i >= 0 ; --i) {
            QStandardItem *item = m_appListModel->item(i, 0);
            if (appInfo.pkgName == item->data(AM_LIST_VIEW_ITEM_DATA_ROLE_PKG_NAME).toString()) {
                m_appListModel->removeRow(i);
                break;
            }
        }
        break;
    }
    }

    // 刷新右侧显示内容
    if (appInfo.pkgName == m_showingAppInfo.pkgName) {
        // 删除后，显示列表当前选中应用
        AppInfo currentAppInfo;
        if (0 <= m_appListView->currentIndex().row() &&
                m_appListView->currentIndex().row() < m_showingInfoList.size()) {
            currentAppInfo = m_showingInfoList.at(m_appListView->currentIndex().row());
        }
        showAppInfo(currentAppInfo);
    }
    // 更新应用个数标签
    updateAppCountLabel();
}

QString AppManagerWidget::formateAppInfo(const AppInfo &info)
{
    QString text;
    // 本地安装包信息
    text += "本地安装信息\n"
            "-----------------------------\n";
    if (info.isInstalled) {
        text += QString("应用名：%1\n包名：%2\n安装后占用：%3\n更新时间：%4\n维护者：%5\n"
                        "架构：%6\n版本：%7\n下载地址：%8\n安装包大小：%9\n主页：%10\n"
                        "桌面文件：%11\n执行命令：%12\n执行路径：%13\n依赖：%14\n描述：%15\n\n")
                    .arg(info.desktopInfo.appName)
                    .arg(info.installedPkgInfo.pkgName)
                    .arg(info.installedPkgInfo.installedSize ? formatBytes(info.installedPkgInfo.installedSize * 1024, 2) : "0 B")
                    .arg(info.installedPkgInfo.updatedTime)
                    .arg(info.installedPkgInfo.maintainer)
                    .arg(info.installedPkgInfo.arch)
                    .arg(info.installedPkgInfo.version)
                    .arg(info.installedPkgInfo.downloadUrl)
                    .arg(info.installedPkgInfo.pkgSize ? formatBytes(info.installedPkgInfo.pkgSize, 1) : "0 B")
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

void AppManagerWidget::setItemModelFromAppInfoList(const QList<AppInfo> &appInfoList)
{
    // 更新正在显示的应用列表
    m_showingInfoList = appInfoList;

    // 更新应用个数标签
    updateAppCountLabel();

    // 先清空数据
    m_appListModel->removeRows(0, m_appListModel->rowCount());

    for (const AppInfo &info : appInfoList) {
        QString appName = info.desktopInfo.appName;
        if (appName.isEmpty()) {
            appName = info.pkgName;
        }
        QStandardItem *item = new QStandardItem(appName);
        item->setData(ListViewItemMarginVar, Dtk::ItemDataRole::MarginsRole);
        if (!info.desktopInfo.themeIconName.isEmpty()) {
            item->setIcon(QIcon::fromTheme(info.desktopInfo.themeIconName));
        } else {
            item->setIcon(QIcon::fromTheme(APP_THEME_ICON_DEFAULT));
        }

        item->setData(info.pkgName, AM_LIST_VIEW_ITEM_DATA_ROLE_PKG_NAME);
        m_appListModel->appendRow(QList<QStandardItem *> {item});
    }

    if (!m_showingInfoList.isEmpty()) {
        m_appListView->setCurrentIndex(m_appListModel->index(0, 0));
        showAppInfo(m_showingInfoList[0]);
    } else {
        // 此类别无应用，则显示空信息
        showAppInfo(AppInfo());
    }
}

void AppManagerWidget::showAllAppInfoList()
{
    m_displayRangeType = All;
    setItemModelFromAppInfoList(m_appInfoList);
}

void AppManagerWidget::onlyShowInstalledAppInfoList()
{
    m_displayRangeType = Installed;
    QList<AM::AppInfo> installedAppInfoList;
    for (const AppInfo &info : m_appInfoList) {
        if (!info.isInstalled) {
            continue;
        }
        installedAppInfoList.append(info);
    }

    setItemModelFromAppInfoList(installedAppInfoList);
}

void AppManagerWidget::onlyShowUIAppInfoList()
{
    m_displayRangeType = Gui;
    QList<AM::AppInfo> uiAppInfoList;
    for (const AppInfo &info : m_appInfoList) {
        if (info.desktopInfo.desktopPath.isEmpty()) {
            continue;
        }
        uiAppInfoList.append(info);
    }

    setItemModelFromAppInfoList(uiAppInfoList);
}

void AppManagerWidget::showSearchedAppInfoList()
{
    m_displayRangeType = Searched;
    QList<AM::AppInfo> searchedList = m_model->getSearchedAppInfoList();

    setItemModelFromAppInfoList(searchedList);
}

void AppManagerWidget::showVerHeldAppInfoList()
{
    m_displayRangeType = VerHeld;
    QList<AM::AppInfo> verHeldAppInfoList;
    for (const AppInfo &info : m_appInfoList) {
        if (!info.installedPkgInfo.isHoldVersion) {
            continue;
        }
        verHeldAppInfoList.append(info);
    }

    setItemModelFromAppInfoList(verHeldAppInfoList);
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

QList<QStandardItem *> AppManagerWidget::createViewItemList(const AM::AppInfo &appInfo)
{
    // 更新列表显示数据
    QStandardItem *item = new QStandardItem();
    updateItemFromAppInfo(item, appInfo);
    QList<QStandardItem *> itemList;
    itemList << item;
    return itemList;
}

void AppManagerWidget::updateItemFromAppInfo(QStandardItem *item, const AppInfo &appInfo)
{
    QString appName = appInfo.desktopInfo.appName;
    if (appName.isEmpty()) {
        appName = appInfo.pkgName;
    }
    item->setText(appName);

    if (!appInfo.desktopInfo.themeIconName.isEmpty()) {
        item->setIcon(QIcon::fromTheme(appInfo.desktopInfo.themeIconName));
    } else {
        item->setIcon(QIcon::fromTheme(APP_THEME_ICON_DEFAULT));
    }

    item->setData(appInfo.pkgName, AM_LIST_VIEW_ITEM_DATA_ROLE_PKG_NAME);
}

// 更新应用个数标签
void AppManagerWidget::updateAppCountLabel()
{
    QString showingTypeStr;
    for (QAction *action : m_filterMenu->actions()) {
        if (action->isChecked()) {
            showingTypeStr = action->text();
            break;
        }
    }
    m_appCountLabel->setText(QString("%1：共%2个").arg(showingTypeStr).arg(m_showingInfoList.size()));
}

void AppManagerWidget::updateHighlightText()
{
    QTextDocument *doc;
    if (m_infoBtn->isChecked()) {
        doc = m_appInfoTextEdit->document();
    } else if (m_filesBtn->isChecked()) {
        doc = m_appFileListTextEdit->document();
    } else {
        qWarning() << Q_FUNC_INFO << "no info content need find";
        return;
    }
    QTextCursor cursor(doc);
    // 重置全部文字格式
    cursor.select(QTextCursor::SelectionType::Document);
    cursor.setCharFormat(m_appInfoTextEdit->textCursor().charFormat());

    QString findtext = m_findLineEdit->text(); //获得对话框的内容
//    qInfo() << Q_FUNC_INFO << findtext;
    if (findtext.isEmpty()) {
        return;
    }

    QTextCursor highlightCursor(doc);
    // 开始查找
    cursor.beginEditBlock();
    QTextCharFormat colorFormat(highlightCursor.charFormat());
    colorFormat.setBackground(Qt::GlobalColor::red);
    while (!highlightCursor.isNull() && !highlightCursor.atEnd()) {
        highlightCursor = doc->find(findtext, highlightCursor);
        if (!highlightCursor.isNull()) {
            highlightCursor.mergeCharFormat(colorFormat);
        }

        qApp->processEvents();
    }
    cursor.endEditBlock();
}
