#pragma once

#include <DMainWindow>
#include <DtkWidgets>

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

private:
    DBlurEffectWidget *m_barBlurBg;
    DBlurEffectWidget *m_centralWidgetBlurBg;
    bool m_isDeepin;
};
