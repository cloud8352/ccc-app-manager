#pragma once

#include "common/appmanagercommon.h"
#include "appmanagermodel.h"

#include <DFrame>

#include <QStandardItem>
#include <QComboBox>
#include <QTextCursor>

class AppManagerModel;

DWIDGET_BEGIN_NAMESPACE
class DListView;
class DBlurEffectWidget;
class DButtonBoxButton;
class DButtonBox;
class DSpinner;
DWIDGET_END_NAMESPACE

class QStandardItemModel;
class QTextEdit;
class QLabel;
class QWidget;
class QLineEdit;
class QMenu;

DWIDGET_USE_NAMESPACE

class AppManagerWidget : public QWidget
{
    Q_OBJECT
public:
    enum DisplayRangeType {
        All = 0,
        Installed,
        Gui,
        Searched,
        VerHeld //保持版本
    };

    AppManagerWidget(AppManagerModel *model, QWidget *parent = nullptr);
    virtual ~AppManagerWidget() override;

public Q_SLOTS:
    void showAppInfo(const AM::AppInfo &info);
    void showAppFileList(const AM::AppInfo &info);
    void onSearchEditingFinished();
    void onSearchTaskFinished();
    // 软件安装变动
    void onAppInstalled(const AM::AppInfo &appInfo);
    void onAppUpdated(const AM::AppInfo &appInfo);
    void onAppUninstalled(const AM::AppInfo &appInfo);
    // 当排序器出发后
    void onSorterMenuTriggered(QAction *action);

private:
    QString formateAppInfo(const AM::AppInfo &info);

    // 根据应用信息列表设置标准单元数据对象数据
    void setItemModelFromAppInfoList(const QList<AM::AppInfo> &appInfoList);
    void showAllAppInfoList();
    void onlyShowInstalledAppInfoList();
    void onlyShowUIAppInfoList();
    void showSearchedAppInfoList();
    void showVerHeldAppInfoList();

    void setLoading(bool loading);
    QList<QStandardItem *> createViewItemList(const AM::AppInfo &appInfo);
    void updateItemFromAppInfo(QStandardItem *item, const AM::AppInfo &appInfo);
    AppInfo getAppInfoFromModelIndex(const QModelIndex &index);
    AppInfo getAppInfoFromListViewModelByPkgName(const QString &pkgName);

    // 更新应用个数标签
    void updateAppCountLabel();
    // 更新高亮显示文字
    void updateHighlightText();
    // 移动到下一个高亮显示文字
    void moveToNextHighlightText();

private:
    QList<AM::AppInfo> m_appInfoList;
    AM::AppInfo m_showingAppInfo;

    AppManagerModel *m_model;

    DSpinner *m_waitingSpinner;
    QWidget *m_contentWidget;

    QLineEdit *m_searchLineEdit;
    QMenu *m_filterMenu;
    QAction *m_showAllAppAction;
    QAction *m_showInstalledAppAction;
    QAction *m_showGuiAppAction;
    QAction *m_showSearchedAppAction;
    QAction *m_showVerHeldAppAction; // 保持版本的app过滤按钮
    DisplayRangeType m_displayRangeType; // 显示范围类型

    QMenu *m_sorterMenu;
    QAction *m_descendingSortByNameAction;
    QAction *m_descendingSortByInstalledSizeAction;
    QAction *m_descendingSortByUpdatedTimeAction;
    QAction *m_currentSortingAction;

    QStandardItemModel *m_appListModel;
    DListView *m_appListView;
    QLabel *m_appCountLabel; // 应用个数标签
    QLabel *m_appAbstractLabel;
    QLabel *m_appNameLable;
    QComboBox *m_holdVerComboBox; // 是否保持版本下拉框

    DButtonBoxButton *m_infoBtn;
    DButtonBoxButton *m_filesBtn;
    DButtonBox *m_infoSwitchBtn;
    QLineEdit *m_findLineEdit;
    QList<QTextCursor> m_highlightCursorList;
    QTextCursor m_currentMovedCursor;
    QTextEdit *m_appInfoTextEdit;
    QTextEdit *m_appFileListTextEdit;
};
