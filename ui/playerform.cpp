#include "playerform.h"
#include "ui_playerform.h"

#include "player/device.h"

#include <QFile>
#include <QMediaDevices>
#include <QStyle>
#include <QTimer>

#include <QGuiApplication>
#include <QScreen>

PlayerForm::PlayerForm(QWidget *parent)
    : QWidget(parent), ui(new Ui::PlayerForm) {
    ui->setupUi(this);
    ui->listWidgetFiles->hide();

    // loadIcons();

    timerProgress = new QTimer(this); // 定时器-获取当前视频时间
    connect(timerProgress, &QTimer::timeout, this,
            &PlayerForm::timerTimeoutSlot);
    timerProgress->setInterval(500);

    //    listOutputAudioDevices();
    connect(this, &PlayerForm::playDoneSignal, this, &PlayerForm::playDoneSlot);
}

PlayerForm::~PlayerForm() {
    qDebug() << "PlayerForm::~PlayerForm() ";

    stopPlayer();
    stopSpeaker();

    qDebug() << "PlayerForm::~PlayerForm() done ";

    delete ui;
}

bool PlayerForm::openMedia(MediaSource media) {
    stopPlayer();
    stopSpeaker();

    player_.reset(new FFPlayer);
    player_->SetMediaSource(media);
    if (!player_->Open()) {
        qDebug() << "打开失败";
        return false;
    }

    qDebug() << "total seconds " << player_->GetTotalSeconds();
    onTotalSeconds(player_->GetTotalSeconds());

    if (player_->HasAudio()) {
        AudioFormat audioFmt;
        if (!player_->GetAudioFormat(&audioFmt)) {
            qDebug() << "获取音频参数失败";
            return false;
        }

        speaker_.reset(new AudioSpeaker);

        AudioDeviceFormat deviceFmt;
        deviceFmt.channel_count = 2;
        deviceFmt.sample_rate = 44100;
        deviceFmt.sample_fmt = AudioSampleFormat::Int16;
        player_->SetAudioDeviceFormat(deviceFmt);

        player_->SetAudioFrameCallback(
            std::bind(&PlayerForm::playAudio, this, std::placeholders::_1,
                      std::placeholders::_2, std::placeholders::_3));
        player_->SetAudioClockCallback(
            [=]() { return speaker_->AudioClock(); });
        speaker_->start();
    }

    player_->SetVideoFrameCallback(
        std::bind(&PlayerForm::playVideo, this, std::placeholders::_1));

    player_->SetPlayDoneCallback([=]() {
        // 该函数在播放器内部线程中执行，不能直接reset，需要借助信号槽处理
        emit this->playDoneSignal();
    });

    timerProgress->start();

    if (player_->Play()) {
        qDebug() << "播放成功";
        return true;
    } else {
        qDebug() << "播放失败";
        return false;
    }
}

void PlayerForm::playAudio(char *buf, int size, double clock) {
    speaker_->write(buf, size, clock);
}

void PlayerForm::playVideo(AVFrame *frame) {
    if (!ui->openGLWidget->isSupportedFormat(frame->format)) {
        return;
    }

    QSharedPointer<VideoFrame> videoFrame(
        new VideoFrame(frame->format, frame->width, frame->height));

    if (AV_PIX_FMT_YUV420P == videoFrame->pixfmt ||
        AV_PIX_FMT_YUVJ420P == videoFrame->pixfmt) {

        for (int i = 0; i < frame->height; i++) {
            memcpy(videoFrame->data[0] + i * frame->width,
                   frame->data[0] + i * frame->linesize[0],
                   frame->width); // 按行复制数据，末尾有对齐数据
        }

        for (int i = 0; i < frame->height / 2; i++) {
            memcpy(videoFrame->data[1] + i * frame->width / 2,
                   frame->data[1] + i * frame->linesize[1], frame->width / 2);
        }

        for (int i = 0; i < frame->height / 2; i++) {
            memcpy(videoFrame->data[2] + i * frame->width / 2,
                   frame->data[2] + i * frame->linesize[2], frame->width / 2);
        }

    } else if (AV_PIX_FMT_NV12 == videoFrame->pixfmt) {
        for (int i = 0; i < frame->height; i++) {
            memcpy(videoFrame->data[0] + i * frame->width,
                   frame->data[0] + i * frame->linesize[0],
                   frame->width); // 按行复制数据，末尾有对齐数据
        }

        for (int i = 0; i < frame->height / 2; i++) {
            memcpy(videoFrame->data[1] + i * frame->width,
                   frame->data[1] + i * frame->linesize[1],
                   frame->width); // UV数据在一起
        }
    }

    // qDebug() << QThread::currentThreadId() << "playVideo";
    emit ui->openGLWidget->playVideoSignal(videoFrame);
}

void PlayerForm::clickPushButtonFullScreen() {
    // 对pushButton实现模拟点击
    // 定义左键点击事件，Qt::NoModifier代表无其他修饰键被按下
    //  QMouseEvent mouseDown(QEvent::MouseButtonPress, QPoint(1, 1),
    //                        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    //  //定义左键释放事件，Qt::NoModifier代表无其他修饰键被按下
    //  QMouseEvent mouseUp(QEvent::MouseButtonRelease, QPoint(1, 1),
    //                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    //  //向按钮pushButton发送鼠标左键按下事件，之后发送鼠标左键释放事件，模拟一次点击
    //  QApplication::sendEvent(ui->pushButtonFullScreen, &mouseDown);
    //  QApplication::sendEvent(ui->pushButtonFullScreen, &mouseUp);
}

