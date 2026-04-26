#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "youtube.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QDir>
#include <QStandardItemModel>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void apiReply(int pos, QNetworkReply *);
    void authenticReply();
    void on_auther_released();
    void uploadingStatus(int state);


    void on_loadFiles_released();

    void resizeEvent(QResizeEvent *event);
    void on_uploadFiles_released();

    void on_availablePlaylists_currentIndexChanged(int index);

    void on_thumbnail_released();

private:
    Ui::MainWindow *ui;
    bool authenticated = false;
    int uploadIndex=0;
    QString access_token;
    std::unique_ptr<YouTube> youtube = nullptr;
    QString thumbnail;
    QMessageBox msgBox;
    void oauthSignIn();

    QStandardItemModel *model;
    QStringList horizontalHeader;
    QStringList verticalHeader;
    QStringList fileList, titles, tags, description;
    QString fileDirectory;
    QStringList playListIds;
    QStringList playListTitles;

    QDateTime dateTime;
    //today time
    QDate date;
    // Set the time to 00:00:00
    QTime midnight;
    void nextVideoUpload();
    void setPlainText(const QString &msg);

    QString m_access_token;
    void initYoutubeConnection();
};
#endif // MAINWINDOW_H
