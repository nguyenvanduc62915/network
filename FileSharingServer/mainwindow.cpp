#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QMessageBox>
#include <QDir>

#include "structs.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow) {
    ui->setupUi(this);

    setWindowFlags(windowFlags() | Qt::MSWindowsFixedSizeDialogHint);

    if (!QDir("data").exists()) {
        QDir().mkdir("data");
    }

    if (!QDir("database").exists()) {
        QDir().mkdir("database");
    }

    users = new QSettings("database\\users.dat", QSettings::IniFormat);
    groups = new QSettings("database\\groups.dat", QSettings::IniFormat);

    QStringList groupList = groups->allKeys();
    foreach (const QString& group, groupList) {
        groupMembers.insert(group, new QSettings("database\\" + group + ".group", QSettings::IniFormat));

        qDebug() << "Group:" << group;
        foreach (const QString& key, groupMembers.value(group)->allKeys()) {
            qDebug() << key << ":" << groupMembers.value(group)->value(key);
        }
    }

    model = new QStringListModel(this);

    ui->listView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->listView->setModel(model);

    server = new QTcpServer();
    if (server->listen(QHostAddress::Any, 1234)) {
        connect(server, &QTcpServer::newConnection, this, &MainWindow::newClientConnection);
        ui->statusbar->showMessage("Server is listening on port 1234...");
        writeLog("Server is listening on port 1234...");
    } else {
        QMessageBox::critical(this, "QTcpServer", QString("Unable to start the server: %1.").arg(server->errorString()));
        exit(EXIT_FAILURE);
    }

    qDebug() << isValidGroupName("group1");
}

MainWindow::~MainWindow() {
    foreach (QTcpSocket* socket, clients.keys()) {
        socket->close();
        socket->deleteLater();
    }

    foreach (const QString& key, groupMembers.keys()) {
        groupMembers.value(key)->deleteLater();
    }

    users->deleteLater();
    groups->deleteLater();
    model->deleteLater();

    server->close();
    server->deleteLater();

    delete ui;
}

void MainWindow::writeLog(const QString& log) {
    if(model->insertRow(model->rowCount())) {
        QModelIndex index = model->index(model->rowCount() - 1, 0);
        model->setData(index, log);
        ui->listView->setCurrentIndex(model->index(model->rowCount() - 1));
    }
}

void MainWindow::newClientConnection() {
    while (server->hasPendingConnections()) {
        QTcpSocket* socket = server->nextPendingConnection();
        QPair<qint64, QString> pair;
        pair.first = socket->socketDescriptor();
        pair.second = QString();
        clients.insert(socket, pair);
        connect(socket, &QTcpSocket::readyRead, this, &MainWindow::onClientReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, &MainWindow::onClientDisconnected);
        connect(socket, &QAbstractSocket::errorOccurred, this, &MainWindow::onErrorOccurred);
        writeLog(QString("Client(%1) has just connected").arg(socket->socketDescriptor()));
    }
}

void MainWindow::onClientDisconnected() {
    QTcpSocket* socket = reinterpret_cast<QTcpSocket*>(sender());
    QMap<QTcpSocket*, QPair<qint64, QString>>::iterator it = clients.find(socket);
    if (it != clients.end()) {
        writeLog(QString("Client(%1) has just disconnected").arg(it.value().first));
        clients.erase(it);
    }

    socket->deleteLater();
}

void MainWindow::onErrorOccurred(QAbstractSocket::SocketError error) {
    switch (error) {
        case QAbstractSocket::RemoteHostClosedError:
            break;

        case QAbstractSocket::HostNotFoundError:
            QMessageBox::information(this, "QTcpServer", "The host was not found.");
            break;

        case QAbstractSocket::ConnectionRefusedError:
            QMessageBox::information(this, "QTcpServer", "The connection was refused by the peer.");
            break;

        default:
            QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
            QMessageBox::information(this, "QTcpServer", QString("The following error occurred: %1.").arg(socket->errorString()));
            break;
    }
}

void MainWindow::onClientReadyRead() {
    QTcpSocket* socket = reinterpret_cast<QTcpSocket*>(sender());

    QByteArray buffer;

    QDataStream socketStream(socket);
    socketStream.setVersion(QDataStream::Qt_5_15);

    socketStream.startTransaction();
    socketStream >> buffer;

    if(!socketStream.commitTransaction()) {
        QString message = QString("%1::Waiting for more data to come..").arg(socket->socketDescriptor());
        qDebug() << message;
        return;
    }

    handleMessage(socket, buffer);
}

