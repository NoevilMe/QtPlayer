#include "yuvvideowidget.h"
#include "av_def.h"

#include <QThread>
#include <QTimer>

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
}

YuvVideoWidget::YuvVideoWidget(QWidget *parent)
    : QOpenGLWidget{parent},
      formats{AV_PIX_FMT_YUV420P, AV_PIX_FMT_YUVJ420P, AV_PIX_FMT_NV12,
              AV_PIX_FMT_NV21},
      videoWidth(0),
      videoHeight(0),
      posVBO(0),
      textVBO(0),
      vao(0),
      program(nullptr),
      transInitialized(false) {

    // 使用信号槽跨线程传递
    connect(this, &YuvVideoWidget::playFrame, this,
            &YuvVideoWidget::slotPlayFrame);
}

YuvVideoWidget::~YuvVideoWidget() {
    releaseVAO();
    releaseVBO();
    releaseTextures();

    if (program) {
        program->deleteLater();
        delete program;
        program = nullptr;
    }
}

void YuvVideoWidget::adjustVideoSize(int width, int height) {
    if (videoWidth == width && videoHeight == height) {
        return;
    }

    videoWidth = width;
    videoHeight = height;

    if (videoHeight == 0) {
        videoHeight = 1;
    }

    videoRatio = (float)videoWidth / videoHeight;

    if (!transInitialized) {
        qDebug() << QThread::currentThreadId() << "calcTransMatrix";
        calcTransMatrix(this->width(), this->height());
        transInitialized = true;
    }
}

bool YuvVideoWidget::isSupportedFormat(int fmt) {
    return formats.contains(fmt);
}

void YuvVideoWidget::clear() {
    /*
0x258c playVideo
0x258c opengl playFrame
0x58ac reset player done
0x58ac stop speaker ...
0x58ac reset speaker ...
...
0x58ac playDoneSlot
YuvVideoWidget::clear()
0x58ac playVideoSlot
0x58ac paintGL  QRect(0,0 654x302)
0x58ac paintGL bind

信号槽机制会导致先前传递到UI的视频帧，可能会在videoFrame.reset()了之后才到达。
     */
    // qDebug() << QThread::currentThreadId() << "opengl clear()";
    videoFrame.reset();

    // 下次播放视频仍然需要重新计算变换矩阵
    transInitialized = false;

    // glClearColor(0.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    update();
}

void YuvVideoWidget::slotPlayFrame(const QSharedPointer<VideoFrame> &frame) {
    // qDebug() << QThread::currentThreadId() << "slotPlayFrame";

    videoFrame = frame;

    if (!videoFrame)
        return;

    if (!formats.contains(videoFrame->pixfmt)) {
        qDebug() << "unsupported frame " << videoFrame->pixfmt;
        return;
    }

    adjustVideoSize(frame->width, frame->height);

    update();
}

void YuvVideoWidget::paintAVFrame(AVFrame *frame) {
    if (!frame)
        return;

    if (!formats.contains(frame->format)) {
        qDebug() << "unsupported frame " << frame->format;
        return;
    }

    // https://blog.csdn.net/chinabinlang/article/details/7804808
    // resetVideoSize(frame->linesize[0], frame->height);
    adjustVideoSize(frame->width, frame->height);

    // qDebug() << "format" << frame->format << "width " << frame->width
    //          << ", height " << frame->height << ", line size0 "
    //          << frame->linesize[0] << ", line size1 " << frame->linesize[1];

    videoFrame.reset(
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

        // memcpy(textData_[0], frame->data[0],
        //        frame->linesize[0] * frame->height);
        // memcpy(textData_[1], frame->data[1],
        //        frame->linesize[1] * frame->height / 2);
        // memcpy(textData_[2], frame->data[2],
        //        frame->linesize[2] * frame->height / 2);
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

        // memcpy(videoFrame->data[0], frame->data[0],
        //        frame->linesize[0] * frame->height);
        // memcpy(videoFrame->data[1], frame->data[1],
        //        frame->linesize[1] * frame->height / 2);
    }

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
}

void YuvVideoWidget::resizeGL(int w, int h) {
    // 防止被零除
    if (h == 0) {
        h = 1; // 将高设为1
    }

    // 设置视口
    glViewport(0, 0, w, h);

    calcTransMatrix(w, h);
}

