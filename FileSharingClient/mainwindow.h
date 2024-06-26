#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTcpSocket>
#include <QHostAddress>
#include <QStringListModel>
#include <QMap>
#include <QPair>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "itemfile.h"

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
    void displayError(const QString &msg);

    void updateCurrent();
    void updateListWidget();

    void onReadyRead();
    void onSocketDisconnected();
    void onErrorOccurred(QAbstractSocket::SocketError error);

    void sendSignIn();
    void sendSignUp();
    void sendSignOut();
    void sendGet();
    void sendCreateGroup();
    void sendJoinGroup();
    void sendCreateFolder();
    void sendUpload();
    void sendDownload(QJsonObject object);
    void sendDelete(QJsonObject object);

    void handleMessage(QByteArray data);
    void processGet(QByteArray data);
    void processDownload(QByteArray data);

private:
    Ui::MainWindow *ui;

    QStringListModel *model;
    QTcpSocket *socket;
    QList<ItemFile*> items;
    QJsonObject jsonData, current;
    QString currentUser;
};

#endif // MAINWINDOW_H