void PlayerForm::listOutputAudioDevices() {
    auto audio_devics = QMediaDevices::audioOutputs();
    for (auto &dev : audio_devics) {
        qDebug() << "audio output id " << dev.id() << ", desc "
                 << dev.description() << ", preferred fmt "
                 << dev.preferredFormat() << ", sample fmts "
                 << dev.supportedSampleFormats() << ", sample rate ["
                 << dev.minimumSampleRate() << ", " << dev.maximumSampleRate()
                 << "], channel [" << dev.minimumChannelCount() << ", "
                 << dev.maximumChannelCount() << "]";
    }
}

void PlayerForm::onTotalSeconds(double seconds) {

    int sec = (int)seconds;
    ui->horizontalSliderProgress->setRange(0, sec);

    QString totalTime;
    QString hStr = QString("0%1").arg(sec / 3600);
    QString mStr = QString("0%1").arg(sec / 60 % 60);
    QString sStr = QString("0%1").arg(sec % 60);
    if (hStr == "00") {
        totalTime = QString("%1:%2").arg(mStr.right(2)).arg(sStr.right(2));
    } else {
        totalTime =
            QString("%1:%2:%3").arg(hStr).arg(mStr.right(2)).arg(sStr.right(2));
    }

    ui->labelTotalTime->setText(totalTime);
}

void PlayerForm::stopPlayer() {
    if (player_) {
        qDebug() << "reset player ...";

        // 如果有正在执行的播放器，PlayDoneCallback的延迟执行可能会释放掉新的播放器
        player_->SetPlayDoneCallback(nullptr);

        if (player_->IsPlaying()) {
            player_->Stop();
        }
        player_.reset();
    }
}

void PlayerForm::stopSpeaker() {
    qDebug() << "stop speaker ...";
    if (speaker_) {
        qDebug() << "reset speaker ...";
        speaker_->Stop();
        speaker_->wait();
        speaker_.reset();
    }
}

// bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
//    if (fsWidget_ != nullptr && watched == fsWidget_ &&
//        event->type() == QEvent::KeyPress) {
//        qDebug() << "ESC";
//        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
//        if (keyEvent->key() == Qt::Key_Escape) {
//            clickPushButtonFullScreen();

//            return true; // 事件已处理，不传递给其他对象
//        }
//    }
//    return QMainWindow::eventFilter(watched, event); // 将事件传递给基类处理
//}

void PlayerForm::keyPressEvent(QKeyEvent *event) {
    // if (this->isFullScreen() && event->key() == Qt::Key_Escape) {
    //     qDebug() << "ESC";
    //     clickPushButtonFullScreen();
    // }
}

void PlayerForm::on_pushButtonList_toggled(bool checked) {
    if (ui->listWidgetFiles->isHidden()) {
        ui->listWidgetFiles->show();
    } else {
        ui->listWidgetFiles->hide();
    }
}

void PlayerForm::on_pushButtonFullScreen_toggled(bool checked) {
    // https://blog.csdn.net/gdizcm/article/details/131649492
    // https://blog.csdn.net/bai2010bingbing/article/details/91378903
    // https://www.cnblogs.com/wuhanpjf/p/11247770.html
    // https://www.cnblogs.com/lvdongjie/p/3758025.html

    // if (checked) {
    //     qDebug() << "enable full screen";

    //     this->menuWidget()->hide();
    //     ui->widgetControl->hide();
    //     ui->listWidgetFiles->hide();
    //     this->showFullScreen();
    //     //        this->hide();

    // } else {
    //     qDebug() << "disable full screen";
    //     this->menuWidget()->show();
    //     ui->widgetControl->show();
    //     this->showNormal();
    //     //        this->show();
    // }
}

void PlayerForm::playDoneSlot() {
    qDebug() << "playDoneSlot";
    player_.reset();
    timerProgress->stop();
}

void PlayerForm::timerTimeoutSlot() {
    if (QObject::sender() == timerProgress) {
        qint64 Sec = player_->GetClock();
        ui->horizontalSliderProgress->setValue(Sec);

        QString curTime;
        QString hStr = QString("0%1").arg(Sec / 3600);
        QString mStr = QString("0%1").arg(Sec / 60 % 60);
        QString sStr = QString("0%1").arg(Sec % 60);
        if (hStr == "00") {
            curTime = QString("%1:%2").arg(mStr.right(2)).arg(sStr.right(2));
        } else {
            curTime = QString("%1:%2:%3")
                          .arg(hStr)
                          .arg(mStr.right(2))
                          .arg(sStr.right(2));
        }

        ui->labelCurrentTime->setText(curTime);
    }
}

void PlayerForm::closeEvent(QCloseEvent *event) {
    qDebug() << "PlayerForm::closeEvent";
}

void PlayerForm::loadIcons() {
    iconHelper.reset(new IconHelper(":/font/fa-regular-400.ttf",
                                    "Font Awesome 6 Pro Regular"));

    // QImage img = iconHelper->getPixmap1("#F08784", 0x25b6, 600, 600,
    // 600).toImage();
    QPixmap pix1 = iconHelper->getPixmap1("#F08784", 0x25b6, 600, 600, 600);
    // img.save("play.png");
    QPixmap pix2 = iconHelper->getPixmap1("#308704", 0x25b6, 600, 600, 600);

    QIcon icon;
    icon.addPixmap(pix1, QIcon::Active);
    icon.addPixmap(pix2, QIcon::Normal);

    ui->pushButtonPlay->setIcon(icon);
    ui->pushButtonPlay->setIconSize(QSize(42, 42));
    // ui->pushButtonPlay->setStyleSheet("QPushButton#pushButtonPlay:hover{"
    //                                   "border:1px solid transparent;"
    //                                   "}");
    // ui->pushButtonPlay->
}
