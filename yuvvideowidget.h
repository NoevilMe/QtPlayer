#ifndef YUVVIDEOWIDGET_H
#define YUVVIDEOWIDGET_H

#include <QOpenGLBuffer>
#include <QOpenGLExtraFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLWidget>
#include <QFile>


/*
QOpenGLExtraFunctions可以提供VAO相关函数
*/
struct AVFrame;

class YuvVideoWidget : public QOpenGLWidget, protected QOpenGLExtraFunctions {
    Q_OBJECT
public:
    explicit YuvVideoWidget(QWidget *parent = nullptr);
    ~YuvVideoWidget();

    void init(int width, int height);

    void paintFrame(unsigned char *buf);
    void paintAVFrame(AVFrame *frame);

    void resetVideoSize(int width, int height);

public slots:
    void PlayOneFrame();

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

    void resetTextData();

private:
    int width_;
    int height_;

    float videoRatio_= -1;

    QMatrix4x4 trans_;

    GLuint posVbo_;  // 顶点坐标VBO
    GLuint textVbo_; // 纹理坐标VBO
    GLuint vao_;

    GLuint textYUV_[3] = {0};    // 纹理对象ID（YUV各一个）
    GLuint uniformYUV_[3] = {0}; // fragment shader中yuv变量地址

    QOpenGLShaderProgram *program; // 着色器程序容器

    int pixFormat_;
    unsigned char *textData_[3] = {0};

    unsigned char *bufYuv420p_;
    FILE *yuvFile_;

    //    QOpenGLVertexArrayObject vaoQuad;
    //    QOpenGLBuffer vboQuad;
    //    QOpenGLShaderProgram *shaderProgram;
    //    QVector<GLfloat> vertexData;
};

#endif // YUVVIDEOWIDGET_H
