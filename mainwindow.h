#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTcpSocket>
#include <QAudioOutput>
#include <QAudioFormat>
#include <QIODevice>
#include <QByteArray>
#include <QMutex>
#include <QElapsedTimer>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void on_connectButton_clicked();
    void on_disconnectButton_clicked();
    void on_tcpSocket_connected();
    void on_tcpSocket_disconnected();
    void on_tcpSocket_readyRead();
    void on_audioOutput_notify();
    void on_audioOutput_stateChanged(QAudio::State state);

private:
    Ui::MainWindow *ui;
    QTcpSocket *d_tcpSocket = nullptr;
    QAudioOutput *d_audioOutput = nullptr;
    QAudioFormat d_format;
    QElapsedTimer d_dataTimer;
    QByteArray *d_dataBuffer = nullptr;
    QMutex *d_dataBufferMutex = nullptr;
    QIODevice *d_audioStream = nullptr;
    int d_initialCounter;
};

#endif // MAINWINDOW_H
