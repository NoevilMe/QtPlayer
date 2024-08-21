#ifndef YUVVIDEOWIDGET_H
#define YUVVIDEOWIDGET_H

#include <QList>
#include <QOpenGLBuffer>
#include <QOpenGLExtraFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLWidget>
#include <QSharedPointer>

/*
QOpenGLExtraFunctions可以提供VAO相关函数
*/
struct AVFrame;
struct VideoFrame;

class YuvVideoWidget : public QOpenGLWidget, protected QOpenGLExtraFunctions {
    Q_OBJECT
public:
    explicit YuvVideoWidget(QWidget *parent = nullptr);
    ~YuvVideoWidget();

    void paintAVFrame(AVFrame *frame);

    void resetVideoSize(int width, int height);

    QList<int> supportedFormats() { return formats; }
    bool isSupportedFormat(int fmt);

    void clear();
    void displayEnable(bool enable);

signals:
    void playFrame(const QSharedPointer<VideoFrame> &frame);

private slots:
    void playVideoSlot(const QSharedPointer<VideoFrame> &frame);

protected:
    virtual void initializeGL();
    virtual void resizeGL(int w, int h);
    virtual void paintGL();

private:
    void initShader(); // 着色器

    void initVBO(); // Vertex Buffer Objects, VBO 管理顶点
    void initVAO(); // VAO绑定VBO属性

    void initTextures(); // 材质，Y,U,V
    void drawTextures();

    void releaseVBO();
    void releaseVAO();
    void releaseTextures();

private:
    QList<int> formats;

    int videoWidth;
    int videoHeight;
    float videoRatio = -1;

    QMatrix4x4 trans;

    GLuint posVBO;  // 顶点坐标VBO
    GLuint textVBO; // 纹理坐标VBO
    GLuint vao;

    GLuint textYUV[3] = {0}; // 纹理对象ID（YUV各一个）

    QOpenGLShaderProgram *program; // 着色器程序容器

    QSharedPointer<VideoFrame> videoFrame;
    bool display;

    //    QOpenGLVertexArrayObject vaoQuad;
    //    QOpenGLBuffer vboQuad;
    //    QVector<GLfloat> vertexData;
};

#endif // YUVVIDEOWIDGET_H