void YuvVideoWidget::paintGL() {
    // qDebug() << QThread::currentThreadId() << "paintGL " << this->rect();
    if (!videoFrame)
        return;

    // qDebug() << QThread::currentThreadId() << "paintGL bind";

    program->bind();
    program->setUniformValue("trans", trans);
    if (videoFrame->pixfmt >= 0) {
        program->setUniformValue("pixFormat", videoFrame->pixfmt);
    }

    glBindVertexArray(vao);
    drawTextures();

    // GL_TRIANGLE_STRIP：有两种情况，
    // （1）当前顶点序号n是偶数时，三角形三个顶点的顺序是(n - 2, n - 1, n )。
    // （2）当前顶点序号n是奇数时，三角形三个顶点的顺序是(n - 1, n - 2, n)。
    //    这两种情况，保证了采用此种渲染方式的三角形顶点的卷绕顺序。
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glBindVertexArray(0);
    program->release();
}

void YuvVideoWidget::calcTransMatrix(int w, int h) {
    if (h == 0) {
        h = 1;
    }

    auto winRatio = (float)w / h;
    trans.setToIdentity();
    if (winRatio > videoRatio) {
        trans.scale(videoRatio / winRatio, 1.0f, 1.0f);
    } else {
        trans.scale(1.0f, winRatio / videoRatio, 1.0f);
    }
}

void YuvVideoWidget::initShader() {
    // 初始化opengl （QOpenGLFunctions继承）函数
    //    initializeOpenGLFunctions();

    // https://blog.csdn.net/zhangpengzp/article/details/89532590

    // 顶点着色器源码
    // 为了设置顶点着色器的输出，我们必须把位置数据赋值给预定义的gl_Position变量
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
                // shader 会将数据归一化，而 uv 的取值区间本身存在-128到正128 然后归一化到0-1 为了正确计算成rgb，
                // 则需要归一化到 -0.5 - 0.5的区间
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
                yuv.g = texture2D(uTexture, texturePos).r - 0.5;
                yuv.b = texture2D(uTexture, texturePos).a - 0.5;
            }else if(pixFormat==24)//AV_PIX_FMT_NV21
            {
                yuv.r = texture2D(yTexture, texturePos).r;
                yuv.g = texture2D(uTexture, texturePos).a - 0.5;
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

#if 0
    // 从shader获取材质
    GLuint uniformYUV[3] = {0}; // fragment shader中yuv变量地址
    uniformYUV[0] = program->uniformLocation("yTexture");
    uniformYUV[1] = program->uniformLocation("uTexture");
    uniformYUV[2] = program->uniformLocation("vTexture");

    // 指定y纹理要使用新值
    // 只能用0,1,2等表示纹理单元的索引，这是opengl不人性化的地方
    // 0对应纹理单元GL_TEXTURE0 1对应纹理单元GL_TEXTURE1 2对应纹理的单元
    glUniform1i(uniformYUV[0], 0);
    // 指定u纹理要使用新值
    glUniform1i(uniformYUV[1], 1);
    // 指定v纹理要使用新值
    glUniform1i(uniformYUV[2], 2);
#else
    // 直接用Qt包装类
    program->setUniformValue("yTexture", 0);
    program->setUniformValue("uTexture", 1);
    program->setUniformValue("vTexture", 2);
#endif

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

    /*
     GL_ARRAY_BUFFER：用于存储顶点数组数据，如顶点位置、颜色、纹理坐标等，并通过glVertexAttribPointer()函数关联到特定的顶点属性。
     https://learnopengl-cn.github.io/01%20Getting%20started/04%20Hello%20Triangle/
     */

    glGenBuffers(1, &posVBO);
    glBindBuffer(GL_ARRAY_BUFFER, posVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vert), vert, GL_STATIC_DRAW);

    glGenBuffers(1, &textVBO);
    glBindBuffer(GL_ARRAY_BUFFER, textVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(text), text, GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void YuvVideoWidget::initVAO() {
    //     VAO创建
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // 绑定vbo ebo 加入属性描述信息
    //.1 加入位置属性描述信息
    // glVertexAttribPointer函数告诉OpenGL该如何解析顶点数据
    glBindBuffer(GL_ARRAY_BUFFER, posVBO);
    // 第一个参数指定我们要配置的顶点属性。与layout(location = 0)对应
    // 第二个参数指定顶点属性的大小。顶点属性是一个vec4，但是我们提供的数据由2个值组成，所以大小是2。
    // 第三个参数指定数据的类型，这里是GL_FLOAT(GLSL中vec*都是由浮点数值组成的)。
    // 第四个参数定义我们是否希望数据被标准化(Normalize)。如果我们设置为GL_TRUE，所有数据都会被映射到0（对于有符号型signed数据是-1）到1之间。我们把它设置为GL_FALSE。
    // 第五个参数叫做步长(Stride)，它告诉我们在连续的顶点属性组之间的间隔。
    // 由于下个组位置数据在2个float之后，我们把步长设置为2 * sizeof(float)。
    // 要注意的是由于我们知道这个数组是紧密排列的（在两个顶点属性之间没有空隙）我们也可以设置为0来让OpenGL决定具体步长是多少（只有当数值是紧密排列时才可用）。一旦我们有更多的顶点属性，我们就必须更小心地定义每个顶点属性之间的间隔，我们在后面会看到更多的例子（译注:
    // 这个参数的意思简单说就是从这个属性第二次出现的地方到整个数组0位置之间有多少字节）。
    // 最后一个参数的类型是void*，所以需要我们进行这个奇怪的强制类型转换。它表示位置数据在缓冲中起始位置的偏移量(Offset)。由于位置数据在数组的开头，所以这里是0。我们会在后面详细解释这个参数。
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2,
                          (void *)0);
    glEnableVertexAttribArray(0);

    //.2 加入材质属性描述数据
    glBindBuffer(GL_ARRAY_BUFFER, textVBO);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2,
                          (void *)0);
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindVertexArray(0);
}

