#pragma once
#include "appmanagerwidget.h"

#include <DMainWindow>

DWIDGET_BEGIN_NAMESPACE
class DBlurEffectWidget;
DWIDGET_END_NAMESPACE

DWIDGET_USE_NAMESPACE
DCORE_USE_NAMESPACE

class MainWindow : public DMainWindow
{
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);
    virtual ~MainWindow() override;

protected:
    virtual void resizeEvent(QResizeEvent *event) override;
    virtual void closeEvent(QCloseEvent *event) override;

private Q_SLOTS:
    // 安装包安装完成时
    void onPkgInstallFinished(bool successed, const QString &err);
    void openSparkStoreNeedBeInstallDlg();

private:
    void updateUIByRunningStatus();

private:
    DBlurEffectWidget *m_barBlurBg;
    DBlurEffectWidget *m_centralWidgetBlurBg;
    bool m_isDeepin;
    // 应用管理
    AppManagerModel *m_appManagerModel;
    AppManagerWidget *m_appManagerWidget;
};
