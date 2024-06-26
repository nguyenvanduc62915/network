#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDebug>
#include <QDir>
#include <QQueue>
#include <QMessageBox>
#include <QInputDialog>
#include <QListWidgetItem>
#include <QJsonValue>
#include <QFileDialog>
#include <QStandardPaths>

#include "structs.h"
#include "itemfile.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow) {
    ui->setupUi(this);

    setWindowFlags(windowFlags() | Qt::MSWindowsFixedSizeDialogHint);

    ui->pages->setCurrentIndex(1);

    socket = new QTcpSocket(this);

    connect(socket, &QTcpSocket::readyRead, this, &MainWindow::onReadyRead);
    connect(socket, &QTcpSocket::disconnected, this, &MainWindow::onSocketDisconnected);
    connect(socket, &QAbstractSocket::errorOccurred, this, &MainWindow::onErrorOccurred);

    socket->connectToHost(QHostAddress::LocalHost, 1234);
    if (socket->waitForConnected()) {
        qDebug() << "Connected to Server";
    } else {
        QMessageBox::critical(this, "QTcpClient", QString("The following error occurred: %1.").arg(socket->errorString()));
        exit(EXIT_FAILURE);
    }

    connect(ui->btnSignIn, &QPushButton::clicked, this, [this]() {
        sendSignIn();
    });

    connect(ui->btnSignUp, &QPushButton::clicked, this, [this]() {
        sendSignUp();
    });

    connect(ui->btnSignOut, &QPushButton::clicked, this, [this]() {
        sendSignOut();
    });

    connect(ui->btnRefresh, &QPushButton::clicked, this, [this]() {
        sendGet();
    });

    connect(ui->btnCreateGroup, &QPushButton::clicked, this, [this]() {
        sendCreateGroup();
    });

    connect(ui->btnJoinGroup, &QPushButton::clicked, this, [this]() {
        sendJoinGroup();
    });

    connect(ui->btnCreateFolder, &QPushButton::clicked, this, [this]() {
        sendCreateFolder();
    });

    connect(ui->btnUpload, &QPushButton::clicked, this, [this]() {
        sendUpload();
    });

    connect(ui->btnDownload, &QPushButton::clicked, this, [this]() {
        QListWidgetItem* item = ui->listWidget->currentItem();
        if (item) {
            for (int i = 0; i < ui->listWidget->count(); i++) {
                if (ui->listWidget->item(i) == item) {
                    sendDownload(items[i]->getData());
                    break;
                }
            }
        } else {
            qDebug() << ("Download: Please select a file");
            QMessageBox::information(this, "Information", "Please select a file");
        }
    });

    connect(ui->btnDelete, &QPushButton::clicked, this, [this]() {
        QListWidgetItem* item = ui->listWidget->currentItem();
        if (item) {
            for (int i = 0; i < ui->listWidget->count(); i++) {
                if (ui->listWidget->item(i) == item) {
                    sendDelete(items[i]->getData());
                    break;
                }
            }
        } else {
            qDebug() << ("Download: Please select a file");
            QMessageBox::information(this, "Information", "Please select a file");
        }
    });

    connect(ui->listWidget, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        for (int i = 0; i < ui->listWidget->count(); i++) {
            if (ui->listWidget->item(i) == item) {
                if (items[i]->getData().value("type").toString() == "dir") {
                    current = items[i]->getData();
                    updateListWidget();
                }
                break;
            }
        }
    });

    connect(ui->btnBack, &QPushButton::clicked, this, [this]() {
        QString path = current.value("path").toString();
        QString parentPath;
        if (path.lastIndexOf(QDir::separator()) != -1) {
            parentPath = path.mid(0, path.lastIndexOf(QDir::separator()));
        }

        QQueue<QJsonObject> queue;
        queue.enqueue(jsonData);
        while (!queue.isEmpty()) {
            QJsonObject object = queue.dequeue();
            if (object.value("path").toString() == parentPath) {
                current = object;
                updateListWidget();
                break;
            }

            QJsonArray children = object.value("children").toArray();
            for (int i = 0; i < children.count(); i++) {
                QJsonObject child = children.at(i).toObject();
                if (child.value("type").toString() == "dir") {
                    queue.enqueue(child);
                }
            }
        }
    });
}

MainWindow::~MainWindow() {
    if (socket->isOpen()) {
        socket->close();
        socket->deleteLater();
    }

    delete ui;
}

void MainWindow::displayError(const QString &msg) {
    QMessageBox::critical(this, "Error", msg);
}

