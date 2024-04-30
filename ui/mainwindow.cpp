#include "mainwindow.h"
#include "multiplayerform.h"
#include "openmediadialog.h"
#include "playerform.h"
#include "ui_mainwindow.h"

#include <QFile>
#include <QStyle>

#include <QWKWidgets/widgetwindowagent.h>
#include <windowbar/windowbar.h>
#include <windowbar/windowbutton.h>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow) {
    ui->setupUi(this);
    installWindowAgent();

    pagePlayer = new PlayerForm();
    pagePlayer->setObjectName("player");
    pageMonitor = new MultiPlayerForm();
    pageMonitor->setObjectName("monitor");
    ui->stackedWidget->addWidget(pagePlayer);
    ui->stackedWidget->addWidget(pageMonitor);
    ui->stackedWidget->setCurrentWidget(pagePlayer);
    ui->stackedWidget->update();

    loadStyleSheet(Light);
}

MainWindow::~MainWindow() { delete ui; }

bool MainWindow::event(QEvent *event) {
    switch (event->type()) {
    case QEvent::WindowActivate: {
        auto menu = menuWidget();
        menu->setProperty("bar-active", true);
        style()->polish(menu);
        break;
    }

    case QEvent::WindowDeactivate: {
        auto menu = menuWidget();
        menu->setProperty("bar-active", false);
        style()->polish(menu);
        break;
    }

    default:
        break;
    }
    return QMainWindow::event(event);
}

void MainWindow::closeEvent(QCloseEvent *event) {
    // if (!(qApp->keyboardModifiers() & Qt::ControlModifier)) {
    //     QTimer::singleShot(1000, this, &QWidget::show);
    // }
    // player_->Stop();

    event->accept();
}

