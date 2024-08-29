#include "playerform.h"
#include "openmediadialog.h"
#include "ui_playerform.h"

#include <QFile>
#include <QFileInfo>
#include <QMediaDevices>
#include <QStyle>
#include <QTimer>

#include <QGuiApplication>
#include <QScreen>

namespace qtav {

static int AUDIO_SAMPLING_RATES[] = {
    96000, // 0
    88200, // 1
    64000, // 2
    48000, // 3
    44100, // 4
    32000, // 5
    24000, // 6
    22050, // 7
    16000, // 8
    12000, // 9
    11025, // 10
    8000,  // 11
    7350,  // 12
    -1,    // 13
    -1,    // 14
    -1,    // 15
};

bool IsValidSampleRate(int rate) {
    if (rate < 0)
        return true;

    for (int i = 0;
         i < sizeof(AUDIO_SAMPLING_RATES) / sizeof(AUDIO_SAMPLING_RATES[0]);
         ++i) {
        if (rate == AUDIO_SAMPLING_RATES[i]) {
            return true;
        }
    }

    return false;
}

/*
    enum SampleFormat : quint16 {
        Unknown,
        UInt8,
        Int16,
        Int32,
        Float,
        NSampleFormats
    };

enum AVSampleFormat {
AV_SAMPLE_FMT_NONE = -1,
AV_SAMPLE_FMT_U8,          ///< unsigned 8 bits
AV_SAMPLE_FMT_S16,         ///< signed 16 bits
AV_SAMPLE_FMT_S32,         ///< signed 32 bits
AV_SAMPLE_FMT_FLT,         ///< float
AV_SAMPLE_FMT_DBL,         ///< double

 AV_SAMPLE_FMT_U8P,         ///< unsigned 8 bits, planar
 AV_SAMPLE_FMT_S16P,        ///< signed 16 bits, planar
 AV_SAMPLE_FMT_S32P,        ///< signed 32 bits, planar
 AV_SAMPLE_FMT_FLTP,        ///< float, planar
 AV_SAMPLE_FMT_DBLP,        ///< double, planar
 AV_SAMPLE_FMT_S64,         ///< signed 64 bits
 AV_SAMPLE_FMT_S64P,        ///< signed 64 bits, planar

 AV_SAMPLE_FMT_NB           ///< Number of sample formats. DO NOT USE if linking
dynamically
};
 */

QAudioFormat::SampleFormat AvSampleFormatToQtSampleFormat(AVSampleFormat fmt) {
    QAudioFormat::SampleFormat qt_fmt = QAudioFormat::Unknown;
    switch (fmt) {
    case AV_SAMPLE_FMT_U8:
        qt_fmt = QAudioFormat::UInt8;
        break;
    case AV_SAMPLE_FMT_S16:
        qt_fmt = QAudioFormat::Int16; // 输出的采样格式。绝⼤部分声卡⽀持
        break;
    case AV_SAMPLE_FMT_S32:
        qt_fmt = QAudioFormat::Int32;
        break;
    case AV_SAMPLE_FMT_FLT:
        qt_fmt = QAudioFormat::Float;
        break;
    default:
        qt_fmt = QAudioFormat::Unknown;
        break;
    }

    return qt_fmt;
}

AVSampleFormat QtSampleFormatToAvSampleFormat(QAudioFormat::SampleFormat fmt) {
    AVSampleFormat av_fmt = AV_SAMPLE_FMT_NONE;
    switch (fmt) {
    case QAudioFormat::UInt8:
        av_fmt = AV_SAMPLE_FMT_U8;
        break;
    case QAudioFormat::Int16:
        av_fmt = AV_SAMPLE_FMT_S16; // 输出的采样格式。绝⼤部分声卡⽀持
        break;
    case QAudioFormat::Int32:
        av_fmt = AV_SAMPLE_FMT_S32;
        break;
    case QAudioFormat::Float:
        av_fmt = AV_SAMPLE_FMT_FLT;
        break;
    default:
        av_fmt = AV_SAMPLE_FMT_NONE;
        break;
    }

    return av_fmt;
}

bool InSampleFormatList(QAudioFormat::SampleFormat fmt,
                        const QList<QAudioFormat::SampleFormat> &lst) {
    for (auto &f : lst) {
        if (f == fmt)
            return true;
    }

    return false;
}

} // namespace qtav