void MainWindow::updateCurrent() {
    if (current.isEmpty() || current.value("path").toString().isEmpty()) {
        return;
    }

    QString path = current.value("path").toString();

    QQueue<QJsonObject> queue;
    queue.enqueue(jsonData);
    while (!queue.isEmpty()) {
        QJsonObject object = queue.dequeue();
        if (object.value("path").toString() == path) {
            current = object;
            return;
        }

        QJsonArray children = object.value("children").toArray();
        for (int i = 0; i < children.count(); i++) {
            QJsonObject child = children.at(i).toObject();
            if (child.value("type").toString() == "dir") {
                queue.enqueue(child);
            }
        }
    }

    current = QJsonObject();
}

void MainWindow::updateListWidget() {
    ui->btnCreateFolder->hide();
    ui->btnUpload->hide();
    ui->btnDownload->hide();
    ui->btnDelete->hide();

    if (current.isEmpty() || current.value("path").toString().isEmpty()) {
        current = jsonData;
        ui->btnBack->setEnabled(false);
        ui->lbPath->setText(QString(">"));
    } else {
        QString path = current.value("path").toString().replace(QDir::separator(), " > ");
        ui->lbPath->setText(QString("> ") + path);
        ui->btnBack->setEnabled(!path.isEmpty());
        ui->btnCreateFolder->show();
        ui->btnUpload->show();
        ui->btnDownload->show();
        if (!currentUser.isEmpty() && currentUser == current.value("leader").toString()) {
            ui->btnDelete->show();
        }
    }

    items.clear();
    ui->listWidget->clear();

    QJsonArray children = current.value("children").toArray();
    for (int i = 0; i < children.count(); i++) {
        auto widget = new ItemFile(this);
        widget->setData(children.at(i).toObject());

        items.append(widget);

        auto item = new QListWidgetItem();
        item->setSizeHint(QSize(480, 48));

        ui->listWidget->addItem(item);
        ui->listWidget->setItemWidget(item, widget);
    }
}

void MainWindow::onReadyRead() {
    QByteArray buffer;

    QDataStream socketStream(socket);
    socketStream.setVersion(QDataStream::Qt_5_15);

    socketStream.startTransaction();
    socketStream >> buffer;

    if(!socketStream.commitTransaction()) {
        QString message = QString("%1> Waiting for more data to come..").arg(socket->socketDescriptor());
        qDebug() << message;
        return;
    }

    handleMessage(buffer);
}

void MainWindow::onSocketDisconnected() {
    socket->deleteLater();
    socket = nullptr;
    qDebug() << "Disconnected";
}

void MainWindow::onErrorOccurred(QAbstractSocket::SocketError error) {
    switch (error) {
        case QAbstractSocket::RemoteHostClosedError:
            break;

        case QAbstractSocket::HostNotFoundError:
            QMessageBox::information(this, "QTcpClient", "The host was not found. Please check the host name and port settings.");
            break;

        case QAbstractSocket::ConnectionRefusedError:
            QMessageBox::information(this, "QTcpClient", "The connection was refused by the peer. Make sure QTCPServer is running, and check that the host name and port settings are correct.");
            break;

        default:
            QMessageBox::information(this, "QTcpClient", QString("The following error occurred: %1.").arg(socket->errorString()));
            break;
    }
}

void MainWindow::sendSignIn() {
    if(socket) {
        if(socket->isOpen()) {
            QString username = ui->edtUsername->text();
            QString password = ui->edtPassword->text();
            if (username.isEmpty() || password.isEmpty()) {
                QMessageBox::warning(this, "Warning", "Please fill all required fields");
                return;
            }

            QDataStream socketStream(socket);
            socketStream.setVersion(QDataStream::Qt_5_15);

            Request type = Request::RequestSignIn;
            QByteArray typeArray = QByteArray::number(type);
            typeArray.resize(8);

            QString str = username + ";" + password;
            QByteArray byteArray = str.toUtf8();
            byteArray.prepend(typeArray);

            socketStream << byteArray;
        } else {
            QMessageBox::critical(this, "QTcpClient", "Socket doesn't seem to be opened");
        }
    } else {
        QMessageBox::critical(this, "QTcpClient", "Not connected");
    }
}

