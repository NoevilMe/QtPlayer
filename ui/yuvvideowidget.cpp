#include "yuvvideowidget.h"
#include "util/util.h"

#include <QTimer>

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
}

YuvVideoWidget::YuvVideoWidget(QWidget *parent)
    : QOpenGLWidget{parent},
      width_(0),
      height_(0),
      posVbo_(0),
      textVbo_(0),
      vao_(0),
      program(nullptr),
      pixFormat_(AV_PIX_FMT_NONE),
      bufYuv420p_(nullptr),
      yuvFile_(nullptr) {}

YuvVideoWidget::~YuvVideoWidget() { resetTextData(); }

void YuvVideoWidget::resetTextData() {
    if (textData_[0]) {
        delete[] textData_[0];
        textData_[0] = nullptr;
    }

    if (textData_[1]) {
        delete[] textData_[1];
        textData_[1] = nullptr;
    }
    if (textData_[2]) {
        delete[] textData_[2];
        textData_[2] = nullptr;
    }
}

void YuvVideoWidget::init(int width, int height) {
    // 分配材质内存空间
    textData_[0] = new unsigned char[width * height]; // Y
    textData_[1] = new unsigned char[width * height / 2]; // U. NV12占用会大一些
    textData_[2] = new unsigned char[width * height / 2]; // V

    width_ = width;
    height_ = height;

    videoRatio_ = (float)width / height;
}

void YuvVideoWidget::resetVideoSize(int width, int height) {
    if (width_ == width && height_ == height) {
        return;
    }

    resetTextData();

    // 分配材质内存空间
    textData_[0] = new unsigned char[width * height]; // Y
    textData_[1] = new unsigned char[width * height / 2]; // U，NV12会将UV放这里
    textData_[2] = new unsigned char[width * height / 2]; // V

    width_ = width;
    height_ = height;

    videoRatio_ = (float)width / height;
}

void YuvVideoWidget::paintFrame(unsigned char *buf) {
    if (!buf)
        return;

    memcpy(textData_[0], buf, width_ * height_);
    memcpy(textData_[1], buf + width_ * height_, width_ * height_ / 4);
    memcpy(textData_[2], buf + width_ * height_ * 5 / 4, width_ * height_ / 4);

    // 刷新显示
    update();
}

void YuvVideoWidget::paintAVFrame(AVFrame *frame) {
    if (!frame)
        return;

    if ((AVPixelFormat)frame->format != AV_PIX_FMT_YUV420P &&
        (AVPixelFormat)frame->format != AV_PIX_FMT_NV12 &&
        (AVPixelFormat)frame->format != AV_PIX_FMT_YUVJ420P) {
        qDebug() << "unsupported frame " << frame->format;
        return;
    }

    // if (AV_PIX_FMT_YUVJ420P == frame->format) {
    //     std::string filename =
    //         std::to_string(util::TimeMilliseconds()) + ".yuv";
    //     QFile file_(filename.data());
    //     file_.open(QIODevice::WriteOnly);

    //     // for (int i = 0; i < frame->height; i++) {
    //     //     file_.write((char *)(frame->data[0] + i * frame->linesize[0]),
    //     //                 frame->width);
    //     // }

    //     // for (int i = 0; i < frame->height / 2; i++) {
    //     //     file_.write((char *)(frame->data[1] + i * frame->linesize[1]),
    //     //                 frame->width);
    //     // }

    //     // file_.write((char *)frame->data[0], frame->linesize[0] *
    //     frame->height);
    //     // file_.write((char *)frame->data[1], frame->linesize[1] *
    //     frame->height / 2);
    //     // file_.write((char *)frame->data[2], frame->linesize[2] *
    //     frame->height / 2);

    //     // file_.write((char *)frame->data[0], frame->linesize[0] *
    //     frame->height);
    //     // file_.write((char *)frame->data[1], frame->linesize[1] *
    //     frame->height / 2); file_.flush();
    // }

    // https://blog.csdn.net/chinabinlang/article/details/7804808
    resetVideoSize(frame->linesize[0], frame->height);
    // resetVideoSize(frame->width, frame->height);

    // qDebug() << "width " << frame->width << ", height " << frame->height
    //          << ", line size " << frame->linesize[0];

    pixFormat_ = frame->format;
    if (AV_PIX_FMT_YUV420P == pixFormat_ || AV_PIX_FMT_YUVJ420P == pixFormat_) {
        memcpy(textData_[0], frame->data[0],
               frame->linesize[0] * frame->height);
        memcpy(textData_[1], frame->data[1],
               frame->linesize[1] * frame->height / 2);
        memcpy(textData_[2], frame->data[2],
               frame->linesize[2] * frame->height / 2);
    } else if (AV_PIX_FMT_NV12 == pixFormat_) {
        memcpy(textData_[0], frame->data[0],
               frame->linesize[0] * frame->height);
        memcpy(textData_[1], frame->data[1],
               frame->linesize[1] * frame->height / 2);
    }

    //    for (int i = 0; i < frame->height; i++) {
    //        memcpy(textData_[0] + i * frame->width,
    //               frame->data[0] + i * frame->linesize[0], frame->width);
    //    }

    //    for (int i = 0; i < frame->height / 2; i++) {
    //        memcpy(textData_[1] + i * frame->width / 2,
    //               frame->data[1] + i * frame->linesize[1], frame->width / 2);
    //    }

    //    for (int i = 0; i < frame->height / 2; i++) {
    //        memcpy(textData_[2] + i * frame->width / 2,
    //               frame->data[2] + i * frame->linesize[2], frame->width / 2);
    //    }

    update();
}