void MainWindow::handleMessage(QTcpSocket* sender, QByteArray bytes) {
    int request = bytes.mid(0, 8).toInt();
    bytes = bytes.mid(8);

    switch (request) {
        case RequestNone:
            writeLog(QString("%1> RequestNone(%2)").arg(sender->socketDescriptor()).arg(bytes.size()));
            break;

        case RequestSignIn:
            writeLog(QString("%1> RequestSignIn(%2)").arg(sender->socketDescriptor()).arg(bytes.size()));
            processSignIn(sender, bytes);
            break;

        case RequestSignUp:
            writeLog(QString("%1> RequestSignUp(%2)").arg(sender->socketDescriptor()).arg(bytes.size()));
            processSignUp(sender, bytes);
            break;

        case RequestSignOut:
            writeLog(QString("%1> RequestSignOut(%2)").arg(sender->socketDescriptor()).arg(bytes.size()));
            processSignOut(sender, bytes);
            break;

        case RequestGet:
            writeLog(QString("%1> RequestGet(%2)").arg(sender->socketDescriptor()).arg(bytes.size()));
            processGet(sender, bytes);
            break;

        case RequestCreateGroup:
            writeLog(QString("%1> RequestCreateGroup(%2)").arg(sender->socketDescriptor()).arg(bytes.size()));
            processCreateGroup(sender, bytes);
            break;

        case RequestJoinGroup:
            writeLog(QString("%1> RequestJoinGroup(%2)").arg(sender->socketDescriptor()).arg(bytes.size()));
            processJoinGroup(sender, bytes);
            break;

        case RequestCreateFolder:
            writeLog(QString("%1> RequestCreateFolder(%2)").arg(sender->socketDescriptor()).arg(bytes.size()));
            processCreateFolder(sender, bytes);
            break;

        case RequestUploadFile:
            writeLog(QString("%1> RequestUploadFile(%2)").arg(sender->socketDescriptor()).arg(bytes.size()));
            processUploadFile(sender, bytes);
            break;

        case RequestDownloadFile:
            writeLog(QString("%1> RequestDownloadFile(%2)").arg(sender->socketDescriptor()).arg(bytes.size()));
            processDownloadFile(sender, bytes);
            break;

        case RequestDelete:
            writeLog(QString("%1> RequestDelete(%2)").arg(sender->socketDescriptor()).arg(bytes.size()));
            processDelete(sender, bytes);
            break;

        default:
            writeLog(QString("%1> Invalid request (%2): %3").arg(sender->socketDescriptor()).arg(request).arg(bytes.size()));
            break;
    }
}

void MainWindow::processSignIn(QTcpSocket *sender, QByteArray bytes) {
    QByteArray errorCode = QByteArray::number(ResponseSignInError);
    errorCode.resize(8);
    QByteArray successCode = QByteArray::number(ResponseSignInSuccess);
    successCode.resize(8);

    QString dataStr = bytes;
    QStringList list = dataStr.split(";");
    if (list.size() < 2 || list[0].isEmpty() || list[1].isEmpty()) {
        QString msg = "Invalid data";
        writeLog(QString("%1> processSignIn: %2").arg(sender->socketDescriptor()).arg(msg));

        QByteArray byteArray = msg.toUtf8();
        byteArray.prepend(errorCode);
        sendResponse(sender, byteArray);
        return;
    }

    if (!users->allKeys().contains(list[0], Qt::CaseInsensitive)) {
        QString msg = list[0] + " doesn't exist";
        writeLog(QString("%1> processSignIn: %2").arg(sender->socketDescriptor()).arg(msg));

        QByteArray byteArray = msg.toUtf8();
        byteArray.prepend(errorCode);
        sendResponse(sender, byteArray);
        return;
    }

    if (QString::compare(list[1], users->value(list[0], QString()).toString()) != 0) {
        QString msg = list[0] + "The password is incorrect";
        writeLog(QString("%1> processSignIn: %2").arg(sender->socketDescriptor()).arg(msg));

        QByteArray byteArray = msg.toUtf8();
        byteArray.prepend(errorCode);
        sendResponse(sender, byteArray);
        return;
    }

    QMapIterator<QTcpSocket*, QPair<qint64, QString>> iter(clients);
    while(iter.hasNext()) {
        iter.next();
        if (QString::compare(list[0], iter.value().second) == 0) {
            QString msg = list[0] + " already signed in";
            writeLog(QString("%1> processSignIn: %2").arg(sender->socketDescriptor()).arg(msg));

            QByteArray byteArray = msg.toUtf8();
            byteArray.prepend(errorCode);
            sendResponse(sender, byteArray);
            return;
        }
    }

    QMap<QTcpSocket*, QPair<qint64, QString>>::iterator it = clients.find(sender);
    if (it != clients.end()) {
        it.value().second = list[0];
    }

    QString msg = "SignIn success";
    QByteArray byteArray = msg.toUtf8();
    byteArray.prepend(successCode);
    sendResponse(sender, byteArray);

    writeLog(QString("%1> processSignIn: %2").arg(sender->socketDescriptor()).arg("Success!"));
}

