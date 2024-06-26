#include "itemfile.h"
#include "ui_itemfile.h"

#include <QPixmap>
#include <QJsonValue>
#include <QJsonArray>

ItemFile::ItemFile(QWidget* parent) : QWidget(parent), ui(new Ui::ItemFile) {
    ui->setupUi(this);
}

ItemFile::~ItemFile() {
    delete ui;
}

void ItemFile::setData(const QJsonObject& data) {
    this->data = data;

    QJsonValue name = data.value("name");
    ui->lbName->setText(name.toString());

    QJsonValue type = data.value("type");

    QPixmap pic(":images/folder.png");
    if (type == "file") {
        pic = QPixmap(":images/file.png");
    } else {
        QJsonArray children = data.value("children").toArray();
        if (children.isEmpty()) {
            pic= QPixmap(":images/folder_empty.png");
        }
    }

    ui->icon->setPixmap(pic);
}

QJsonObject ItemFile::getData() const {
    return data;
}