void MainWindow::sendSignUp() {
    if(socket) {
        if(socket->isOpen()) {
            QString username = ui->edtUsername->text();
            QString password = ui->edtPassword->text();
            if (username.isEmpty() || password.isEmpty()) {
                QMessageBox::warning(this, "Warning", "Please fill all required fields");
                return;
            }

            QDataStream socketStream(socket);
            socketStream.setVersion(QDataStream::Qt_5_15);

            Request type = Request::RequestSignUp;
            QByteArray typeArray = QByteArray::number(type);
            typeArray.resize(8);

            QString str = username + ";" + password;
            QByteArray byteArray = str.toUtf8();
            byteArray.prepend(typeArray);

            socketStream << byteArray;
        } else {
            QMessageBox::critical(this, "QTcpClient", "Socket doesn't seem to be opened");
        }
    } else {
        QMessageBox::critical(this, "QTcpClient", "Not connected");
    }
}

void MainWindow::sendSignOut() {
    if(socket) {
        if(socket->isOpen()) {
            QString str = currentUser;

            QDataStream socketStream(socket);
            socketStream.setVersion(QDataStream::Qt_5_15);

            Request type = Request::RequestSignOut;
            QByteArray typeArray = QByteArray::number(type);
            typeArray.resize(8);

            QByteArray byteArray = str.toUtf8();
            byteArray.prepend(typeArray);

            socketStream << byteArray;
        } else {
            QMessageBox::critical(this, "QTcpClient", "Socket doesn't seem to be opened");
        }
    } else {
        QMessageBox::critical(this, "QTcpClient", "Not connected");
    }
}

void MainWindow::sendGet() {
    if(socket) {
        if(socket->isOpen()) {
            QString str = currentUser;

            QDataStream socketStream(socket);
            socketStream.setVersion(QDataStream::Qt_5_15);

            Request type = Request::RequestGet;
            QByteArray typeArray = QByteArray::number(type);
            typeArray.resize(8);

            QByteArray byteArray = str.toUtf8();
            byteArray.prepend(typeArray);

            socketStream << byteArray;
        } else {
            QMessageBox::critical(this, "QTcpClient", "Socket doesn't seem to be opened");
        }
    } else {
        QMessageBox::critical(this, "QTcpClient", "Not connected");
    }
}

void MainWindow::sendCreateGroup() {
    bool ok;
    QString name;
    do {
        name = QInputDialog::getText(0, "Create Group", "Group name:", QLineEdit::Normal, "", &ok);
        if (!ok) {
            return;
        }
    } while (ok && name.isEmpty());

    if(socket) {
        if(socket->isOpen()) {
            QDataStream socketStream(socket);
            socketStream.setVersion(QDataStream::Qt_5_15);

            Request type = Request::RequestCreateGroup;
            QByteArray typeArray = QByteArray::number(type);
            typeArray.resize(8);

            QByteArray byteArray = name.toUtf8();
            byteArray.prepend(typeArray);

            socketStream << byteArray;
        } else {
            QMessageBox::critical(this, "QTcpClient", "Socket doesn't seem to be opened");
        }
    } else {
        QMessageBox::critical(this, "QTcpClient", "Not connected");
    }
}

void MainWindow::sendJoinGroup() {
    bool ok;
    QString name;
    do {
        name = QInputDialog::getText(0, "Join Group", "Group name:", QLineEdit::Normal, "", &ok);
        if (!ok) {
            return;
        }
    } while (ok && name.isEmpty());

    if(socket) {
        if(socket->isOpen()) {
            QDataStream socketStream(socket);
            socketStream.setVersion(QDataStream::Qt_5_15);

            Request type = Request::RequestJoinGroup;
            QByteArray typeArray = QByteArray::number(type);
            typeArray.resize(8);

            QByteArray byteArray = name.toUtf8();
            byteArray.prepend(typeArray);

            socketStream << byteArray;
        } else {
            QMessageBox::critical(this, "QTcpClient", "Socket doesn't seem to be opened");
        }
    } else {
        QMessageBox::critical(this, "QTcpClient", "Not connected");
    }
}

void MainWindow::sendCreateFolder() {
    if (current.isEmpty() || current.value("path").toString().isEmpty()) {
        return;
    }

    bool ok;
    QString name;
    do {
        name = QInputDialog::getText(0, "Create Folder", "Folder name:", QLineEdit::Normal, "", &ok);
        if (!ok) {
            return;
        }
    } while (ok && name.isEmpty());

    QString folderPath = current.value("path").toString() + QDir::separator() + name;
    if(socket) {
        if(socket->isOpen()) {
            QDataStream socketStream(socket);
            socketStream.setVersion(QDataStream::Qt_5_15);

            Request type = Request::RequestCreateFolder;
            QByteArray typeArray = QByteArray::number(type);
            typeArray.resize(8);

            QByteArray byteArray = folderPath.toUtf8();
            byteArray.prepend(typeArray);

            socketStream << byteArray;
        } else {
            QMessageBox::critical(this, "QTcpClient", "Socket doesn't seem to be opened");
        }
    } else {
        QMessageBox::critical(this, "QTcpClient", "Not connected");
    }
}