PlayerForm::PlayerForm(QWidget *parent)
    : QWidget(parent), ui(new Ui::PlayerForm) {
    ui->setupUi(this);
    ui->listWidgetFiles->hide();

    volumeSlider = new VolumeSlider(Qt::Vertical, this);
    volumeSlider->setFixedSize(20, 100);
    volumeSlider->setRange(0, 100);
    volumeSlider->setVisible(false);
    volumeSlider->setValue(50);
    connect(volumeSlider, &VolumeSlider::valueChanged, this,
            &PlayerForm::slotVolumeChanged);

    timerProgress = new QTimer(this); // 定时器-获取当前视频时间
    connect(timerProgress, &QTimer::timeout, this,
            &PlayerForm::slotTimerTimeout);
    timerProgress->setInterval(500);

    initContext();

    // 使用队列模式，保证即使是UI线程触发playDone信号，也能按顺序最后到达，重置控件
    connect(this, &PlayerForm::playDone, this, &PlayerForm::slotPlayDone,
            Qt::QueuedConnection);
    connect(ui->horSliderProgress, &ProgressSlider::sliderChanged, this,
            &PlayerForm::slotProgressChanged);

    qDebug() << "PlayerForm" << QThread::currentThreadId();
}

PlayerForm::~PlayerForm() {
    qDebug() << "PlayerForm::~PlayerForm() ";

    stopPlayer();
    stopSpeaker();

    qDebug() << "PlayerForm::~PlayerForm() done ";

    delete ui;
}

bool PlayerForm::NegotiateAudioFormat(const AudioFormat *in, AudioFormat *out) {
    // 用于协商音频格式与音频设备。
    // return 是否需要重采样。

    if (!in || !out)
        return false;

    bool resample = false;

    // 回调这个函数说明有音频, 就创建音频设备
    QAudioDevice audioDevice = AudioSpeaker::getDevice();

    QAudioFormat preferFmt = audioDevice.preferredFormat();

    QAudioFormat::SampleFormat selectSmplFmt = QAudioFormat::Unknown;
    QAudioFormat::SampleFormat smplFmt =
        qtav::AvSampleFormatToQtSampleFormat((AVSampleFormat)in->sample_fmt);
    if (smplFmt ==
        QAudioFormat::Unknown) { // 不认识的格式，例如AV_SAMPLE_FMT_FLTP.
                                 // 需要重采样
        resample = true;
        selectSmplFmt = preferFmt.sampleFormat();
    } else {
        auto supportedFmts = audioDevice.supportedSampleFormats();
        if (qtav::InSampleFormatList(smplFmt, supportedFmts)) {
            selectSmplFmt = smplFmt;
        } else {
            resample = true;
            selectSmplFmt = preferFmt.sampleFormat();
        }
    }
    out->sample_fmt = qtav::QtSampleFormatToAvSampleFormat(selectSmplFmt);

    // sample rate
    if (!qtav::IsValidSampleRate(in->sample_rate)) {
        out->sample_rate = preferFmt.sampleRate();
        resample = true;
    } else if (in->sample_rate >= audioDevice.minimumSampleRate() &&
               in->sample_rate <= audioDevice.maximumSampleRate()) {
        out->sample_rate = in->sample_rate;
    } else {
        out->sample_rate = preferFmt.sampleRate();
        resample = true;
    }

    // channel count
    if (in->channel_count >= audioDevice.minimumChannelCount() &&
        in->channel_count <= audioDevice.maximumChannelCount()) {
        out->channel_count = in->channel_count;
    } else {
        out->channel_count = preferFmt.channelCount();
        resample = true;
    }

    // hardcode
    // out->sample_fmt =
    //     qtav::QtSampleFormatToAvSampleFormat(QAudioFormat::SampleFormat::Int16);
    // resample = true;

    qDebug() << "NegotiateAudioFormat resample" << resample << ", sample fmt"
             << qtav::AvSampleFormatToQtSampleFormat(
                    (AVSampleFormat)out->sample_fmt)
             << ", sample rate" << out->sample_rate << ", channel"
             << out->channel_count;

    // speaker 前面应该创建
    if (speaker) {
        QAudioFormat applyFmt;
        applyFmt.setSampleFormat(qtav::AvSampleFormatToQtSampleFormat(
            (AVSampleFormat)out->sample_fmt));
        applyFmt.setSampleRate(out->sample_rate);
        applyFmt.setChannelCount(out->channel_count);

        qDebug() << "audio device" << audioDevice.description() << ", format"
                 << applyFmt;

        speaker->start(audioDevice, applyFmt);
        // start之后才有效
        speaker->setVolume(volumeSlider->value());
        qDebug() << QThread::currentThreadId() << "start speaker";
    }

    return resample;
}

