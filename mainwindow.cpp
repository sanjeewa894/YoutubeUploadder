#include "mainwindow.h"
#include "qjsonarray.h"
#include "ui_mainwindow.h"

#include <QtNetwork/QNetworkAccessManager>
#include <QJsonObject>
#include <QtNetwork/QNetworkReply>

#include <QOAuth2AuthorizationCodeFlow>
#include <QUrl>
#include <QTableView>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connect(&youtube, &YouTube::networkReply,this, &MainWindow::apiReply);
    connect(&youtube, &YouTube::authenticatioReply,this, &MainWindow::authenticReply);
    connect(&youtube, &YouTube::uploadStatus,this, &MainWindow::uploadingStatus);
    connect(&youtube, &YouTube::uploadProgress,[&](uint8_t progress){
        ui->progressBar->setValue(progress);
    });
    connect(&youtube, &YouTube::showStatusText,[&](const QString &msg){
       setPlainText(msg);
    });

    model = new QStandardItemModel(0, 4,this);
    horizontalHeader.append("File Name");
    horizontalHeader.append("Video Title");
    horizontalHeader.append("Tags");
    horizontalHeader.append("Description");
    model->setHorizontalHeaderLabels(horizontalHeader);
    model->setVerticalHeaderLabels(verticalHeader);
    ui->tableView->setModel(model);

    //today time
    date = dateTime.currentDateTime().date().addDays(1);
    midnight = {5, 0, 0};// Set the time to 00:00:00
    ui->uploadCount->setText(QString::number(uploadIndex));

}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::apiReply(int pos, QNetworkReply *reply){
    QNetworkReply::NetworkError upload_error;
    QByteArray body;
    QJsonObject object;

    switch(pos){
        case 1:
            //playlist creation
            upload_error = reply->error();
            qInfo()<<upload_error;
            //call upload
            if(upload_error == QNetworkReply::NetworkError::NoError){
                setPlainText("Uploading Next Video");
                nextVideoUpload();
            }
            break;
        case 2:
            body = reply->readAll();
            object = QJsonDocument::fromJson(body).object();
            qDebug()<<reply->error();
            setPlainText("Playlist read error: " + QString::number(reply->error()));
            if(reply->error() == QNetworkReply::NoError){
                playListIds.clear();
                playListTitles.clear();
                QJsonArray items = object["items"].toArray();
                foreach (QJsonValue item, items) {
                    QString id = item["id"].toString();
                    QJsonObject snipet = item["snippet"].toObject();
                    QString pTitle = snipet["title"].toString();
                    qDebug()<<id<<pTitle;
                    playListIds.append(id);
                    playListTitles.append(pTitle);
                }
                setPlainText("Received playlists: "+QString::number(playListTitles.size()));
                ui->availablePlaylists->addItems(playListTitles);
            }
            break;
        case 3:
            //playlist item set
            body = reply->readAll();
            object = QJsonDocument::fromJson(body).object();
            qDebug()<<reply->error();
            break;
        case 4:
            //thumbnail set
            body = reply->readAll();
            object = QJsonDocument::fromJson(body).object();
            qDebug()<<reply->error();
            break;
    }
}

//after granting the access
void MainWindow::authenticReply(){
    authenticated = true;
    ui->auther->setText("Granted");
    setPlainText("Authentication Success!");
    youtube.getPlaylists();
}


void MainWindow::uploadingStatus(int state){
    switch(state){
        case 1:
            ui->statusbar->showMessage("Failed to upload");
            break;
        case 2:
            ui->statusbar->showMessage("Video upload error");
            break;
        case 3:
            ui->statusbar->showMessage("Video upload succeeded!!!");
            break;
        case 4:
            ui->statusbar->showMessage("Failed adding to playlist");
            break;
        case 5:
            ui->statusbar->showMessage("Succeeded adding to playlist!!!");
            break;
        case 6:
            ui->statusbar->showMessage("Failed to set thumbnail");
            break;
        case 7:
            ui->statusbar->showMessage("Successfully set thumbnail!!!");
            //go to next upload
            nextVideoUpload();
            break;
        case 8:
            break;
    }
}