void MainWindow::sendUpload() {
    QString filename =  QFileDialog::getOpenFileName(this, "Select File", QDir::currentPath(), "All files (*.*)");
    if (filename.isNull() || filename.isEmpty()) {
        qDebug() << (QString("sendFile: Cancel"));
        return;
    }

    QFileInfo info(filename);
    if (!info.exists()) {
        qDebug() << (QString("sendFile: file not exists"));
        return;
    }

    QFile file(info.filePath());
    if(file.open(QIODevice::ReadOnly)){
        QString fileName(info.fileName());

        QDataStream socketStream(socket);
        socketStream.setVersion(QDataStream::Qt_5_15);

        Request type = Request::RequestUploadFile;
        QByteArray typeArray = QByteArray::number(type);
        typeArray.resize(8);

        QByteArray header;
        header.prepend(QString(current.value("path").toString() + QDir::separator() + fileName).toUtf8());
        header.resize(256);

        QByteArray byteArray = file.readAll();
        byteArray.prepend(header);
        byteArray.prepend(typeArray);

        socketStream << byteArray;
    } else {
        QMessageBox::critical(this, "File Client", "File is not readable!");
    }
}

void MainWindow::sendDownload(QJsonObject object) {
    if (object.value("type").toString() != "file") {
        qDebug() << ("Download: Please select a file");
        QMessageBox::information(this, "Information", "Please select a file");
        return;
    }

    QString data = object.value("path").toString();

    if(socket) {
        if(socket->isOpen()) {
            QDataStream socketStream(socket);
            socketStream.setVersion(QDataStream::Qt_5_15);

            Request type = Request::RequestDownloadFile;
            QByteArray typeArray = QByteArray::number(type);
            typeArray.resize(8);

            QByteArray byteArray = data.toUtf8();
            byteArray.prepend(typeArray);

            socketStream << byteArray;
        } else {
            QMessageBox::critical(this, "QTcpClient", "Socket doesn't seem to be opened");
        }
    } else {
        QMessageBox::critical(this, "QTcpClient", "Not connected");
    }
}

void MainWindow::sendDelete(QJsonObject object) {
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "Delete", "Are you sure to delete this item?", QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes) {
        return;
    }

    QString data = object.value("path").toString();
    if(socket) {
        if(socket->isOpen()) {
            QDataStream socketStream(socket);
            socketStream.setVersion(QDataStream::Qt_5_15);

            Request type = Request::RequestDelete;
            QByteArray typeArray = QByteArray::number(type);
            typeArray.resize(8);

            QByteArray byteArray = data.toUtf8();
            byteArray.prepend(typeArray);

            socketStream << byteArray;
        } else {
            QMessageBox::critical(this, "QTcpClient", "Socket doesn't seem to be opened");
        }
    } else {
        QMessageBox::critical(this, "QTcpClient", "Not connected");
    }
}