bool PlayerForm::openMedia(MediaSource media) {
    stopPlayer();
    stopSpeaker();

    player.reset(new FFPlayer);
    player->SetMediaSource(media);
    if (!player->Open()) {
        qDebug() << "打开失败";
        return false;
    }

    qDebug() << "total seconds " << player->GetTotalSeconds();
    onTotalSeconds(player->GetTotalSeconds());

    if (player->HasAudio()) {
        AudioFormat audioFmt;
        if (!player->GetAudioFormat(&audioFmt)) {
            qDebug() << "获取音频参数失败";
            return false;
        }

        speaker.reset(new AudioSpeaker(true));

        player->SetAudioFrameCallback(
            std::bind(&PlayerForm::playAudio, this, std::placeholders::_1,
                      std::placeholders::_2, std::placeholders::_3));
        player->SetNegotiateAudioFormatCallback(
            std::bind(&PlayerForm::NegotiateAudioFormat, this,
                      std::placeholders::_1, std::placeholders::_2));
        player->SetAudioClockCallback([=]() { return speaker->audioClock(); });
    }

    player->SetVideoFrameCallback(std::bind(&PlayerForm::playVideo, this,
                                            std::placeholders::_1,
                                            std::placeholders::_2));

    player->SetPlayDoneCallback([=]() {
        // 该函数在播放器内部线程中执行，不能直接reset，需要借助信号槽处理
        qDebug() << QThread::currentThreadId() << "emit playDone";
        emit this->playDone();
    });

    timerProgress->start();

    if (player->Play()) {
        qDebug() << "播放成功";
        ui->pushButtonPlay->setChecked(true);

        if (windowTitleCb) {
            if (media.type == MediaType::kMediaFile) {
                QString title = QFileInfo(media.src.data()).fileName();
                windowTitleCb(title);
            } else {
                windowTitleCb(media.src.data());
            }
        }

        return true;
    } else {
        qDebug() << "播放失败";
        return false;
    }
}

void PlayerForm::playAudio(const char *data, int size, double clock) {
    if (!speaker)
        return;

    qDebug() << QThread::currentThreadId() << util::TimeMilliseconds()
             << "playAudio callback" << clock << ", size" << size;

    std::shared_ptr<std::string> audio_data(new std::string(data, size));
    speaker->write(audio_data, clock);
}

void PlayerForm::playVideo(AVFrame *frame, double clock) {
    if (!ui->openGLWidget->isSupportedFormat(frame->format)) {
        return;
    }

    QSharedPointer<VideoFrame> videoFrame;

    if (AV_PIX_FMT_YUV420P == frame->format ||
        AV_PIX_FMT_YUVJ420P == frame->format) {

        videoFrame.reset(
            new VideoFrame(frame->format, frame->width, frame->height));

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

    } else if (AV_PIX_FMT_NV12 == frame->format) {
#if 0
        videoFrame.reset(
            new VideoFrame(frame->format, frame->width, frame->height));

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
#else
        int width = frame->linesize[0];

        videoFrame.reset(new VideoFrame(frame->format, width, frame->height));

        for (int i = 0; i < frame->height; i++) {
            memcpy(videoFrame->data[0] + i * width,
                   frame->data[0] + i * frame->linesize[0],
                   width); // 按行复制数据，末尾有对齐数据
        }

        for (int i = 0; i < frame->height / 2; i++) {
            memcpy(videoFrame->data[1] + i * width,
                   frame->data[1] + i * frame->linesize[1],
                   width); // UV数据在一起
        }
#endif
    }

    // qDebug() << QThread::currentThreadId() << "playVideo emit playFrame";
    emit ui->openGLWidget->playFrame(videoFrame);
}

void PlayerForm::openDialog() {
    OpenMediaDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        qDebug() << "open media " << (int)dlg.mediaSource.type << ", "
                 << dlg.mediaSource.src;
        openMedia(dlg.mediaSource);
    } else {
        ui->pushButtonPlay->setChecked((bool)player);
    }
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
    ui->horSliderProgress->setRange(0, sec);

    QString totalTime = formatTimestamp(sec, false);
    ui->labelTotalTime->setText(totalTime);
}

void PlayerForm::stopPlayer() {
    if (player) {
        qDebug() << QThread::currentThreadId() << "reset player ...";

        // 如果有正在执行的播放器，PlayDoneCallback的延迟执行可能会释放掉新的播放器
        player->SetPlayDoneCallback(nullptr);

        if (player->IsPlaying()) {
            player->Stop();
        }
        player.reset();
        qDebug() << QThread::currentThreadId() << "reset player done";
    }
}

void PlayerForm::stopSpeaker() {
    if (speaker) {
        qDebug() << QThread::currentThreadId() << "reset speaker ...";
        speaker->stop();
        speaker.reset();
    }
}

void PlayerForm::pauseSpeaker() {
    if (speaker) {
        speaker->pause();
    }
}

void PlayerForm::resumeSpeaker() {
    if (speaker) {
        speaker->resume();
    }
}

