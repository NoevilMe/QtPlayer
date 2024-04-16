#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QFile>
#include <QStyle>
#include <QTimer>

#include <QGuiApplication>
#include <QScreen>

#include <QWKWidgets/widgetwindowagent.h>
#include <windowbar/windowbar.h>
#include <windowbar/windowbutton.h>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow) {
    installWindowAgent();

    ui->setupUi(this);

    ui->openGLWidget->init(1280, 720);

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
        file->addAction(new QAction(tr("打开(&O)"), menuBar));
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

#ifdef Q_OS_WIN
        settings->addSeparator();
#elif defined(Q_OS_MAC)
        settings->addAction(darkBlurAction);
        settings->addAction(lightBlurAction);
        settings->addAction(noBlurAction);
#endif

        menuBar->addMenu(file);
        menuBar->addSeparator();
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

void MainWindow::loadStyleSheet(Theme theme) {
    if (!styleSheet().isEmpty() && theme == currentTheme)
        return;
    currentTheme = theme;

    if (QFile qss(theme == Dark ? QStringLiteral(":/dark-style.qss")
                                : QStringLiteral(":/light-style.qss"));
        qss.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setStyleSheet(QString::fromUtf8(qss.readAll()));
        //        Q_EMIT themeChanged();
    }
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
    if (fsWidget_ != nullptr && watched == fsWidget_ &&
        event->type() == QEvent::KeyPress) {
        qDebug() << "ESC";
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Escape) {
            //对pushButton实现模拟点击
            //定义左键点击事件，Qt::NoModifier代表无其他修饰键被按下
            QMouseEvent mouseDown(QEvent::MouseButtonPress, QPoint(1, 1),
                                  Qt::LeftButton, Qt::LeftButton,
                                  Qt::NoModifier);
            //定义左键释放事件，Qt::NoModifier代表无其他修饰键被按下
            QMouseEvent mouseUp(QEvent::MouseButtonRelease, QPoint(1, 1),
                                Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            //向按钮pushButton发送鼠标左键按下事件，之后发送鼠标左键释放事件，模拟一次点击
            QApplication::sendEvent(ui->pushButtonFullScreen, &mouseDown);
            QApplication::sendEvent(ui->pushButtonFullScreen, &mouseUp);

            return true; // 事件已处理，不传递给其他对象
        }
    }
    return QMainWindow::eventFilter(watched, event); // 将事件传递给基类处理
}

void MainWindow::on_pushButtonList_toggled(bool checked) {
    if (checked) {
        ui->listWidgetFiles->hide();
    } else {
        ui->listWidgetFiles->show();
    }
}

void MainWindow::on_pushButtonFullScreen_toggled(bool checked) {
    // https://blog.csdn.net/gdizcm/article/details/131649492
    // https://blog.csdn.net/bai2010bingbing/article/details/91378903
    // https://www.cnblogs.com/wuhanpjf/p/11247770.html
    // https://www.cnblogs.com/lvdongjie/p/3758025.html

    if (checked) {
        qDebug() << "enable full screen";

//        auto scs = QGuiApplication::screens();
//        for(auto &s: scs) {
//            qDebug() <<s->geometry();
//        }

//        auto ps = this->screen();
//        qDebug() << "this screen" <<ps->geometry();

//        fsWidget_ = ui->centralwidget;
//        fsParent_ = fsWidget_->parentWidget();
//        fsFlags_ = fsWidget_->windowFlags();

//        fsWidget_->setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
//        fsWidget_->setFocus();
//        fsWidget_->installEventFilter(this);
        this->menuWidget()->hide();
        this->showFullScreen();
//        this->hide();

    } else {
        qDebug() << "disable full screen";
        //        ui->centralwidget->setWindowFlags(Qt::Window|Qt::WindowStaysOnTopHint|Qt::FramelessWindowHint);
        //        ui->centralwidget->setFocus();
        //        ui->centralwidget->showFullScreen();
        //        fsWidget_->setParent(fsParent_);
//        fsWidget_->setWindowFlags(fsFlags_);
//        fsWidget_->showNormal();

//        fsWidget_->removeEventFilter(this);
//        fsWidget_ = nullptr;
//        fsParent_ = nullptr;
        this->menuWidget()->show();
        this->showNormal();
//        this->show();
    }

    //    ui->centralwidget->raise();
    //    this->show();
    //    this->showFullScreen();
    //    this->raise();
    //    ui->openGLWidget->show();
    //    ui->openGLWidget->showFullScreen();
    //    ui->openGLWidget->raise();
}
