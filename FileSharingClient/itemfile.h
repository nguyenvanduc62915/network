#ifndef ITEMFILE_H
#define ITEMFILE_H

#include <QWidget>
#include <QJsonObject>

namespace Ui {
    class ItemFile;
}

class ItemFile : public QWidget {
    Q_OBJECT

public:
    explicit ItemFile(QWidget* parent = nullptr);
    ~ItemFile();

    void setData(const QJsonObject& data);
    QJsonObject getData() const;

private:
    Ui::ItemFile* ui;

    QJsonObject data;
};

#endif // ITEMFILE_H
