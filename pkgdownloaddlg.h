#pragma once
#include "appmanagercommon.h"

#include <DFrame>

class AppManagerModel;

DWIDGET_BEGIN_NAMESPACE
class DBlurEffectWidget;
class DTitlebar;
class DProgressBar;
DWIDGET_END_NAMESPACE

class QMenu;
class QAction;
class QPushButton;
class QLabel;
class QTextEdit;

DWIDGET_USE_NAMESPACE

class PkgDownloadDlg : public DFrame
{
    Q_OBJECT
public:
    explicit PkgDownloadDlg(const AM::AppInfo &appInfo, AppManagerModel *model, QWidget *parent = nullptr);
    virtual ~PkgDownloadDlg() override;

protected:
    virtual void resizeEvent(QResizeEvent *event) override;
    virtual void closeEvent(QCloseEvent *event) override;

public Q_SLOTS:
    void onVerSelectMenuTrigered(QAction *action);
    void onFileDownloadProgressChanged(const QString &url, qint64 bytesRead, qint64 totalBytes);

private:
    void initConnection();
    void updateUI();

private:
    AppManagerModel *m_model;
    AM::AppInfo m_showingAppInfo;
    DBlurEffectWidget *m_centralWidgetBlurBg;
    DTitlebar *m_titlebar;

    AM::PkgInfo m_selectedPkgInfo;
    QMenu *m_versionSelectMenu;
    QList<QAction *> m_versionActionList;
    QPushButton *m_versionSelectBtn;
    QPushButton *m_downloadBtn;
    bool m_isDownloading;
    QPushButton *m_openDirBtn;
    QLabel *m_pkgSizeLable;
    DProgressBar *m_progressBar;
    QTextEdit *m_infoEdit;
};