void MainWindow::processSignUp(QTcpSocket *sender, QByteArray bytes) {
    QByteArray errorCode = QByteArray::number(ResponseSignUpError);
    errorCode.resize(8);
    QByteArray successCode = QByteArray::number(ResponseSignUpSuccess);
    successCode.resize(8);

    QString dataStr = bytes;
    QStringList list = dataStr.split(";");
    if (list.size() < 2 || list[0].isEmpty() || list[1].isEmpty()) {
        QString msg = "Invalid data";
        writeLog(QString("%1> processSignUp: %2").arg(sender->socketDescriptor()).arg(msg));

        QByteArray byteArray = msg.toUtf8();
        byteArray.prepend(errorCode);
        sendResponse(sender, byteArray);
        return;
    }

    if (users->allKeys().contains(list[0], Qt::CaseInsensitive)) {
        QString msg = list[0] + " already exist";
        writeLog(QString("%1> processSignUp: %2").arg(sender->socketDescriptor()).arg(msg));

        QByteArray byteArray = msg.toUtf8();
        byteArray.prepend(errorCode);
        sendResponse(sender, byteArray);
        return;
    }

    users->setValue(list[0], list[1]);

    QString msg = "SignUp success";
    QByteArray byteArray = msg.toUtf8();
    byteArray.prepend(successCode);
    sendResponse(sender, byteArray);

    writeLog(QString("%1> processSignUp: (%2, %3) %4").arg(sender->socketDescriptor()).arg(list[0], list[1], "Success!"));
}

void MainWindow::processSignOut(QTcpSocket *sender, QByteArray bytes) {
    QMap<QTcpSocket*, QPair<qint64, QString>>::iterator it = clients.find(sender);
    if (it == clients.end()) {
        QString msg = "An error occurred";
        writeLog(QString("%1> processSignOut: %2").arg(sender->socketDescriptor()).arg(msg));

        QByteArray errorCode = QByteArray::number(ResponseSignOutError);
        errorCode.resize(8);
        QByteArray byteArray = msg.toUtf8();
        byteArray.prepend(errorCode);
        sendResponse(sender, byteArray);
        return;
    }

    it.value().second = QString();

    QString msg = "SignOut success";
    QByteArray typeArray = QByteArray::number(ResponseSignOutSuccess);
    typeArray.resize(8);
    QByteArray byteArray = msg.toUtf8();
    byteArray.prepend(typeArray);
    sendResponse(sender, byteArray);

    writeLog(QString("%1> processSignOut: %2").arg(sender->socketDescriptor()).arg("Success!"));
}

void MainWindow::processGet(QTcpSocket *sender, QByteArray bytes) {
    QByteArray successCode = QByteArray::number(ResponseGetSuccess);
    successCode.resize(8);
    QByteArray errorCode = QByteArray::number(ResponseGetError);
    errorCode.resize(8);

    QMap<QTcpSocket*, QPair<qint64, QString>>::iterator iter = clients.find(sender);
    if (iter == clients.end()) {
        QString msg = "An error occurred";
        writeLog(QString("%1> processGet: %2").arg(sender->socketDescriptor()).arg(msg));

        QByteArray byteArray = msg.toUtf8();
        byteArray.prepend(errorCode);
        sendResponse(sender, byteArray);
        return;
    }

    QString user = iter.value().second;
    if (user.isEmpty()) {
        QString msg = "You are not signed in";
        writeLog(QString("%1> processCreateGroup: %2").arg(sender->socketDescriptor()).arg(msg));

        QByteArray byteArray = msg.toUtf8();
        byteArray.prepend(errorCode);
        sendResponse(sender, byteArray);
        return;
    }

    QJsonArray array;
    foreach (const QString& key, groupMembers.keys()) {
        if (groupMembers.value(key)->allKeys().contains(user, Qt::CaseInsensitive)) {
            QString leader;
            if (groupMembers.value(key)->value(user).toString().compare("1") == 0) {
                leader = user;
            }
            array.push_back(getData(QString("data") + QDir::separator() + key, leader));
        }
    }

    QJsonObject data;
    data.insert("name", "");
    data.insert("path", "");
    data.insert("type", "root");
    data.insert("children", array);

    QJsonDocument jsonDocument;
    jsonDocument.setObject(data);

    QString responseMessage = jsonDocument.toJson(QJsonDocument::Compact);

    QByteArray responseData = responseMessage.toUtf8();
    responseData.prepend(successCode);

    sendResponse(sender, responseData);
}