void YuvVideoWidget::releaseVBO() {
    glDeleteBuffers(1, &posVBO);
    glDeleteBuffers(1, &textVBO);
}

void YuvVideoWidget::releaseVAO() { glDeleteVertexArrays(1, &vao); }

void YuvVideoWidget::releaseTextures() { glDeleteTextures(3, textYUV); }

void YuvVideoWidget::initTextures() {
    // Y,U,V各一个
    glGenTextures(3, textYUV);
    for (int i = 0; i < 3; ++i) {
        //--绑定纹理对象--
        glBindTexture(GL_TEXTURE_2D, textYUV[i]);
        // 字节对齐,网上很多代码都是少了这一步,导致有时候花屏
        // https://blog.csdn.net/feiyangqingyun/article/details/106985503
        //  glPixelStorei(GL_UNPACK_ROW_LENGTH, linesizeY);
        //  放大过滤，线性插值   GL_NEAREST(效率高，但马赛克严重)
        //  设置纹理的过滤方式
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
    QSharedPointer<VideoFrame> curFrame = videoFrame;
    if (!curFrame)
        return;

    if (curFrame->pixfmt == AV_PIX_FMT_YUV420P) {

        // 加载y数据纹理
        // 激活纹理单元GL_TEXTURE0
        glActiveTexture(GL_TEXTURE0);

        // 使用来自y数据生成纹理
        glBindTexture(GL_TEXTURE_2D, textYUV[0]);

        // 使用内存中m_pBufYuv420p数据创建真正的y数据纹理
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, curFrame->width,
                     curFrame->height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE,
                     curFrame->data[0]);

        // 加载u数据纹理
        glActiveTexture(GL_TEXTURE1); // 激活纹理单元GL_TEXTURE1
        glBindTexture(GL_TEXTURE_2D, textYUV[1]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, curFrame->width / 2,
                     curFrame->height / 2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE,
                     (char *)curFrame->data[1]);

        // 加载v数据纹理
        glActiveTexture(GL_TEXTURE2); // 激活纹理单元GL_TEXTURE2
        glBindTexture(GL_TEXTURE_2D, textYUV[2]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, curFrame->width / 2,
                     curFrame->height / 2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE,
                     (char *)curFrame->data[2]);
    } else if (curFrame->pixfmt == AV_PIX_FMT_NV12) {
        // glUniform1i(uniformFmt, pixFormat_);

        // 加载y数据纹理
        // 激活纹理单元GL_TEXTURE0
        glActiveTexture(GL_TEXTURE0);

        // 使用来自y数据生成纹理
        glBindTexture(GL_TEXTURE_2D, textYUV[0]);

        // 使用内存中m_pBufYuv420p数据创建真正的y数据纹理
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, curFrame->width,
                     curFrame->height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE,
                     curFrame->data[0]);

        // 加载u数据纹理
        glActiveTexture(GL_TEXTURE1); // 激活纹理单元GL_TEXTURE1
        glBindTexture(GL_TEXTURE_2D, textYUV[1]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA, curFrame->width / 2,
                     curFrame->height / 2, 0, GL_LUMINANCE_ALPHA,
                     GL_UNSIGNED_BYTE, (char *)curFrame->data[1]);
    }
}
