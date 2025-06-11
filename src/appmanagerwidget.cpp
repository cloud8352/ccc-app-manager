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

// 高亮文字背景颜色
const QColor HighlightTextBgColor(255, 255, 0, 190);
// 当前定位到的高亮文字背景颜色
const QColor LocatedHighlightTextBgColor(0, 0, 255, 120);

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
    , m_sorterMenu(nullptr)
    , m_descendingSortByNameAction(nullptr)
    , m_descendingSortByInstalledSizeAction(nullptr)
    , m_descendingSortByUpdatedTimeAction(nullptr)
    , m_currentSortingAction(nullptr)
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
    filterBtn->setIcon(QIcon::fromTheme("filter"));
    filterBtn->setFixedWidth(30);
    guideOperatingLayout->addWidget(filterBtn);

    m_filterMenu = new QMenu(this);
    m_showAllAppAction = new QAction("全部应用", this);
    m_showAllAppAction->setCheckable(true);
    m_filterMenu->addAction(m_showAllAppAction);
    m_showInstalledAppAction = new QAction("已安装应用", this);
    m_showInstalledAppAction->setCheckable(true);
    m_filterMenu->addAction(m_showInstalledAppAction);
    m_showGuiAppAction = new QAction("界面应用", this);
    m_showGuiAppAction->setCheckable(true);
    m_filterMenu->addAction(m_showGuiAppAction);
    m_showSearchedAppAction = new QAction("搜索结果", this);
    m_showSearchedAppAction->setCheckable(true);
    m_filterMenu->addAction(m_showSearchedAppAction);
    m_showVerHeldAppAction = new QAction("已保持版本", this);
    m_showVerHeldAppAction->setCheckable(true);
    m_filterMenu->addAction(m_showVerHeldAppAction);

    QPushButton *sorterBtn = new QPushButton(this);
    sorterBtn->setToolTip("排序");
    sorterBtn->setIcon(QIcon::fromTheme("sorter"));
    sorterBtn->setFixedWidth(30);
    guideOperatingLayout->addWidget(sorterBtn);

    m_sorterMenu = new QMenu(this);
    m_descendingSortByNameAction = new QAction("名称", this);
    m_descendingSortByNameAction->setCheckable(true);
    m_sorterMenu->addAction(m_descendingSortByNameAction);
    m_descendingSortByInstalledSizeAction = new QAction("大小", this);
    m_descendingSortByInstalledSizeAction->setCheckable(true);
    m_sorterMenu->addAction(m_descendingSortByInstalledSizeAction);
    m_descendingSortByUpdatedTimeAction = new QAction("时间", this);
    m_descendingSortByUpdatedTimeAction->setCheckable(true);
    m_sorterMenu->addAction(m_descendingSortByUpdatedTimeAction);
    m_currentSortingAction = m_descendingSortByNameAction;

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
                                "不保持版本：会随系统更新而更新，dpkg --get-selections查询该安装包状态为install\n"
                                  "【注意】请谨慎将系统组件保持版本，因为系统组件往往与系统环境强绑定，新版本系统很可能与老版本系统组件不兼容，如dde-session-shell");
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
    // 查找下一个按钮
    DIconButton *findNextContentBtn = new DIconButton(this);
    findNextContentBtn->setToolTip("查找下一个");
    findNextContentBtn->setIcon(QIcon::fromTheme("chevron-down"));
    findNextContentBtn->setFixedSize(30, 30);
    findNextContentBtn->setIconSize(QSize(30, 30));
    findNextContentBtn->setEnabledCircle(true);
    findContentFrameLayout->addWidget(findNextContentBtn);
    // 取消搜索按钮
    DIconButton *cancelSearchBtn = new DIconButton(this);
    cancelSearchBtn->setToolTip("取消查找");
    cancelSearchBtn->setIcon(DStyle::StandardPixmap::SP_CloseButton);
    cancelSearchBtn->setFixedSize(30, 30);
    cancelSearchBtn->setIconSize(QSize(30, 30));
    cancelSearchBtn->setEnabledCircle(true);
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
    // 跳转到应用商店菜单
    QAction *gotoDeepinAppStoreAction = new QAction("深度", this);
    gotoDeepinAppStoreAction->setIcon(QIcon::fromTheme("distributor-logo-deepin"));
    gotoDeepinAppStoreAction->setCheckable(false);
    QAction *gotoSparkAppStoreAction = new QAction("星火", this);
    gotoSparkAppStoreAction->setIcon(QIcon::fromTheme("spark-store"));
    gotoSparkAppStoreAction->setCheckable(false);
    QMenu *gotoAppStoreBtnMenu = new QMenu(this);
    gotoAppStoreBtnMenu->addAction(gotoDeepinAppStoreAction);
    gotoAppStoreBtnMenu->addAction(gotoSparkAppStoreAction);
    // 跳转到应用商店按钮
    QPushButton *gotoAppStoreBtn = new QPushButton("在应用商店中查看", this);
    gotoAppStoreBtn->setMenu(gotoAppStoreBtnMenu);
    gotoAppStoreBtn->setMaximumWidth(190);
    firstLineBottomLayout->addWidget(gotoAppStoreBtn);

    firstLineBottomLayout->addSpacing(10);
    QPushButton *getPkgFromSrvBtn = new QPushButton("在线获取安装包", this);
    getPkgFromSrvBtn->setMaximumWidth(170);
    firstLineBottomLayout->addWidget(getPkgFromSrvBtn);

    firstLineBottomLayout->addSpacing(10);
    // 跳转到应用商店菜单
    QAction *buildPkgAction = new QAction("不包含依赖", this);
    buildPkgAction->setCheckable(false);
    QAction *buildPkgWithDependsAction = new QAction("包含依赖（实验）", this);
    buildPkgWithDependsAction->setCheckable(false);
    QMenu *buildPkgMenu = new QMenu(this);
    buildPkgMenu->addAction(buildPkgAction);
    buildPkgMenu->addAction(buildPkgWithDependsAction);
    QPushButton *getPkgFromLocalBtn = new QPushButton("离线获取安装包", this);
    getPkgFromLocalBtn->setMenu(buildPkgMenu);
    getPkgFromLocalBtn->setMaximumWidth(170);
    firstLineBottomLayout->addWidget(getPkgFromLocalBtn);

    // 左置顶
    firstLineBottomLayout->addStretch(1);

    // 底部留10px空隙
    infoFrameLayout->addSpacing(10);

    // connection
    // 搜索框
    connect(m_searchLineEdit, &QLineEdit::editingFinished, this, &AppManagerWidget::onSearchEditingFinished);

    // 过滤菜单
    connect(filterBtn, &QPushButton::pressed, this, [=] {
        filterBtn->setDown(true);
        m_filterMenu->exec(QCursor::pos());
        filterBtn->setDown(false);
    });
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

    // 排序菜单
    connect(sorterBtn, &QPushButton::pressed, this, [=] {
        sorterBtn->setDown(true);
        m_sorterMenu->exec(QCursor::pos());
        sorterBtn->setDown(false);
    });
    connect(m_sorterMenu, &QMenu::triggered, this, &AppManagerWidget::onSorterMenuTriggered);

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

        AppInfo info = getAppInfoFromModelIndex(index);
        showAppInfo(info);
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
    connect(findNextContentBtn, &DIconButton::clicked, this, &AppManagerWidget::moveToNextHighlightText);
    connect(cancelSearchBtn, &DIconButton::clicked, this, [findContentFrame, openFindToolBtn, this] {
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

    // 跳转到应用商店
    connect(gotoAppStoreBtnMenu, &QMenu::triggered, this, [gotoDeepinAppStoreAction, gotoSparkAppStoreAction, this](QAction *action) {
        if (action == gotoDeepinAppStoreAction) {
            this->m_model->openStoreAppDetailPage(m_showingAppInfo.pkgName);
        } else if (action == gotoSparkAppStoreAction) {
            this->m_model->openSpkStoreAppDetailPage(m_showingAppInfo.pkgName);
        }
    });

    // 在线获取安装包
    connect(getPkgFromSrvBtn, &QPushButton::clicked, this, [this](bool) {
        PkgDownloadDlg *dlg = new PkgDownloadDlg(m_showingAppInfo, m_model, this);
        dlg->show();
    });

    // 离线获取安装包
    connect(buildPkgMenu, &QMenu::triggered, this, [buildPkgAction, buildPkgWithDependsAction, this](QAction *action) {
        bool withDepends = false;
        QString msg;
        if (action == buildPkgAction) {
            withDepends = false;
            msg = "是否开始离线获取安装包？";
        } else if (action == buildPkgWithDependsAction) {
            withDepends = true;
            msg = "是否开始连同依赖一起构建安装包？\n"
                  "（这种方法不能绝对保证打包程序的功能正常，是一种备用方案，请谨慎使用）";
        }

        // 确认窗口
        DDialog confirmDlg(this);
        confirmDlg.setMessage(msg);

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

        Q_EMIT this->m_model->notifyThreadBuildPkg(m_showingAppInfo, withDepends);
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
        if (m_showingAppInfo.installedPkgInfo.version == srvPkgInfo.version
                && m_showingAppInfo.installedPkgInfo.arch == srvPkgInfo.arch) {
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
        m_appListModel->insertRow(0, createViewItemList(appInfo));
        break;
    }
    case Gui: {
        // 有desktop文件
        if (!appInfo.desktopInfo.desktopPath.isEmpty()) {
            m_appListModel->insertRow(0, createViewItemList(appInfo));
        }
        break;
    }
    case VerHeld:
        if (appInfo.installedPkgInfo.isHoldVersion) {
            m_appListModel->insertRow(0, createViewItemList(appInfo));
        }
        break;
    }

    // 刷新右侧显示内容
    if (appInfo.pkgName == m_showingAppInfo.pkgName) {
        // 更改后，显示列表当前选中应用
        AppInfo currentAppInfo = getAppInfoFromListViewModelByPkgName(m_showingAppInfo.pkgName);
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
        int operateType = 0; // 0 - none, 1 - add, 2 - remove, 3 - update
        bool isInList = false;
        for (int i = m_appListModel->rowCount() - 1; i >= 0 ; --i) {
            QStandardItem *item = m_appListModel->item(i, 0);
            AppInfo inListAppInfo = getAppInfoFromModelIndex(item->index());
            if (appInfo.pkgName != inListAppInfo.pkgName) {
                continue;
            }
            isInList = true;

            if (appInfo.desktopInfo.desktopPath.isEmpty()) {
                operateType = 2;
            } else {
                operateType = 3;
            }
            break;
        }
        // 不在gui列表中，并存在desktop信息，则为添加操作
        if (!isInList && !appInfo.desktopInfo.desktopPath.isEmpty()) {
            operateType = 1;
        }
        // 如果不需要操作
        if (0 == operateType) {
            break;
        }

        // 根据操作类型，更新界面和缓存列表
        if (1 == operateType) {
            m_appListModel->insertRow(0, createViewItemList(appInfo));
        } else {
            for (int i = m_appListModel->rowCount() - 1; i >= 0 ; --i) {
                QStandardItem *item = m_appListModel->item(i, 0);
                AppInfo inListAppInfo = getAppInfoFromModelIndex(item->index());
                if (appInfo.pkgName != inListAppInfo.pkgName) {
                    continue;
                }

                if (2 == operateType) {
                    m_appListModel->removeRow(i);
                } else if (3 == operateType) {
                    updateItemFromAppInfo(item, appInfo);
                }
                break;
            }
        }

        break;
    }
    case VerHeld: {
        int operateType = 0; // 0 - none, 1 - add, 2 - remove, 3 - update
        bool isInList = false;
        for (int i = m_appListModel->rowCount() - 1; i >= 0 ; --i) {
            QStandardItem *item = m_appListModel->item(i, 0);
            AppInfo inListAppInfo = getAppInfoFromModelIndex(item->index());
            if (appInfo.pkgName != inListAppInfo.pkgName) {
                continue;
            }
            isInList = true;

            if (!appInfo.installedPkgInfo.isHoldVersion) {
                operateType = 2;
            } else {
                operateType = 3;
            }
            break;
        }
        // 不在held列表中，并为保持版本，则为添加操作
        if (!isInList && appInfo.installedPkgInfo.isHoldVersion) {
            operateType = 1;
        }
        // 如果不需要操作
        if (0 == operateType) {
            break;
        }

        // 根据操作类型，更新界面和缓存列表
        if (1 == operateType) {
            m_appListModel->insertRow(0, createViewItemList(appInfo));
        } else {
            for (int i = m_appListModel->rowCount() - 1; i >= 0 ; --i) {
                QStandardItem *item = m_appListModel->item(i, 0);
                AppInfo inListAppInfo = getAppInfoFromModelIndex(item->index());
                if (appInfo.pkgName != inListAppInfo.pkgName) {
                    continue;
                }

                if (2 == operateType) {
                    m_appListModel->removeRow(i);
                } else if (3 == operateType) {
                    updateItemFromAppInfo(item, appInfo);
                }
                break;
            }
        }

        break;
    }
    }

    // 刷新右侧显示内容
    if (appInfo.pkgName == m_showingAppInfo.pkgName) {
        // 更改后，显示列表当前选中应用
        AppInfo currentAppInfo = getAppInfoFromListViewModelByPkgName(m_showingAppInfo.pkgName);
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
        AppInfo currentAppInfo = getAppInfoFromListViewModelByPkgName(m_showingAppInfo.pkgName);
        showAppInfo(currentAppInfo);
    }
    // 更新应用个数标签
    updateAppCountLabel();
}

void AppManagerWidget::onSorterMenuTriggered(QAction *action)
{
    m_descendingSortByNameAction->setChecked(false);
    m_descendingSortByInstalledSizeAction->setChecked(false);
    m_descendingSortByUpdatedTimeAction->setChecked(false);

    action->setChecked(true);
    m_currentSortingAction = action;
    if (m_descendingSortByNameAction == action) {
        m_appListModel->setSortRole(AM_LIST_VIEW_ITEM_DATA_ROLE_APP_NAME);
        m_appListModel->sort(0, Qt::SortOrder::AscendingOrder);
    } else if (m_descendingSortByInstalledSizeAction == action) {
        m_appListModel->setSortRole(AM_LIST_VIEW_ITEM_DATA_ROLE_INSTALLED_SIZE);
        m_appListModel->sort(0, Qt::SortOrder::DescendingOrder);
    } else if (m_descendingSortByUpdatedTimeAction == action) {
        m_appListModel->setSortRole(AM_LIST_VIEW_ITEM_DATA_ROLE_UPDATED_TIME);
        m_appListModel->sort(0, Qt::SortOrder::DescendingOrder);
    }

    if (m_appListModel->rowCount()) {
        QModelIndex modelIndex = m_appListModel->index(0, 0);
        m_appListView->setCurrentIndex(modelIndex);
        AppInfo info = getAppInfoFromModelIndex(modelIndex);
        showAppInfo(info);
    } else {
        // 此类别无应用，则显示空信息
        showAppInfo(AppInfo());
    }
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
                    .arg(info.installedPkgInfo.installedSize ? formatBytes(info.installedPkgInfo.installedSize * 1024, 2) : "0B")
                    .arg(info.installedPkgInfo.updatedTime)
                    .arg(info.installedPkgInfo.maintainer)
                    .arg(info.installedPkgInfo.arch)
                    .arg(info.installedPkgInfo.version)
                    .arg(info.installedPkgInfo.downloadUrl)
                    .arg(info.installedPkgInfo.pkgSize ? formatBytes(info.installedPkgInfo.pkgSize, 1) : "0B")
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
    // 先清空数据
    m_appListModel->removeRows(0, m_appListModel->rowCount());

    for (const AppInfo &info : appInfoList) {
        QList<QStandardItem *> items = createViewItemList(info);
        m_appListModel->appendRow(items);
    }

    // 排序
    onSorterMenuTriggered(m_currentSortingAction);

    // 更新应用个数标签
    updateAppCountLabel();
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

    item->setData(ListViewItemMarginVar, Dtk::ItemDataRole::MarginsRole);

    item->setData(QVariant::fromValue(appInfo), AM_LIST_VIEW_ITEM_DATA_ROLE_ALL_DATA);
    item->setData(appInfo.pkgName, AM_LIST_VIEW_ITEM_DATA_ROLE_PKG_NAME);
    // 为统一排序，将名称全部转换成小写
    QString appNameSortStr = getPinYinInfoFromStr(appName).normalPinYin;
    appNameSortStr = appNameSortStr.toLower();
    item->setData(appNameSortStr, AM_LIST_VIEW_ITEM_DATA_ROLE_APP_NAME);
    item->setData(appInfo.installedPkgInfo.pkgSize, AM_LIST_VIEW_ITEM_DATA_ROLE_PKG_SIZE);
    item->setData(appInfo.installedPkgInfo.installedSize, AM_LIST_VIEW_ITEM_DATA_ROLE_INSTALLED_SIZE);
    item->setData(appInfo.installedPkgInfo.updatedTime, AM_LIST_VIEW_ITEM_DATA_ROLE_UPDATED_TIME);
}

AppInfo AppManagerWidget::getAppInfoFromModelIndex(const QModelIndex &index)
{
    AppInfo info = index.data(AM_LIST_VIEW_ITEM_DATA_ROLE_ALL_DATA).value<AppInfo>();
    return info;
}

AppInfo AppManagerWidget::getAppInfoFromListViewModelByPkgName(const QString &pkgName)
{
    for (int i = m_appListModel->rowCount() - 1; i >= 0; --i) {
        QStandardItem *item = m_appListModel->item(i, 0);
        const AppInfo &appInfo = getAppInfoFromModelIndex(item->index());

        if (appInfo.pkgName == pkgName) {
            return appInfo;
        }
    }

    return AppInfo();
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
    m_appCountLabel->setText(QString("%1：共%2个").arg(showingTypeStr).arg(m_appListModel->rowCount()));
}

void AppManagerWidget::updateHighlightText()
{
    // 清空上次高亮指针列表
    m_highlightCursorList.clear();
    m_currentMovedCursor.clearSelection();
    // 判断待搜索的文档
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
    colorFormat.setBackground(HighlightTextBgColor);
    while (!highlightCursor.isNull() && !highlightCursor.atEnd()) {
        highlightCursor = doc->find(findtext, highlightCursor);
        if (!highlightCursor.isNull()) {
            highlightCursor.mergeCharFormat(colorFormat);
            m_highlightCursorList.append(highlightCursor);
        }

        qApp->processEvents();
    }
    cursor.endEditBlock();
}

void AppManagerWidget::moveToNextHighlightText()
{
    if (m_highlightCursorList.isEmpty()) {
        qInfo() << Q_FUNC_INFO << "highlight text is empty";
        return;
    }

    // 判断待搜索的文档编辑器
    QTextEdit *edit;
    if (m_infoBtn->isChecked()) {
        edit = m_appInfoTextEdit;
    } else if (m_filesBtn->isChecked()) {
        edit = m_appFileListTextEdit;
    } else {
        qWarning() << Q_FUNC_INFO << "no info content need find";
        return;
    }

    QTextCursor nextHighlightCursor;
    int currentCursorAnchor = edit->textCursor().anchor();
//    qInfo() << Q_FUNC_INFO << "currentCursorAnchor" << currentCursorAnchor;
    for (QList<QTextCursor>::const_iterator cIter = m_highlightCursorList.cbegin();
         cIter != m_highlightCursorList.cend(); ++cIter) {
        if (currentCursorAnchor < cIter->anchor()) {
            nextHighlightCursor = *cIter;
            break;
        }
    }
    if (nextHighlightCursor.isNull()) {
        nextHighlightCursor = m_highlightCursorList.first();
    }
    int nextHighlightCursorAnchor = nextHighlightCursor.anchor();
//    qInfo() << Q_FUNC_INFO << "nextHighlightCursorAnchor" << nextHighlightCursorAnchor;

    bool finded = true; // 是否找到
    bool haveLoopFinded = false; // 只循环查找一次
    edit->moveCursor(QTextCursor::MoveOperation::PreviousCharacter, QTextCursor::MoveMode::MoveAnchor);
    while (edit->textCursor().anchor() != nextHighlightCursorAnchor) {
        edit->moveCursor(QTextCursor::MoveOperation::NextCharacter, QTextCursor::MoveMode::MoveAnchor);
        // 当移到文档末尾时还没找到高亮文字指针
        if (edit->textCursor().atEnd()) {
            if (!haveLoopFinded) {
                QTextCursor currentCursor = edit->textCursor();
                currentCursor.movePosition(QTextCursor::MoveOperation::Start, QTextCursor::MoveMode::MoveAnchor);
                edit->setTextCursor(currentCursor);
                haveLoopFinded = true;
                 qInfo() << Q_FUNC_INFO << "move to start" << edit->textCursor().anchor();
            } else {
                finded = false;
                break;
            }
        }
    }

    // 还原上次移到到的高亮文字背景颜色
    QTextCharFormat colorFormat(m_currentMovedCursor.charFormat());
    colorFormat.setBackground(HighlightTextBgColor);
    m_currentMovedCursor.setCharFormat(colorFormat);

    // 设置当前移到到的高亮文字背景颜色
    m_currentMovedCursor = nextHighlightCursor;
    colorFormat = m_currentMovedCursor.charFormat();
    colorFormat.setBackground(LocatedHighlightTextBgColor);
    m_currentMovedCursor.mergeCharFormat(colorFormat);
}