void MainWindow::handleMessage(QByteArray data) {
    int responseCode = data.mid(0, 8).toInt();
    data = data.mid(8);

    switch (responseCode) {
        case ResponseNone:
            qDebug() << (QString("ResponseNone: ") + QString::fromStdString(data.toStdString()));
            break;

        case ResponseSignInSuccess:
            qDebug() << (QString("ResponseSignInSuccess: ") + QString::fromStdString(data.toStdString()));
            currentUser = ui->edtUsername->text();
            current = QJsonObject();
            ui->edtPassword->setText("");
            ui->pages->setCurrentIndex(0);
            sendGet();
            break;

        case ResponseSignInError:
            qDebug() << (QString("ResponseSignInError: ") + QString::fromStdString(data.toStdString()));
            displayError(QString::fromStdString(data.toStdString()));
            break;

        case ResponseSignUpSuccess:
            qDebug() << (QString("ResponseSignUpSuccess: ") + QString::fromStdString(data.toStdString()));
            QMessageBox::information(this, "Information", "Your sign up was successful");
            break;

        case ResponseSignUpError:
            qDebug() << (QString("ResponseSignUpError: ") + QString::fromStdString(data.toStdString()));
            displayError(QString::fromStdString(data.toStdString()));
            break;

        case ResponseSignOutSuccess:
            qDebug() << (QString("ResponseSignOutSuccess: ") + QString::fromStdString(data.toStdString()));
            currentUser = QString();
            ui->pages->setCurrentIndex(1);
            break;

        case ResponseSignOutError:
            qDebug() << (QString("ResponseSignOutError: ") + QString::fromStdString(data.toStdString()));
            displayError(QString::fromStdString(data.toStdString()));
            break;

        case ResponseGetSuccess:
            // qDebug() << (QString("ResponseGetSuccess: ") + QString::fromStdString(data.toStdString()));
            processGet(data);
            break;

        case ResponseGetError:
            qDebug() << (QString("ResponseGetError: ") + QString::fromStdString(data.toStdString()));
            displayError(QString::fromStdString(data.toStdString()));
            break;

        case ResponseCreateGroupSuccess:
            current = QJsonObject();
            processGet(data);

            qDebug() << (QString("ResponseCreateGroupSuccess: ") + QString::fromStdString(data.toStdString()));
            QMessageBox::information(this, "Success", "Create group successfully!");
            break;

        case ResponseCreateGroupError:
            qDebug() << (QString("ResponseCreateGroupError: ") + QString::fromStdString(data.toStdString()));
            displayError(QString::fromStdString(data.toStdString()));
            break;

        case ResponseJoinGroupSuccess:
            current = QJsonObject();
            processGet(data);

            qDebug() << (QString("ResponseJoinGroupSuccess: ") + QString::fromStdString(data.toStdString()));
            QMessageBox::information(this, "Success", "Join group successfully!");
            break;

        case ResponseJoinGroupError:
            qDebug() << (QString("ResponseJoinGroupError: ") + QString::fromStdString(data.toStdString()));
            displayError(QString::fromStdString(data.toStdString()));
            break;

        case ResponseCreateFolderSuccess:
            processGet(data);

            qDebug() << (QString("ResponseCreateFolderSuccess: ") + QString::fromStdString(data.toStdString()));
            QMessageBox::information(this, "Success", "Create folder successfully!");
            break;

        case ResponseCreateFolderError:
            qDebug() << (QString("ResponseCreateFolderError: ") + QString::fromStdString(data.toStdString()));
            displayError(QString::fromStdString(data.toStdString()));
            break;

        case ResponseUploadFileSuccess:
            processGet(data);

            qDebug() << (QString("ResponseUploadFileSuccess: ") + QString::fromStdString(data.toStdString()));
            QMessageBox::information(this, "Success", "Upload file successfully!");
            break;

        case ResponseUploadFileError:
            qDebug() << (QString("ResponseUploadFileError: ") + QString::fromStdString(data.toStdString()));
            displayError(QString::fromStdString(data.toStdString()));
            break;

        case ResponseDownloadFileSuccess:
            qDebug() << (QString("ResponseDownloadFileSuccess: OK"));
            processDownload(data);
            break;

        case ResponseDownloadFileError:
            qDebug() << (QString("ResponseDownloadFileError: ") + QString::fromStdString(data.toStdString()));
            displayError(QString::fromStdString(data.toStdString()));
            break;

        case ResponseDeleteSuccess:
            processGet(data);

            qDebug() << (QString("ResponseDeleteSuccess: ") + QString::fromStdString(data.toStdString()));
            QMessageBox::information(this, "Success", "Delete successfully!");
            break;

        case ResponseDeleteError:
            qDebug() << (QString("ResponseDeleteError: ") + QString::fromStdString(data.toStdString()));
            displayError(QString::fromStdString(data.toStdString()));
            break;

        default:
            break;
    }
}

void MainWindow::processGet(QByteArray data) {
    QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
    this->jsonData = jsonDoc.object();
    updateCurrent();
    updateListWidget();
}

void MainWindow::processDownload(QByteArray data) {
    QString header = data.mid(0, 128);
    data = data.mid(128);

    QStringList list = header.split(",");
    if (list.size() < 2) {
        qDebug() << ("processDownloadFile: Invalid data");
        QMessageBox::warning(this, "Download", "Invalid data");
        return;
    }

    QString filename = list[0];
    QString size = list[1];

    qDebug() << ("Download file " + filename);

    QString filePath = QFileDialog::getSaveFileName(this, tr("Save File"), QDir::currentPath() + QDir::separator() + filename);
    qDebug() << ("Download save on " + filePath);
    if (filePath.isEmpty()) {
        QMessageBox::information(this,"Download", QString("File %1 discarded.").arg(filename));
        return;
    }

    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(data);
        QString message = QString("Download file successfully stored on disk under the path %2").arg(QString(filePath));
        qDebug() << message;
    } else {
        QMessageBox::critical(this,"Download", "An error occurred while trying to write the file.");
    }
}
