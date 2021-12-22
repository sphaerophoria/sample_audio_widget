#include <GL/gl.h>
#include <chrono>
#include <cstdint>
#include <iostream>

#include <QOpenGLWidget>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions>

#include "al.h"
#include "alc.h"

#include <QApplication>
#include <QOpenGLDebugLogger>
#include <QTimer>
#include <iterator>
#include <limits>
#include <qnamespace.h>
#include <qopenglshaderprogram.h>
#include <qopenglvertexarrayobject.h>

#include <ratio>
#include <thread>
#include <mutex>

namespace {
    std::size_t incrementIdx(std::size_t idx, std::size_t size) {
        return (idx + 1) % size;
    }
}
class AudioChartWidget : public QOpenGLWidget
{
    Q_OBJECT

public:
    AudioChartWidget(QWidget* parent = nullptr) : QOpenGLWidget(parent) {
        setAttribute(Qt::WA_AlwaysStackOnTop);
        samples_.resize(480 * 10);
    }

    void initializeGL() override;
    void paintGL() override;

    void AddSamples(const std::vector<std::int16_t>& samples) {
        std::lock_guard<std::mutex> lock(mutex_);

        for (std::size_t i = 0; i < samples.size(); ++i) {
            if (sample_pos_ == 0) {
                samples_[pos_] = samples[i];
                pos_ = incrementIdx(pos_, samples_.size());
            }
            sample_pos_ = incrementIdx(sample_pos_, 100);
        }
    }
private:
    std::mutex mutex_;
    QOpenGLDebugLogger logger;
    QOpenGLShaderProgram program_;

    QTimer render_timer_;

    // Max value is INT16_MAX
    std::vector<float> samples_ = {};
    std::size_t pos_ = 0;
    std::size_t sample_pos_ = 0;
};

void AudioChartWidget::initializeGL()
{
    logger.initialize();
    logger.startLogging();
    program_.addShaderFromSourceCode(QOpenGLShader::Vertex,
        R"--(
        #version 330 core
        layout (location = 0) in float aPos;
        uniform int numSamples;
        uniform int bufPos;
        void main(void)
        {
            float xPos = 0.0;
            if (gl_VertexID > bufPos) {
                xPos = float(gl_VertexID - bufPos) / float(numSamples) * 2.0 - 1.0;
            }
            else {
                xPos = float(numSamples - bufPos + gl_VertexID) / float(numSamples) * 2.0 - 1.0;
            }
            float yPos = float(aPos) / 16384.0;
            gl_Position = vec4(xPos, yPos, 0.0, 1.0);
        }
        )--");
    program_.addShaderFromSourceCode(QOpenGLShader::Fragment,
        "out vec4 FragColor;"
        "void main(void)\n"
        "{\n"
        "   gl_FragColor = vec4(0.34 , 0.82, 0.89, 1.0);\n"
        "}");
    program_.link();

    render_timer_.setInterval(33);
    render_timer_.callOnTimeout([this] { this->update(); });
    render_timer_.start();
}

void AudioChartWidget::paintGL()
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto* f = QOpenGLContext::currentContext()->functions();

    program_.bind();
    QOpenGLBuffer buf;

    buf.create();
    buf.bind();
    buf.allocate(samples_.data(), sizeof(float) * samples_.size());

    f->glVertexAttribPointer(0, 1, GL_FLOAT, GL_FALSE, 0, (void*)0);
    f->glEnableVertexAttribArray(0);

    program_.setUniformValue("numSamples", (GLint)samples_.size());
    program_.setUniformValue("bufPos", (GLint)pos_);


    f->glClearColor(0.0, 0.0, 0.0, 0.0);
    f->glClear(GL_COLOR_BUFFER_BIT);
    f->glLineWidth(3.0);

    // Render contiguous pieces of our circular buffer
    f->glDrawArrays(GL_LINE_STRIP, 0, pos_);
    if (pos_ != samples_.size() - 1) {
        f->glDrawArrays(GL_LINE_STRIP, pos_ + 1, samples_.size() - pos_);
        std::array<int, 2> last_line{{(int)samples_.size(), 0}};
        f->glDrawElements(GL_LINE, 2, GL_UNSIGNED_INT, last_line.data());
    }


    program_.release();
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    AudioChartWidget gl;
    gl.resize(640, 480);
    gl.show();


    auto t = std::thread([&] {
        constexpr int SRATE = 48000;
        constexpr int SSIZE = 3840;

        ALCdevice *device = alcCaptureOpenDevice(NULL, SRATE, AL_FORMAT_MONO16, SSIZE);
        alcCaptureStart(device);

        while (true) {
            ALint numSamples = 0;
            alcGetIntegerv(device, ALC_CAPTURE_SAMPLES, (ALCsizei)sizeof(ALint), &numSamples);
            std::vector<std::int16_t> buffer;
            buffer.resize(numSamples);
            alcCaptureSamples(device, buffer.data(), numSamples);
            gl.AddSamples(buffer);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }

        alcCaptureStop(device);
        alcCaptureCloseDevice(device);

        return 0;
    });
    app.exec();
}

#include "main.moc"
