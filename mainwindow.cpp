#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <QAudioDeviceInfo>
#include <math.h>

#define MIN(a,b) (((a)<(b))?(a):(b))

const int audioBufferSize = 8192;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->disconnectButton->setEnabled(false);
    ui->progressNetwork->setMaximum(audioBufferSize * 10);

    d_dataBuffer = new QByteArray();
    d_dataBufferMutex = new QMutex();

    d_format.setSampleRate(16000);
    d_format.setChannelCount(1);
    d_format.setSampleSize(16);
    d_format.setCodec("audio/pcm");
    d_format.setByteOrder(QAudioFormat::LittleEndian);
    d_format.setSampleType(QAudioFormat::SignedInt);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_connectButton_clicked()
{
    qDebug() << "Clicked connect";

    QString host = ui->hostAddress->text();
    int port = ui->hostPort->text().toInt();

    QAudioDeviceInfo info(QAudioDeviceInfo::defaultOutputDevice());
    if (!info.isFormatSupported(d_format)) {
        qWarning() << "Raw audio format not supported!";
        return;
    }

    d_tcpSocket = new QTcpSocket();
    connect(d_tcpSocket, &QTcpSocket::connected, this, &MainWindow::on_tcpSocket_connected);
    connect(d_tcpSocket, &QTcpSocket::disconnected, this, &MainWindow::on_tcpSocket_disconnected);
    connect(d_tcpSocket, &QTcpSocket::readyRead, this, &MainWindow::on_tcpSocket_readyRead);
    d_tcpSocket->connectToHost(host, port, QIODevice::ReadWrite);
    ui->connectButton->setEnabled(false);
    ui->hostAddress->setEnabled(false);
    ui->hostPort->setEnabled(false);
}

void MainWindow::on_disconnectButton_clicked()
{
    qDebug() << "Clicked disconnect";
    ui->disconnectButton->setEnabled(false);
    d_tcpSocket->disconnectFromHost();
}

void MainWindow::on_tcpSocket_connected()
{
    qDebug() << "Socket connected";
    ui->disconnectButton->setEnabled(true);
    d_initialCounter = 0;
    d_dataTimer.start();
}

void MainWindow::on_tcpSocket_disconnected()
{
    qDebug() << "Socket disconnected";

    d_audioStream->close();
    d_audioOutput->stop();
    d_audioOutput->deleteLater();
    d_audioOutput = nullptr;
    d_dataBuffer->clear();

    ui->progressAudio->setValue(0);
    ui->progressNetwork->setValue(0);
    ui->connectButton->setEnabled(true);
    ui->hostAddress->setEnabled(true);
    ui->hostPort->setEnabled(true);
    d_tcpSocket->deleteLater();
    d_tcpSocket = nullptr;
}

void MainWindow::on_tcpSocket_readyRead()
{
    //qDebug() << "Ready to read";
    d_dataBufferMutex->lock();
    int msecsSinceLast = d_dataTimer.elapsed();
    int bytesAvailable = d_tcpSocket->bytesAvailable();
    double byteRate = (double)(bytesAvailable * 1000)/(double)msecsSinceLast;
    ui->bitrateLabel->setText(QString("%1 kb/s").arg(byteRate/1000.0f));
    d_dataBuffer->append(d_tcpSocket->readAll());
    d_dataTimer.start();

    if (d_audioOutput != nullptr \
            && d_audioOutput->state() == QAudio::SuspendedState \
            && (d_dataBuffer->count() > (audioBufferSize * 8))) {
        d_audioOutput->resume();
    }

    d_dataBufferMutex->unlock();
    ui->progressNetwork->setValue(d_dataBuffer->size());
    d_initialCounter++;

    if (d_dataBuffer->count() > (audioBufferSize * 8) && d_audioOutput == nullptr) {
        d_audioOutput = new QAudioOutput(d_format, this);
        d_audioOutput->setNotifyInterval(128);
        d_audioOutput->setBufferSize(audioBufferSize);
        ui->progressAudio->setMaximum(audioBufferSize);
        ui->progressAudio->setValue(0);
        qDebug() << "Buffer size: " << d_audioOutput->bufferSize();
        qDebug() << "Notify interval: " << d_audioOutput->notifyInterval();
        connect(d_audioOutput, &QAudioOutput::notify, this, &MainWindow::on_audioOutput_notify);
        connect(d_audioOutput, &QAudioOutput::stateChanged, this, &MainWindow::on_audioOutput_stateChanged);
        d_audioStream = d_audioOutput->start();
        on_audioOutput_notify();
    }
}

void MainWindow::on_audioOutput_notify()
{
    d_dataBufferMutex->lock();
    int towrite = MIN(d_audioOutput->bytesFree(), d_dataBuffer->size());
    if (towrite > 0) {
        d_audioStream->write(d_dataBuffer->data(), towrite);
        d_dataBuffer->remove(0, towrite);
        ui->progressAudio->setValue(audioBufferSize - d_audioOutput->bytesFree());
        ui->progressNetwork->setValue(d_dataBuffer->size());
    }

    if (d_dataBuffer->count() < (audioBufferSize * 2)) {
        d_audioOutput->suspend();
    }
    d_dataBufferMutex->unlock();
}

void MainWindow::on_audioOutput_stateChanged(QAudio::State state)
{
    switch (state) {
    case QAudio::ActiveState:
        qDebug() << "QAudio: active";
        break;
    case QAudio::SuspendedState:
        qDebug() << "QAudio: suspended";
        break;
    case QAudio::StoppedState:
        qDebug() << "QAudio: stopped";
        break;
    case QAudio::IdleState:
        qDebug() << "QAudio: idle";
        if (d_audioOutput->error() != QAudio::NoError) {
            qDebug() << "QAudio Error " << d_audioOutput->error();
        }
        else if (d_audioStream != nullptr) {
            on_audioOutput_notify();
        }
        break;
    default:
        qDebug() << "Unknown state";
    }
}
