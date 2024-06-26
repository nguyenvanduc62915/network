#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSettings>
#include <QStringListModel>
#include <QMap>
#include <QPair>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegExp>

QT_BEGIN_NAMESPACE
namespace Ui {
    class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void writeLog(const QString &log);

    void newClientConnection();
    void onClientDisconnected();
    void onErrorOccurred(QAbstractSocket::SocketError error);
    void onClientReadyRead();

    void handleMessage(QTcpSocket *sender, QByteArray bytes);
    void processSignIn(QTcpSocket *sender, QByteArray bytes);
    void processSignUp(QTcpSocket *sender, QByteArray bytes);
    void processSignOut(QTcpSocket *sender, QByteArray bytes);
    void processGet(QTcpSocket *sender, QByteArray bytes);
    void processCreateGroup(QTcpSocket *sender, QByteArray bytes);
    void processJoinGroup(QTcpSocket *sender, QByteArray bytes);
    void processCreateFolder(QTcpSocket *sender, QByteArray bytes);
    void processUploadFile(QTcpSocket *sender, QByteArray bytes);
    void processDownloadFile(QTcpSocket *sender, QByteArray bytes);
    void processDelete(QTcpSocket *sender, QByteArray bytes);

    QJsonObject getData(const QString &path, const QString &leader);
    void sendResponse(QTcpSocket *socket, QByteArray bytes);
    void sendFile(QTcpSocket *socket, QString filePath);

    bool isValidGroupName(const QString& groupName);

private:
    Ui::MainWindow *ui;

    QSettings *users;
    QSettings *groups;
    QMap<QString, QSettings*> groupMembers;

    QStringListModel *model;
    QTcpServer *server;
    QMap<QTcpSocket*, QPair<qint64, QString>> clients;
};

#endif // MAINWINDOW_H