void MainWindow::processCreateGroup(QTcpSocket *sender, QByteArray bytes) {
    QByteArray successCode = QByteArray::number(ResponseCreateGroupSuccess);
    successCode.resize(8);
    QByteArray errorCode = QByteArray::number(ResponseCreateGroupError);
    errorCode.resize(8);

    QMap<QTcpSocket*, QPair<qint64, QString>>::iterator iter = clients.find(sender);
    if (iter == clients.end()) {
        QString msg = "An error occurred";
        writeLog(QString("%1> processCreateGroup: %2").arg(sender->socketDescriptor()).arg(msg));

        QByteArray byteArray = msg.toUtf8();
        byteArray.prepend(errorCode);
        sendResponse(sender, byteArray);
        return;
    }

    QString user = iter.value().second;
    if (user.isEmpty()) {
        QString msg = "You are not signed in";
        writeLog(QString("%1> processCreateGroup: %2").arg(sender->socketDescriptor()).arg(msg));

        QByteArray byteArray = msg.toUtf8();
        byteArray.prepend(errorCode);
        sendResponse(sender, byteArray);
        return;
    }

    QString groupName = bytes;
    if (!isValidGroupName(groupName)) {
        QString msg = "Group name is invalid";
        writeLog(QString("%1> processCreateGroup: %2").arg(sender->socketDescriptor()).arg(msg));

        QByteArray byteArray = msg.toUtf8();
        byteArray.prepend(errorCode);
        sendResponse(sender, byteArray);
        return;
    }

    if (groups->allKeys().contains(groupName, Qt::CaseInsensitive)) {
        QString msg = groupName + " already exist";
        writeLog(QString("%1> processCreateGroup: %2").arg(sender->socketDescriptor()).arg(msg));

        QByteArray byteArray = msg.toUtf8();
        byteArray.prepend(errorCode);
        sendResponse(sender, byteArray);
        return;
    }

    QDir dir(QString("data") + QDir::separator() + groupName);
    if (dir.exists()) {
        dir.removeRecursively();
    }
    QDir().mkdir(QString("data") + QDir::separator() + groupName);

    groups->setValue(groupName, user);
    groupMembers.insert(groupName, new QSettings("database\\" + groupName + ".group", QSettings::IniFormat));
    QSettings* members = groupMembers.value(groupName);
    members->clear();
    members->setValue(user, "1");

    QJsonArray array;
    foreach (const QString& key, groupMembers.keys()) {
        if (groupMembers.value(key)->allKeys().contains(user, Qt::CaseInsensitive)) {
            QString leader;
            if (groupMembers.value(key)->value(user).toString().compare("1") == 0) {
                leader = user;
            }
            array.push_back(getData(QString("data") + QDir::separator() + key, leader));
        }
    }

    QJsonObject data;
    data.insert("name", "");
    data.insert("path", "");
    data.insert("type", "root");
    data.insert("children", array);

    QJsonDocument jsonDocument;
    jsonDocument.setObject(data);

    QString responseMessage = jsonDocument.toJson(QJsonDocument::Compact);

    QByteArray responseData = responseMessage.toUtf8();
    responseData.prepend(successCode);

    sendResponse(sender, responseData);

    writeLog(QString("%1> processCreateGroup: %2").arg(sender->socketDescriptor()).arg("Success!"));
}

void MainWindow::processJoinGroup(QTcpSocket *sender, QByteArray bytes) {
    QByteArray successCode = QByteArray::number(ResponseJoinGroupSuccess);
    successCode.resize(8);
    QByteArray errorCode = QByteArray::number(ResponseJoinGroupError);
    errorCode.resize(8);

    QMap<QTcpSocket*, QPair<qint64, QString>>::iterator iter = clients.find(sender);
    if (iter == clients.end()) {
        QString msg = "An error occurred";
        writeLog(QString("%1> processJoinGroup: %2").arg(sender->socketDescriptor()).arg(msg));

        QByteArray byteArray = msg.toUtf8();
        byteArray.prepend(errorCode);
        sendResponse(sender, byteArray);
        return;
    }

    QString user = iter.value().second;
    if (user.isEmpty()) {
        QString msg = "You are not signed in";
        writeLog(QString("%1> processJoinGroup: %2").arg(sender->socketDescriptor()).arg(msg));

        QByteArray byteArray = msg.toUtf8();
        byteArray.prepend(errorCode);
        sendResponse(sender, byteArray);
        return;
    }

    QString groupName = bytes;
    if (!groups->allKeys().contains(groupName, Qt::CaseInsensitive)) {
        QString msg = groupName + " not exist";
        writeLog(QString("%1> processJoinGroup: %2").arg(sender->socketDescriptor()).arg(msg));

        QByteArray byteArray = msg.toUtf8();
        byteArray.prepend(errorCode);
        sendResponse(sender, byteArray);
        return;
    }

    QSettings* members = groupMembers.value(groupName);
    if (members->allKeys().contains(user, Qt::CaseInsensitive)) {
        QString msg = groupName + " already in group";
        writeLog(QString("%1> processJoinGroup: %2").arg(sender->socketDescriptor()).arg(msg));

        QByteArray byteArray = msg.toUtf8();
        byteArray.prepend(errorCode);
        sendResponse(sender, byteArray);
        return;
    }

    members->setValue(user, "0");

    QJsonArray array;
    foreach (const QString& key, groupMembers.keys()) {
        if (groupMembers.value(key)->allKeys().contains(user, Qt::CaseInsensitive)) {
            QString leader;
            if (groupMembers.value(key)->value(user).toString().compare("1") == 0) {
                leader = user;
            }
            array.push_back(getData(QString("data") + QDir::separator() + key, leader));
        }
    }

    QJsonObject data;
    data.insert("name", "");
    data.insert("path", "");
    data.insert("type", "root");
    data.insert("children", array);

    QJsonDocument jsonDocument;
    jsonDocument.setObject(data);

    QString responseMessage = jsonDocument.toJson(QJsonDocument::Compact);

    QByteArray responseData = responseMessage.toUtf8();
    responseData.prepend(successCode);

    sendResponse(sender, responseData);

    writeLog(QString("%1> processJoinGroup: %2").arg(sender->socketDescriptor()).arg("Success!"));
}