void YuvVideoWidget::PlayOneFrame() {
    // 函数功能读取一张yuv图像数据进行显示，每进入一次，就显示一张图片
    if (NULL == yuvFile_) {
        // 打开yuv视频文件 注意修改文件路径
        // 可以自行将fopen改为QFile中最新的文件操作接口
        //        yuvFile_ = fopen("D:\\work\\yuv_1280x720_i420.yuv", "rb");
        yuvFile_ = fopen("D:\\dev\\TestDemo\\yuv_1280x720_i420.yuv", "rb");
    }

    // 申请内存存一帧yuv图像数据，其大小为分辨率的1.5倍
    int nLen = width_ * height_ * 3 / 2;
    if (NULL == bufYuv420p_) {
        bufYuv420p_ = new unsigned char[nLen];
        qDebug("CPlayWidget::PlayOneFrame new data memory. Len=%d width=%d "
               "height=%d\n",
               nLen, width_, height_);
    }

    // 将一帧yuv图像读到内存中
    if (NULL == yuvFile_) {
        qFatal("read yuv file err.may be path is wrong!\n");
        return;
    }

    // 读一帧数据
    if (fread(bufYuv420p_, 1, nLen, yuvFile_) != nLen) {
        // 关闭文件，并准备重新循环打开播放
        fclose(yuvFile_);
        yuvFile_ = NULL;
    } else {
        memcpy(textData_[0], bufYuv420p_, width_ * height_);
        memcpy(textData_[1], bufYuv420p_ + width_ * height_,
               width_ * height_ / 4);
        memcpy(textData_[2], bufYuv420p_ + width_ * height_ * 5 / 4,
               width_ * height_ / 4);
    }

    // 刷新界面,触发paintGL接口
    update();
}

void YuvVideoWidget::initializeGL() {
    qDebug() << "initializeGL";

    initializeOpenGLFunctions();
    glEnable(GL_DEPTH_TEST);

    initShader();
    initVBO();
    initVAO();

    initTextures();

    glClearColor(0.0, 0.0, 0.0, 1.0);

    // 打开本地文件、启动定时器
    // QTimer *ti = new QTimer(this);
    // connect(ti, SIGNAL(timeout()), this, SLOT(PlayOneFrame()));
    // ti->start(40);
}

void YuvVideoWidget::resizeGL(int w, int h) {
    // 防止被零除
    if (h == 0) {
        h = 1; // 将高设为1
    }

    // 设置视口
    glViewport(0, 0, w, h);

    auto winRatio = (float)w / h;
    trans_.setToIdentity();
    if (winRatio > videoRatio_) {
        trans_.scale(videoRatio_ / winRatio, 1.0f, 1.0f);
    } else {
        // trans_.scale(1.0f, 1 - ((videoRatio_ - winRatio) / 2), 1.0f);
        trans_.scale(1.0f, winRatio / videoRatio_, 1.0f);
    }

    //    qDebug() << "resizeGL " << w << " x " << h;
}

void YuvVideoWidget::paintGL() {
    //    qDebug() << "paintGL " << this->rect();

    //#ifndef GL_SAMPLE
    //    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    //    glClear(GL_COLOR_BUFFER_BIT);

    program->bind();

    //    QMatrix4x4 mat4; //默认是单位矩阵
    //    mat4.scale(0.5); //缩放
    //    // mat4.translate(0.3, 0.3, 0.0);
    //    //    mat4.rotate(45.0f, QVector3D(0.0, 0.0, 1.0));
    //    program->setUniformValue("trans", mat4);
    program->setUniformValue("trans", trans_);
    if (pixFormat_ >= 0) {
        program->setUniformValue("pixFormat", pixFormat_);
    }

    glBindVertexArray(vao_);
    drawTextures();

    // GL_TRIANGLE_STRIP：有两种情况，
    //（1）当前顶点序号n是偶数时，三角形三个顶点的顺序是(n - 2, n - 1, n )。
    //（2）当前顶点序号n是奇数时，三角形三个顶点的顺序是(n - 1, n - 2, n)。
    //    这两种情况，保证了采用此种渲染方式的三角形顶点的卷绕顺序。
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glBindVertexArray(0);

    program->release();
}