/*
 * Create form to request access token from Google's OAuth 2.0 server.
 */
void MainWindow::oauthSignIn() {
    if(ui->auther->text() == "Authenticate"){
        YouTube::ClientID   client = youtube.ParseClientID(":/clientInfo.json");
        if(client.client_id.isEmpty()){
            setPlainText("Failed to parse the client details...");
            return;
        }

        youtube.m_api_file = ":/apikey";
        setPlainText("initializing authentication");
        QOAuth2AuthorizationCodeFlow* auth = youtube.InitOAuth(client,access_token);
        auth->grant();
    }else{
        msgBox.setText("Already connected!");
        msgBox.exec();
        setPlainText("Already authenticated");
    }
    qInfo()<<access_token;
}


void MainWindow::on_auther_released()
{
    oauthSignIn();
}


void MainWindow::on_loadFiles_released()
{

    if(ui->tagsDesc->text().isEmpty()){
        setPlainText("Set Tags before load");
        msgBox.setText("Set Tags before load!");
        msgBox.exec();
        return;
    }
    QFileDialog flDiag;
    fileDirectory = flDiag.getExistingDirectory(this);

    setPlainText("Loading files at " + fileDirectory);
    //check files are available
    QDir dirObj(fileDirectory);
    fileList = dirObj.entryList(QStringList() << "*.mp4" , QDir::Files);
    qInfo() << fileList.size();
    setPlainText("Number of video files found: " + QString::number(fileList.size()));

    model->clear();

    model->index(1,1,model->index(0,0));
    model->setHorizontalHeaderLabels(horizontalHeader);
    model->setVerticalHeaderLabels(verticalHeader);
    int i=0;
    foreach(auto &item, fileList){
        QStandardItem *item1,*item2,*item3,*item4;
        item1 = new QStandardItem(item);
        item2 = new QStandardItem((item.split(".")[1]));
        //set tags
        QString tags="";
        QStringList tmp = ((item.split(".")[1]).split("with")[0]).split("~");
        if(tmp.size()>1){
            QStringList sName = tmp[0].split(" ");
            QStringList aName = tmp[1].split(" ");
            if(sName.size()>1){
                //sdfsd sdfs sdfsd
                tags = sName.join("").trimmed();
                tags += " " + tmp[0].trimmed();
            }else{
                tags = sName[0].trimmed();
            }
            if(aName[0] =="")
                aName.removeAt(0);
            //artist name
            if(aName.size()>1){
                //sdfsd sdfs sdfsd
                tags += " " + aName.join("").trimmed();
                tags += " " + tmp[1].trimmed();
            }else{
                tags += " " + aName[0].trimmed();
            }
        }

        tags += " " + ui->tagsDesc->text().trimmed();
        item3 = new QStandardItem(tags);
        QStringList desc = tags.split(" ");
        item4 = new QStandardItem("#"+desc.join(" #"));

        model->appendRow(item1);
        model->setItem(i,1, item2);
        model->setItem(i,2, item3);
        model->setItem(i,3, item4);
        ++i;
    }

    ui->tableView->resizeRowsToContents();
    // ui->tableView->resizeColumnsToContents();
    ui->tableView->setColumnWidth(0,this->width()/8);
    ui->tableView->setColumnWidth(1,this->width()/4);
    ui->tableView->setColumnWidth(2,this->width()/4);
    ui->tableView->setColumnWidth(3,this->width()/4);
    setPlainText("Files loaded");

}


void MainWindow::resizeEvent(QResizeEvent *event) {
    ui->tableView->setColumnWidth(0,this->width()/8);
    ui->tableView->setColumnWidth(1,this->width()/4);
    ui->tableView->setColumnWidth(2,this->width()/4);
    ui->tableView->setColumnWidth(3,this->width()/4);

    QMainWindow::resizeEvent(event);
}