void MainWindow::processCreateFolder(QTcpSocket *sender, QByteArray bytes) {
    QByteArray successCode = QByteArray::number(ResponseCreateFolderSuccess);
    successCode.resize(8);
    QByteArray errorCode = QByteArray::number(ResponseCreateFolderError);
    errorCode.resize(8);

    QMap<QTcpSocket*, QPair<qint64, QString>>::iterator iter = clients.find(sender);
    if (iter == clients.end()) {
        QString msg = "An error occurred";
        writeLog(QString("%1> processCreateFolder: %2").arg(sender->socketDescriptor()).arg(msg));

        QByteArray byteArray = msg.toUtf8();
        byteArray.prepend(errorCode);
        sendResponse(sender, byteArray);
        return;
    }

    QString user = iter.value().second;
    if (user.isEmpty()) {
        QString msg = "You are not signed in";
        writeLog(QString("%1> processCreateFolder: %2").arg(sender->socketDescriptor()).arg(msg));

        QByteArray byteArray = msg.toUtf8();
        byteArray.prepend(errorCode);
        sendResponse(sender, byteArray);
        return;
    }

    QString folderPath = bytes;
    if (!folderPath.contains(QDir::separator())) {
        QString msg = "Invalid folder path";
        writeLog(QString("%1> processCreateFolder: %2").arg(sender->socketDescriptor()).arg(msg));

        QByteArray byteArray = msg.toUtf8();
        byteArray.prepend(errorCode);
        sendResponse(sender, byteArray);
        return;
    }


    QString groupName = folderPath.left(folderPath.indexOf(QDir::separator()));
    if (!groups->allKeys().contains(groupName, Qt::CaseInsensitive)) {
        QString msg = groupName + " not exist";
        writeLog(QString("%1> processCreateFolder: %2").arg(sender->socketDescriptor()).arg(msg));

        QByteArray byteArray = msg.toUtf8();
        byteArray.prepend(errorCode);
        sendResponse(sender, byteArray);
        return;
    }

    QSettings* members = groupMembers.value(groupName);
    if (!members->allKeys().contains(user, Qt::CaseInsensitive)) {
        QString msg = "Access denied";
        writeLog(QString("%1> processCreateFolder: %2").arg(sender->socketDescriptor()).arg(msg));

        QByteArray byteArray = msg.toUtf8();
        byteArray.prepend(errorCode);
        sendResponse(sender, byteArray);
        return;
    }

    QDir dir(QString("data") + QDir::separator() + folderPath);
    if (dir.exists()) {
        QString msg = "Folder already exists";
        writeLog(QString("%1> processCreateFolder: %2").arg(sender->socketDescriptor()).arg(msg));

        QByteArray byteArray = msg.toUtf8();
        byteArray.prepend(errorCode);
        sendResponse(sender, byteArray);
        return;
    }

    if (!QDir().mkpath(dir.absolutePath())) {
        QString msg = "Cannot create folder";
        writeLog(QString("%1> processCreateFolder: %2").arg(sender->socketDescriptor()).arg(msg));

        QByteArray byteArray = msg.toUtf8();
        byteArray.prepend(errorCode);
        sendResponse(sender, byteArray);
        return;
    }

    QJsonArray array;
    foreach (const QString& key, groupMembers.keys()) {
        if (groupMembers.value(key)->allKeys().contains(user, Qt::CaseInsensitive)) {
            QString leader;
            if (groupMembers.value(key)->value(user).toString().compare("1") == 0) {
                leader = user;
            }
            array.push_back(getData(QString("data") + QDir::separator() + key, leader));
        }
    }

    QJsonObject data;
    data.insert("name", "");
    data.insert("path", "");
    data.insert("type", "root");
    data.insert("children", array);

    QJsonDocument jsonDocument;
    jsonDocument.setObject(data);

    QString responseMessage = jsonDocument.toJson(QJsonDocument::Compact);

    QByteArray responseData = responseMessage.toUtf8();
    responseData.prepend(successCode);

    sendResponse(sender, responseData);

    writeLog(QString("%1> processCreateFolder: %2").arg(sender->socketDescriptor()).arg("Success!"));
}