void YuvVideoWidget::initVBO() {
    // 传递顶点和材质坐标
    // 顶点
    // OpenGL的顶点坐标是X和Y取值[-1,
    // 1]。如果不到这个范围，则窗口的其他区域按比例显示为背景色？
    // 所以调整这个坐标可以做到铺满窗口或者留空
    //    static const GLfloat vert[] = {-0.5f, -0.5f, 0.5f, -0.5f,
    //                                   -0.5f, 0.5f,  0.5f, 0.5f};
    static const GLfloat vert[] = {-1.0f, -1.0f, 1.0f, -1.0f,
                                   -1.0f, 1.0f,  1.0f, 1.0f};

    // 纹理坐标
    static const GLfloat text[] = {0.0f, 1.0f, 1.0f, 1.0f,
                                   0.0f, 0.0f, 1.0f, 0.0f};
    //    static const GLfloat text[] = {0.0f, 0.5f, 0.5f, 0.5f,
    //                                   0.0f, 0.0f, 0.5f, 0.0f};

    glGenBuffers(1, &posVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, posVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vert), vert, GL_STATIC_DRAW);

    glGenBuffers(1, &textVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, textVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(text), text, GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void YuvVideoWidget::initVAO() {
    //     VAO创建
    glGenVertexArrays(1, &vao_);
    glBindVertexArray(vao_);

    //绑定vbo ebo 加入属性描述信息
    //.1 加入位置属性描述信息
    glBindBuffer(GL_ARRAY_BUFFER, posVbo_);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2,
                          (void *)0);
    glEnableVertexAttribArray(0);

    //.2 加入材质属性描述数据
    glBindBuffer(GL_ARRAY_BUFFER, textVbo_);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2,
                          (void *)0);
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindVertexArray(0);
}

void YuvVideoWidget::initTextures() {
    // Y,U,V各一个
    glGenTextures(3, textYUV_);
    for (int i = 0; i < 3; ++i) {
        //--绑定纹理对象--
        glBindTexture(GL_TEXTURE_2D, textYUV_[i]);
        // 放大过滤，线性插值   GL_NEAREST(效率高，但马赛克严重)
        // 设置纹理的过滤方式
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        // 设置纹理的包裹方式
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
}

void YuvVideoWidget::drawTextures() {
    // 默认0-15 纹理单元可用
    // qDebug() << "pix fmt " << pixFormat_;

    if (pixFormat_ == AV_PIX_FMT_YUV420P) {

        // 加载y数据纹理
        // 激活纹理单元GL_TEXTURE0
        glActiveTexture(GL_TEXTURE0);

        // 使用来自y数据生成纹理
        glBindTexture(GL_TEXTURE_2D, textYUV_[0]);

        // 使用内存中m_pBufYuv420p数据创建真正的y数据纹理
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, width_, height_, 0,
                     GL_LUMINANCE, GL_UNSIGNED_BYTE, textData_[0]);

        // 加载u数据纹理
        glActiveTexture(GL_TEXTURE1); // 激活纹理单元GL_TEXTURE1
        glBindTexture(GL_TEXTURE_2D, textYUV_[1]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, width_ / 2, height_ / 2, 0,
                     GL_LUMINANCE, GL_UNSIGNED_BYTE, (char *)textData_[1]);

        // 加载v数据纹理
        glActiveTexture(GL_TEXTURE2); // 激活纹理单元GL_TEXTURE2
        glBindTexture(GL_TEXTURE_2D, textYUV_[2]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, width_ / 2, height_ / 2, 0,
                     GL_LUMINANCE, GL_UNSIGNED_BYTE, (char *)textData_[2]);
    } else if (pixFormat_ == AV_PIX_FMT_NV12) {
        // glUniform1i(uniformFmt, pixFormat_);

        // 加载y数据纹理
        // 激活纹理单元GL_TEXTURE0
        glActiveTexture(GL_TEXTURE0);

        // 使用来自y数据生成纹理
        glBindTexture(GL_TEXTURE_2D, textYUV_[0]);

        // 使用内存中m_pBufYuv420p数据创建真正的y数据纹理
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, width_, height_, 0,
                     GL_LUMINANCE, GL_UNSIGNED_BYTE, textData_[0]);

        // 加载u数据纹理
        glActiveTexture(GL_TEXTURE1); // 激活纹理单元GL_TEXTURE1
        glBindTexture(GL_TEXTURE_2D, textYUV_[1]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA, width_ / 2,
                     height_ / 2, 0, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE,
                     (char *)textData_[1]);
    }
}