void MainWindow::on_uploadFiles_released()
{
    if(!authenticated){
        setPlainText("Not Authenticated");
        msgBox.setText("Authyenticate first!");
        msgBox.exec();
        return;
    }
    if(fileList.size()<1){
        setPlainText("Not file found");
        msgBox.setText("No file found");
        msgBox.exec();
        return;
    }

    if(thumbnail.size()<1){
        setPlainText("Not Thumbnail found");
        msgBox.setText("No Thumbnail found");
        msgBox.exec();
        return;
    }


    titles.clear();
    tags.clear();
    description.clear();
    for(int i=0;i<model->rowCount();++i){
        titles.append((model->data(model->index(i, 1))).toString());
        if((model->data(model->index(i, 1))).toString().length()<1){
            setPlainText("Title is empty at "+ QString::number(i));
            msgBox.setText("Title is empty at "+ QString::number(i));
            msgBox.exec();
            return;
        }
        tags.append((model->data(model->index(i, 2))).toString());
        description.append((model->data(model->index(i, 3))).toString());
    }
    if(titles.size()<1 || titles.size() != fileList.size()){
        setPlainText("Titles are empty!");
        msgBox.setText("Titles are empty");
        msgBox.exec();
        return;
    }
    QString playList = ui->playlistName->text().trimmed();
    if(playList.isEmpty()){
        setPlainText("Playlist is empty!");
        msgBox.setText("Playlist is empty!");
        msgBox.exec();
        return;
    }



    bool createNewPlaylist = ui->checkBox->isChecked();
    //qDebug()<<fileList<<titles<<tags<<description;
    if(createNewPlaylist){
        //create playlist
        youtube.createPlaylist(ui->playlistName->text().trimmed());
        ui->statusbar->showMessage("creating playlist",100000);
        setPlainText("Playlist is creating...");
    }
    else{
        //call update
        setPlainText("Uploading next in the line...");
        nextVideoUpload();
    }
}


void MainWindow::nextVideoUpload(){
    if(uploadIndex >= fileList.size()){
        setPlainText("No files to upload");
        msgBox.setText("No file to upload!");
        msgBox.exec();
        return;
    }
    ui->statusbar->showMessage("Uploading video file "+ fileList.at(uploadIndex));
    setPlainText("Uploading video file " + fileList.at(uploadIndex));
    // Combine the date and time
    QTime addTime = midnight.addSecs(300*uploadIndex);
    QDateTime dateTime(date,addTime);
    // Format the date and time in ISO 8601 format
    QString isoDateTime = dateTime.toString(Qt::ISODate);

    //fileName, title, tags, description, publish
    QStringList nextItem;
    nextItem.append(fileDirectory+ "/" + fileList.at(uploadIndex));
    nextItem.append(titles.at(uploadIndex));
    nextItem.append(tags.at(uploadIndex));
    nextItem.append(description.at(uploadIndex));
    nextItem.append(isoDateTime);
    youtube.VideoUploadRequest(nextItem);
    ++uploadIndex;
    ui->uploadCount->setText(QString::number(uploadIndex));
}

void MainWindow::setPlainText(const QString &msg){
    ui->plainTextEdit->appendPlainText(msg);

}

void MainWindow::on_availablePlaylists_currentIndexChanged(int index)
{
    if(!ui->availablePlaylists->itemText(index).isEmpty()){
        ui->playlistName->setText(ui->availablePlaylists->itemText(index));
        youtube.setPlaylistID(playListIds.at(index));
    }
}


void MainWindow::on_thumbnail_released()
{
    thumbnail = QFileDialog::getOpenFileName();
    youtube.setThumnailLocation(thumbnail);
    setPlainText("Selected Thumbnail: "+ thumbnail);
}