void MainWindow::processUploadFile(QTcpSocket *sender, QByteArray bytes) {
    QByteArray successCode = QByteArray::number(ResponseUploadFileSuccess);
    successCode.resize(8);
    QByteArray errorCode = QByteArray::number(ResponseUploadFileError);
    errorCode.resize(8);

    QMap<QTcpSocket*, QPair<qint64, QString>>::iterator iter = clients.find(sender);
    if (iter == clients.end()) {
        QString msg = "An error occurred";
        writeLog(QString("%1> processUploadFile: %2").arg(sender->socketDescriptor()).arg(msg));

        QByteArray byteArray = msg.toUtf8();
        byteArray.prepend(errorCode);
        sendResponse(sender, byteArray);
        return;
    }

    QString user = iter.value().second;
    if (user.isEmpty()) {
        QString msg = "You are not signed in";
        writeLog(QString("%1> processUploadFile: %2").arg(sender->socketDescriptor()).arg(msg));

        QByteArray byteArray = msg.toUtf8();
        byteArray.prepend(errorCode);
        sendResponse(sender, byteArray);
        return;
    }

    QString filePath = bytes.mid(0, 256);
    bytes = bytes.mid(256);

    if (!filePath.contains(QDir::separator())) {
        QString msg = "Invalid folder path";
        writeLog(QString("%1> processUploadFile: %2").arg(sender->socketDescriptor()).arg(msg));

        QByteArray byteArray = msg.toUtf8();
        byteArray.prepend(errorCode);
        sendResponse(sender, byteArray);
        return;
    }


    QString groupName = filePath.left(filePath.indexOf(QDir::separator()));
    if (!groups->allKeys().contains(groupName, Qt::CaseInsensitive)) {
        QString msg = groupName + " not exist";
        writeLog(QString("%1> processUploadFile: %2").arg(sender->socketDescriptor()).arg(msg));

        QByteArray byteArray = msg.toUtf8();
        byteArray.prepend(errorCode);
        sendResponse(sender, byteArray);
        return;
    }

    QSettings* members = groupMembers.value(groupName);
    if (!members->allKeys().contains(user, Qt::CaseInsensitive)) {
        QString msg = "Access denied";
        writeLog(QString("%1> processUploadFile: %2").arg(sender->socketDescriptor()).arg(msg));

        QByteArray byteArray = msg.toUtf8();
        byteArray.prepend(errorCode);
        sendResponse(sender, byteArray);
        return;
    }

    QFileInfo info(QString("data") + QDir::separator() + filePath);
    if (info.exists()) {
        QString msg = "File already exists";
        writeLog(QString("%1> processUploadFile: %2").arg(sender->socketDescriptor()).arg(msg));

        QByteArray byteArray = msg.toUtf8();
        byteArray.prepend(errorCode);
        sendResponse(sender, byteArray);
        return;
    }

    QFile file = info.filePath();
    if (file.open(QIODevice::WriteOnly)) {
        file.write(bytes);

        QJsonArray array;
        foreach (const QString& key, groupMembers.keys()) {
            if (groupMembers.value(key)->allKeys().contains(user, Qt::CaseInsensitive)) {
                QString leader;
                if (groupMembers.value(key)->value(user).toString().compare("1") == 0) {
                    leader = user;
                }
                array.push_back(getData(QString("data") + QDir::separator() + key, leader));
            }
        }

        QJsonObject data;
        data.insert("name", "");
        data.insert("path", "");
        data.insert("type", "root");
        data.insert("children", array);

        QJsonDocument jsonDocument;
        jsonDocument.setObject(data);

        QString responseMessage = jsonDocument.toJson(QJsonDocument::Compact);

        QByteArray responseData = responseMessage.toUtf8();
        responseData.prepend(successCode);

        sendResponse(sender, responseData);

        writeLog(QString("%1> processUploadFile: %2").arg(sender->socketDescriptor()).arg("Success!"));
    } else {
        QString msg = "An error occurred while trying to write the file";

        QByteArray byteArray = msg.toUtf8();
        byteArray.prepend(errorCode);
        sendResponse(sender, byteArray);

        writeLog(QString("%1> processUploadFile: %2").arg(sender->socketDescriptor()).arg(msg));
        return;
    }
}