void PlayerForm::initContext() {
    if (windowTitleCb) {
        windowTitleCb("");
    }

    timerProgress->stop();

    clearTimestamp();
    ui->horSliderProgress->setValue(0);
    ui->pushButtonPlay->setChecked(false);
}

void PlayerForm::clearContext() {
    initContext();

    ui->openGLWidget->clear();
}

void PlayerForm::setVolumeIcon(bool mute) {
    ui->pushButtonVolume->setChecked(mute);
}

QString PlayerForm::formatTimestamp(int seconds, bool longFmt) {
    if (seconds < 0) {
        if (longFmt) {
            return QString("--:--:--");
        } else {
            return QString("--:--");
        }
    }

    QString fmtStr;
    QString hStr = QString("0%1").arg(seconds / 3600);
    QString mStr = QString("0%1").arg(seconds / 60 % 60);
    QString sStr = QString("0%1").arg(seconds % 60);
    if (!longFmt && hStr == "00") {
        fmtStr = QString("%1:%2").arg(mStr.right(2), sStr.right(2));
    } else {
        fmtStr = QString("%1:%2:%3").arg(hStr, mStr.right(2), sStr.right(2));
    }

    return fmtStr;
}

void PlayerForm::clearTimestamp() {
    QString nullTs = formatTimestamp(-1, false);
    ui->labelCurrentTime->setText(nullTs);
    ui->labelTotalTime->setText(nullTs);
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

void PlayerForm::slotPlayDone() {
    qDebug() << QThread::currentThreadId() << "slotPlayDone";
    player.reset();
    speaker.reset();

    clearContext();
}

void PlayerForm::slotTimerTimeout() {
    if (QObject::sender() == timerProgress) {
        if (!player)
            return;

        int sec = player->GetClock();
        if (!progressSliderPressed) {
            ui->horSliderProgress->setValue(sec);
        }

        QString currentTime = formatTimestamp(sec, false);
        ui->labelCurrentTime->setText(currentTime);
    }
}

void PlayerForm::slotVolumeChanged(int value) {
    if (speaker) {
        speaker->setVolume(value);
    }

    setVolumeIcon(value <= 0);
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
}

void PlayerForm::on_pushButtonPlay_clicked(bool checked) {
    if (!player) {
        openDialog();
        return;
    }

    if (!checked) {
        player->Pause();
        pauseSpeaker();
    } else {
        player->Play();
        resumeSpeaker();
    }
}

void PlayerForm::on_pushButtonStop_clicked() {
    stopPlayer();
    stopSpeaker();

    qDebug() << QThread::currentThreadId() << "emit playDone";
    emit playDone();
}

void PlayerForm::on_pushButtonVolume_clicked(bool checked) {
    qDebug() << "pushButtonVolume checked" << checked;

    // 设吹按钮样式
    ui->pushButtonVolume->setChecked(volumeSlider->value() == 0);

    QPushButton *btn = ui->pushButtonVolume;
    if (volumeSlider->orientation() == Qt::Vertical) {
        volumeSlider->move(btn->x(), this->height() - btn->y() -
                                         volumeSlider->height() - 10);
    } else {
        volumeSlider->move(btn->x() - volumeSlider->width() + 5,
                           this->height() - btn->y() - 17);
    }
    volumeSlider->setVisible(true);
    volumeSlider->setFocus();
}

void PlayerForm::mousePressEvent(QMouseEvent *event) {
    // 获取焦点，隐藏音量控件
    this->setFocus();
    QWidget::mousePressEvent(event);
}

void PlayerForm::slotProgressChanged(int value) {
    qDebug() << "slotProgressChanged to" << value;

    if (!player)
        return;

    player->Seek(value);
}

void PlayerForm::on_horSliderProgress_sliderReleased() {
    // 点击或拖动都会触发 释放信号
    qDebug() << "on_horSliderProgress_sliderReleased";

    // 这里不能直接使用value，其值为按下时候的值，不是释放的值。

    if (progressSliderPressed) {
        if (progressSliderValue >= 0) { // 有效拖动
            qDebug() << "sliderReleased emit sliderChanged"
                     << progressSliderValue;
            emit ui->horSliderProgress->sliderChanged(progressSliderValue);
        }
        progressSliderPressed = false;
    }
}

void PlayerForm::on_horSliderProgress_sliderPressed() {
    qDebug() << "on_horSliderProgress_sliderPressed";
    progressSliderPressed = true;
    progressSliderValue = -1; // 重置
}

void PlayerForm::on_horSliderProgress_sliderMoved(int position) {
    qDebug() << "on_horSliderProgress_sliderMoved" << position;
    progressSliderValue = position; // 更新
}

void PlayerForm::on_horSliderProgress_valueChanged(int value) {
    qDebug() << "on_horSliderProgress_valueChanged" << value;
}