void YuvVideoWidget::initShader() {
    // 初始化opengl （QOpenGLFunctions继承）函数
    //    initializeOpenGLFunctions();

    // https://blog.csdn.net/zhangpengzp/article/details/89532590

    // 顶点着色器源码
    const char *vsrc = R"(#version 330 core
        layout(location = 0) in vec4 vertexIn;
        layout(location = 1) in vec2 textureIn;
        out vec2 texturePos;

        //矩阵必须初始化，初始化单位矩阵，否则GLSL语言中默认矩阵是0矩阵
        uniform mat4 trans = mat4(1.0);

        void main(void)
    {
            gl_Position = trans * vertexIn;
            texturePos = textureIn;
    })";

    // 片段着色器源码
    const char *fsrc = R"(#version 330 core
        in vec2 texturePos;
        uniform sampler2D yTexture;
        uniform sampler2D uTexture;
        uniform sampler2D vTexture;
        uniform int pixFormat = 0;
        void main(void)
        {
            vec3 yuv;
            vec3 rgb;
            if(pixFormat==0) // AV_PIX_FMT_YUV420P
            {
                yuv.r = texture2D(yTexture, texturePos).r;
                yuv.g = texture2D(uTexture, texturePos).r - 0.5;
                yuv.b = texture2D(vTexture, texturePos).r - 0.5;
            }else if(pixFormat==12) //AV_PIX_FMT_YUVJ420P
            {
                yuv.r = texture2D(yTexture, texturePos).r;
                yuv.g = texture2D(uTexture, texturePos).r - 0.5;
                yuv.b = texture2D(vTexture, texturePos).r - 0.5;
            }else if(pixFormat==23) //AV_PIX_FMT_NV12
            {
                yuv.r = texture2D(yTexture, texturePos).r;
                yuv.g = texture2D(uTexture, texturePos).r - 0.5 ;
                // shader 会将数据归一化，而 uv 的取值区间本身存在-128到正128 然后归一化到0-1 为了正确计算成rgb，
                // 则需要归一化到 -0.5 - 0.5的区间
                yuv.b = texture2D(uTexture, texturePos).a - 0.5;
            }else if(pixFormat==24)//AV_PIX_FMT_NV21
            {
                yuv.r = texture2D(yTexture, texturePos).r;
                yuv.g = texture2D(uTexture, texturePos).a - 0.5 ;
                // shader 会将数据归一化，而 uv 的取值区间本身存在-128到正128 然后归一化到0-1 为了正确计算成rgb，
                // 则需要归一化到 -0.5 - 0.5的区间
                yuv.b = texture2D(uTexture, texturePos).r - 0.5;
            }

            rgb = mat3( 1,       1,         1,
                   0,       -0.39465,  2.03211,
                   1.13983, -0.58060,  0) * yuv;
            gl_FragColor = vec4(rgb, 1);
        })";

    program = new QOpenGLShaderProgram(this);
    // program加载shader（顶点和片元）脚本
    // 顶点shader
    program->addShaderFromSourceCode(QOpenGLShader::Vertex, vsrc);
    // 片元（像素）
    program->addShaderFromSourceCode(QOpenGLShader::Fragment, fsrc);

    // GLSL中已经设置location, 不用再绑定位置
    // 设置顶点坐标的变量
    //    program.bindAttributeLocation("aPosition", 0);
    // 设置材质坐标
    //    program.bindAttributeLocation("aTexCoord", 1);

    // 编译shader
    program->link();
    program->bind();

    // 从shader获取材质
    uniformYUV_[0] = program->uniformLocation("yTexture");
    uniformYUV_[1] = program->uniformLocation("uTexture");
    uniformYUV_[2] = program->uniformLocation("vTexture");

    // 指定y纹理要使用新值
    // 只能用0,1,2等表示纹理单元的索引，这是opengl不人性化的地方
    // 0对应纹理单元GL_TEXTURE0 1对应纹理单元GL_TEXTURE1 2对应纹理的单元
    glUniform1i(uniformYUV_[0], 0);
    // 指定u纹理要使用新值
    glUniform1i(uniformYUV_[1], 1);
    // 指定v纹理要使用新值
    glUniform1i(uniformYUV_[2], 2);
}