void MainWindow::processDownloadFile(QTcpSocket *sender, QByteArray bytes) {
    QByteArray successCode = QByteArray::number(ResponseDownloadFileSuccess);
    successCode.resize(8);
    QByteArray errorCode = QByteArray::number(ResponseDownloadFileError);
    errorCode.resize(8);

    QMap<QTcpSocket*, QPair<qint64, QString>>::iterator iter = clients.find(sender);
    if (iter == clients.end()) {
        QString msg = "An error occurred";
        writeLog(QString("%1> processDownloadFile: %2").arg(sender->socketDescriptor()).arg(msg));

        QByteArray byteArray = msg.toUtf8();
        byteArray.prepend(errorCode);
        sendResponse(sender, byteArray);
        return;
    }

    QString user = iter.value().second;
    if (user.isEmpty()) {
        QString msg = "You are not signed in";
        writeLog(QString("%1> processDownloadFile: %2").arg(sender->socketDescriptor()).arg(msg));

        QByteArray byteArray = msg.toUtf8();
        byteArray.prepend(errorCode);
        sendResponse(sender, byteArray);
        return;
    }

    QString filePath = bytes;

    if (!filePath.contains(QDir::separator())) {
        QString msg = "Invalid folder path";
        writeLog(QString("%1> processUploadFile: %2").arg(sender->socketDescriptor()).arg(msg));

        QByteArray byteArray = msg.toUtf8();
        byteArray.prepend(errorCode);
        sendResponse(sender, byteArray);
        return;
    }


    QString groupName = filePath.left(filePath.indexOf(QDir::separator()));
    if (!groups->allKeys().contains(groupName, Qt::CaseInsensitive)) {
        QString msg = groupName + " not exist";
        writeLog(QString("%1> processUploadFile: %2").arg(sender->socketDescriptor()).arg(msg));

        QByteArray byteArray = msg.toUtf8();
        byteArray.prepend(errorCode);
        sendResponse(sender, byteArray);
        return;
    }

    QSettings* members = groupMembers.value(groupName);
    if (!members->allKeys().contains(user, Qt::CaseInsensitive)) {
        QString msg = "Access denied";
        writeLog(QString("%1> processUploadFile: %2").arg(sender->socketDescriptor()).arg(msg));

        QByteArray byteArray = msg.toUtf8();
        byteArray.prepend(errorCode);
        sendResponse(sender, byteArray);
        return;
    }

    QFileInfo info(QString("data") + QDir::separator() + filePath);
    if (!info.exists() || !info.isFile()) {
        QString msg = "Invalid data";

        QByteArray byteArray = msg.toUtf8();
        byteArray.prepend(errorCode);
        sendResponse(sender, byteArray);

        writeLog(QString("%1> processDownloadFile: %2").arg(sender->socketDescriptor()).arg(msg));
        return;
    }

    sendFile(sender, info.filePath());
}

void MainWindow::processDelete(QTcpSocket *sender, QByteArray bytes) {
    QByteArray successCode = QByteArray::number(ResponseDeleteSuccess);
    successCode.resize(8);
    QByteArray errorCode = QByteArray::number(ResponseDeleteError);
    errorCode.resize(8);

    QMap<QTcpSocket*, QPair<qint64, QString>>::iterator iter = clients.find(sender);
    if (iter == clients.end()) {
        QString msg = "An error occurred";
        writeLog(QString("%1> processDelete: %2").arg(sender->socketDescriptor()).arg(msg));

        QByteArray byteArray = msg.toUtf8();
        byteArray.prepend(errorCode);
        sendResponse(sender, byteArray);
        return;
    }

    QString user = iter.value().second;
    if (user.isEmpty()) {
        QString msg = "You are not signed in";
        writeLog(QString("%1> processDelete: %2").arg(sender->socketDescriptor()).arg(msg));

        QByteArray byteArray = msg.toUtf8();
        byteArray.prepend(errorCode);
        sendResponse(sender, byteArray);
        return;
    }

    QString path = bytes;
    if (!path.contains(QDir::separator())) {
        QString msg = "Invalid folder path";
        writeLog(QString("%1> processCreateFolder: %2").arg(sender->socketDescriptor()).arg(msg));

        QByteArray byteArray = msg.toUtf8();
        byteArray.prepend(errorCode);
        sendResponse(sender, byteArray);
        return;
    }


    QString groupName = path.left(path.indexOf(QDir::separator()));
    if (!groups->allKeys().contains(groupName, Qt::CaseInsensitive)) {
        QString msg = groupName + " not exist";
        writeLog(QString("%1> processCreateFolder: %2").arg(sender->socketDescriptor()).arg(msg));

        QByteArray byteArray = msg.toUtf8();
        byteArray.prepend(errorCode);
        sendResponse(sender, byteArray);
        return;
    }

    QSettings* members = groupMembers.value(groupName);
    if (!members->allKeys().contains(user, Qt::CaseInsensitive)) {
        QString msg = "Access denied";
        writeLog(QString("%1> processCreateFolder: %2").arg(sender->socketDescriptor()).arg(msg));

        QByteArray byteArray = msg.toUtf8();
        byteArray.prepend(errorCode);
        sendResponse(sender, byteArray);
        return;
    }

    if (members->value(user).toString().compare("1") != 0) {
        QString msg = "Access denied";
        writeLog(QString("%1> processCreateFolder: %2").arg(sender->socketDescriptor()).arg(msg));

        QByteArray byteArray = msg.toUtf8();
        byteArray.prepend(errorCode);
        sendResponse(sender, byteArray);
        return;
    }

    QFileInfo info(QString("data") + QDir::separator() + path);
    if (info.exists()) {
        if (info.isFile()) {
            if (!QFile(info.filePath()).remove()) {
                QString msg = "Cannot delete file";

                QByteArray byteArray = msg.toUtf8();
                byteArray.prepend(errorCode);
                sendResponse(sender, byteArray);

                writeLog(QString("%1> processDelete: %2").arg(sender->socketDescriptor()).arg(msg));
                return;
            }
        } else if (info.isDir()) {
            if (!QDir(info.filePath()).removeRecursively()) {
                QString msg = "Cannot delete folder";

                QByteArray byteArray = msg.toUtf8();
                byteArray.prepend(errorCode);
                sendResponse(sender, byteArray);

                writeLog(QString("%1> processDelete: %2").arg(sender->socketDescriptor()).arg(msg));
                return;
            }
        }
    }


    QJsonArray array;
    foreach (const QString& key, groupMembers.keys()) {
        if (groupMembers.value(key)->allKeys().contains(user, Qt::CaseInsensitive)) {
            QString leader;
            if (groupMembers.value(key)->value(user).toString().compare("1") == 0) {
                leader = user;
            }
            array.push_back(getData(QString("data") + QDir::separator() + key, leader));
        }
    }

    QJsonObject data;
    data.insert("name", "");
    data.insert("path", "");
    data.insert("type", "root");
    data.insert("children", array);

    QJsonDocument jsonDocument;
    jsonDocument.setObject(data);

    QString responseMessage = jsonDocument.toJson(QJsonDocument::Compact);

    QByteArray responseData = responseMessage.toUtf8();
    responseData.prepend(successCode);

    sendResponse(sender, responseData);

    writeLog(QString("%1> processDelete: %2").arg(sender->socketDescriptor()).arg("Success"));
}