void MainWindow::installWindowAgent() {
    // 1. Setup window agent
    windowAgent = new QWK::WidgetWindowAgent(this);
    windowAgent->setup(this);

    // 2. Construct your title bar
    auto menuBar = [this]() {
        auto menuBar = new QMenuBar();

        // Virtual menu
        auto file = new QMenu(tr("文件(&F)"), menuBar);

        auto openAction = new QAction(tr("打开(&O)"), menuBar);
        file->addAction(openAction);
        connect(openAction, &QAction::triggered, this,
                &MainWindow::openMediaActionTriggered);
        // connect(openAction, &QAction::triggered, this, [=](bool checked) {
        //     OpenMediaDialog dlg(this);
        //     dlg.exec();
        // });

        file->addSeparator();
        file->addAction(new QAction(tr("退出(&E)"), menuBar));

        // Theme action
        auto darkAction = new QAction(tr("Enable dark theme"), menuBar);
        darkAction->setCheckable(true);
        connect(darkAction, &QAction::triggered, this, [this](bool checked) {
            loadStyleSheet(checked ? Dark : Light); //
        });
        connect(this, &MainWindow::themeChanged, darkAction,
                [this, darkAction]() {
                    darkAction->setChecked(currentTheme == Dark); //
                });

        // Real menu
        auto settings = new QMenu(tr("设置(&S)"), menuBar);
        settings->addAction(darkAction);
        settings->addSeparator();
        auto win = new QMenu(tr("窗口(&W)"), menuBar);
        auto winPlayerAction = new QAction(tr("播放器(&P)"), menuBar);
        winPlayerAction->setCheckable(true);
        win->addAction(winPlayerAction);
        auto winMonitorAction = new QAction(tr("监视器(&M)"), menuBar);
        winMonitorAction->setCheckable(true);
        win->addAction(winMonitorAction);

        connect(winPlayerAction, &QAction::triggered, this, [=](bool checked) {
            winMonitorAction->setChecked(!checked);
            if (checked) {
                switchWindowType(WindowType::Player);
            } else {
                switchWindowType(WindowType::Monitor);
            }
        });
        connect(winMonitorAction, &QAction::triggered, this, [=](bool checked) {
            winPlayerAction->setChecked(!checked);
            if (checked) {
                switchWindowType(WindowType::Monitor);
            } else {
                switchWindowType(WindowType::Player);
            }
        });

        settings->addMenu(win);

#ifdef Q_OS_WIN
        settings->addSeparator();
#elif defined(Q_OS_MAC)
        settings->addAction(darkBlurAction);
        settings->addAction(lightBlurAction);
        settings->addAction(noBlurAction);
#endif

        menuBar->addMenu(file);
        menuBar->addSeparator();
        // menuBar->addMenu(win);
        menuBar->addMenu(settings);

        return menuBar;
    }();
    menuBar->setObjectName(QStringLiteral("win-menu-bar"));

    auto titleLabel = new QLabel();
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setObjectName(QStringLiteral("win-title-label"));

#ifndef Q_OS_MAC
    //    auto iconButton = new QWK::WindowButton();
    //    iconButton->setObjectName(QStringLiteral("icon-button"));
    //    iconButton->setSizePolicy(QSizePolicy::Preferred,
    //    QSizePolicy::Preferred);

    auto minButton = new QWK::WindowButton();
    minButton->setObjectName(QStringLiteral("min-button"));
    minButton->setProperty("system-button", true);
    minButton->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    auto maxButton = new QWK::WindowButton();
    maxButton->setCheckable(true);
    maxButton->setObjectName(QStringLiteral("max-button"));
    maxButton->setProperty("system-button", true);
    maxButton->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    auto closeButton = new QWK::WindowButton();
    closeButton->setObjectName(QStringLiteral("close-button"));
    closeButton->setProperty("system-button", true);
    closeButton->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
#endif

    auto windowBar = new QWK::WindowBar();
#ifndef Q_OS_MAC
    //    windowBar->setIconButton(iconButton);
    windowBar->setMinButton(minButton);
    windowBar->setMaxButton(maxButton);
    windowBar->setCloseButton(closeButton);
#endif
    windowBar->setMenuBar(menuBar);
    windowBar->setTitleLabel(titleLabel);
    windowBar->setHostWidget(this);

    windowAgent->setTitleBar(windowBar);
#ifndef Q_OS_MAC
    //    windowAgent->setSystemButton(QWK::WindowAgentBase::WindowIcon,
    //    iconButton);
    windowAgent->setSystemButton(QWK::WindowAgentBase::Minimize, minButton);
    windowAgent->setSystemButton(QWK::WindowAgentBase::Maximize, maxButton);
    windowAgent->setSystemButton(QWK::WindowAgentBase::Close, closeButton);
#endif
    windowAgent->setHitTestVisible(menuBar, true);

#ifdef Q_OS_MAC
    windowAgent->setSystemButtonAreaCallback([](const QSize &size) {
        static constexpr const int width = 75;
        return QRect(QPoint(size.width() - width, 0),
                     QSize(width, size.height())); //
    });
#endif

    setMenuWidget(windowBar);

    /*
    // 3. Adds simulated mouse events to the title bar buttons
    #ifdef Q_OS_WINDOWS
        // Emulate Window system menu button behaviors
        connect(iconButton, &QAbstractButton::clicked, windowAgent, [this,
    iconButton] { iconButton->setProperty("double-click-close", false);

            // Pick a suitable time threshold
            QTimer::singleShot(75, windowAgent, [this, iconButton]() {
                if (iconButton->property("double-click-close").toBool())
                    return;
                windowAgent->showSystemMenu(iconButton->mapToGlobal(QPoint{0,
    iconButton->height()}));
            });
        });
        connect(iconButton, &QWK::WindowButton::doubleClicked, this,
    [iconButton, this]() { iconButton->setProperty("double-click-close", true);
            close();
        });
    #endif*/

#ifndef Q_OS_MAC
    connect(windowBar, &QWK::WindowBar::minimizeRequested, this,
            &QWidget::showMinimized);
    connect(windowBar, &QWK::WindowBar::maximizeRequested, this,
            [this, maxButton](bool max) {
                if (max) {
                    showMaximized();
                } else {
                    showNormal();
                }

                // It's a Qt issue that if a QAbstractButton::clicked triggers a
                // window's maximization, the button remains to be hovered until
                // the mouse move. As a result, we need to manually send leave
                // events to the button.
                //        emulateLeaveEvent(maxButton);
            });
    connect(windowBar, &QWK::WindowBar::closeRequested, this, &QWidget::close);
#endif
}

void MainWindow::getAllDevices() {}

void MainWindow::switchWindowType(WindowType type) {
    if (type == WindowType::Player) {
        ui->stackedWidget->setCurrentWidget(pagePlayer);
    } else if (type == WindowType::Monitor) {
        ui->stackedWidget->setCurrentWidget(pageMonitor);
    }

    ui->stackedWidget->update();
}

void MainWindow::openMediaActionTriggered(bool checked) {
    OpenMediaDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        qDebug()<<"open media " << (int)dlg.mediaSource.type <<", " << dlg.mediaSource.src;

        pagePlayer->openMedia(dlg.mediaSource);
    }
}

void MainWindow::loadStyleSheet(Theme theme) {
    if (!styleSheet().isEmpty() && theme == currentTheme)
        return;
    currentTheme = theme;

    // qApp->setPalette(QPalette("#132D48"));

    if (QFile qss(theme == Dark ? QStringLiteral(":/dark-style.qss")
                                : QStringLiteral(":/light-style.qss"));
        qss.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setStyleSheet(QString::fromUtf8(qss.readAll()));
        //        Q_EMIT themeChanged();
    }
}
