#pragma once

#include "appmanagercommon.h"
#include "appmanagermodel.h"

//#include <DFrame>
#include <DtkWidgets>

class AppManagerModel;

DWIDGET_BEGIN_NAMESPACE
//class DListView;
//class DBlurEffectWidget;
//class DButtonBoxButton;
//class DButtonBox;
//class DSpinner;
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
    AppManagerWidget(AppManagerModel *model, QWidget *parent = nullptr);
    virtual ~AppManagerWidget() override;

public Q_SLOTS:
    void showAppInfo(const AM::AppInfo &info);
    void showAppFileList(const AM::AppInfo &info);
    void onSearchEditingFinished();
    void onSearchTaskFinished();
    void onCreateListViewModeFinished();

private:
    QString formateAppInfo(const AM::AppInfo &info);

    void showAllAppInfoList();
    void onlyShowInstalledAppInfoList();
    void onlyShowUIAppInfoList();
    void showSearchedAppInfoList();

    void setLoading(bool loading);

private:
    QList<AM::AppInfo> m_appInfoList;
    QList<AM::AppInfo> m_showingInfoList;
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

    QStandardItemModel *m_appListModel;
    DListView *m_appListView;
    QLabel *m_appCountLabel; // 应用个数标签
    QLabel *m_appAbstractLabel;
    QLabel *m_appNameLable;

    DPushButton *m_infoBtn;
    DPushButton *m_filesBtn;
    QTextEdit *m_appInfoTextEdit;
    QTextEdit *m_appFileListTextEdit;
};