QJsonObject MainWindow::getData(const QString &path, const QString& leader) {
    QString tmpPath = path;
    QJsonObject object;
    object.insert("leader", leader);

    QFileInfo info(path);
    object.insert("name", info.fileName());
    object.insert("path", tmpPath.replace(QString("data") + QDir::separator(), ""));

    if (info.isDir()) {
        object.insert("type", "dir");
        QDir dir(path);
        QJsonArray children;
        foreach (const QFileInfo& file, dir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries, QDir::DirsFirst | QDir::Name)) {
            children.push_back(getData(file.filePath().replace("/", QDir::separator()).replace("\\", QDir::separator()), leader));
        }
        object.insert("children", children);
    } else {
        object.insert("type", "file");
        object.insert("size", info.size());
    }

    return object;
}

void MainWindow::sendResponse(QTcpSocket *socket, QByteArray bytes) {
    if(socket) {
        if(socket->isOpen()) {
            QDataStream socketStream(socket);
            socketStream.setVersion(QDataStream::Qt_5_15);
            socketStream << bytes;
        } else {
            QMessageBox::critical(this, "QTcpServer", "Socket doesn't seem to be opened");
        }
    } else {
        QMessageBox::critical(this, "QTcpServer", "Not connected");
    }
}

void MainWindow::sendFile(QTcpSocket *socket, QString filePath) {
    QByteArray successCode = QByteArray::number(ResponseDownloadFileSuccess);
    successCode.resize(8);
    QByteArray errorCode = QByteArray::number(ResponseDownloadFileError);
    errorCode.resize(8);

    if(socket) {
        if(socket->isOpen()) {
            QFile file(filePath);
            if(file.open(QIODevice::ReadOnly)){
                writeLog(QString("%1> sendFile: %2").arg(socket->socketDescriptor()).arg("OK!"));

                QFileInfo fileInfo(filePath);
                QString fileName(fileInfo.fileName());

                QDataStream socketStream(socket);
                socketStream.setVersion(QDataStream::Qt_5_15);

                QByteArray header;
                header.prepend(QString("%1,%2").arg(fileName).arg(file.size()).toUtf8());
                header.resize(128);

                QByteArray byteArray = file.readAll();
                byteArray.prepend(header);

                byteArray.prepend(successCode);

                socketStream << byteArray;
            } else {
                QString msg = "Couldn't open the file";
                writeLog(QString("%1::sendFile: %2").arg(socket->socketDescriptor()).arg(msg));

                QByteArray byteArray = msg.toUtf8();
                byteArray.prepend(errorCode);
                sendResponse(socket, byteArray);
                return;
            }
        } else {
            QMessageBox::critical(this,"QTcpServer","Socket doesn't seem to be opened");
        }
    } else {
        QMessageBox::critical(this,"QTcpServer","Not connected");
    }
}

bool MainWindow::isValidGroupName(const QString &groupName) {
    if (groupName.isEmpty()) {
        return false;
    }

    QRegExp regex("^[A-Za-z0-9_\\- ]*$");
    return regex.exactMatch(groupName);
}
