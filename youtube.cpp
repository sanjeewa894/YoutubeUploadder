#include "youtube.h"
#include <QOAuthHttpServerReplyHandler>
#include <QNetworkReply>
#include <QSslSocket>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDesktopServices>
#include <QTimer>

#include <QDebug>

//static int s_unused_val = qmlRegisterType<YouTube>("sonotube", 1, 0, "YouTube");

static const QString s_scope = "https://www.googleapis.com/auth/youtube";//https://www.googleapis.com/auth/youtube.upload";

QByteArray YouTube::LoadFile(QString filename) {
    QFile file(filename);
    file.open(QIODevice::ReadOnly);
    return file.readAll();
}

YouTube::ClientID YouTube::ParseClientID(QString json_filename) {
    QFile file(json_filename);
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text)){
        emit showStatusText("Cannot open the file "+ json_filename);
        return ClientID();
    }
    QByteArray content = file.readAll();
    QJsonObject object = QJsonDocument::fromJson(content).object();
    const auto settingsObject = object["installed"].toObject();

    ClientID id;
    id.auth_uri = settingsObject["auth_uri"].toString();
    id.client_id = settingsObject["client_id"].toString();
    id.token_uri = settingsObject["token_uri"].toString();
    id.client_secret = settingsObject["client_secret"].toString();
    id.project_id = settingsObject["project_id"].toString();
    id.auth_provider_x509_cert_url = settingsObject["auth_provider_x509_cert_url"].toString();
    id.redirect_uris = settingsObject["redirect_uris"].toString();

    qInfo() <<"Client ID: "<<id.client_id;
    qInfo() << "Auth Uri"<<id.auth_uri;
    qInfo() << "Token Uri" <<id.token_uri;
    qInfo() << "Client Secret" << id.client_secret;
    emit showStatusText("Parsed client details..");
    return id;
}

YouTube::YouTube(QObject* parent) : QObject(parent) {

}

YouTube::~YouTube() {
}

QOAuth2AuthorizationCodeFlow* YouTube::InitOAuth(ClientID client, /*out,async*/QString & access_token) {
    // ensure that OpenSSL DLLs are found
    // download from https://slproweb.com/products/Win32OpenSSL.html if missing
    assert(QSslSocket::supportsSsl());

    // https://developers.google.com/identity/protocols/oauth2/scopes#youtube
    auto* auth = new QOAuth2AuthorizationCodeFlow(const_cast<YouTube*>(this));
    auth->setRequestedScopeTokens({"https://www.googleapis.com/auth/youtube"});
    //auth->setScope(s_scope);
    auth->setAuthorizationUrl(client.auth_uri);
    auth->setClientIdentifier(client.client_id);
    auth->setTokenUrl(client.token_uri);
    auth->setClientIdentifierSharedKey(client.client_secret);

    // setup local web server to receive access_token
    auto* replyHandler = new QOAuthHttpServerReplyHandler(8080, auth);
    auth->setReplyHandler(replyHandler);

    connect(replyHandler, &QOAuthHttpServerReplyHandler::tokensReceived, [&](const QVariantMap& map) {
        access_token = map["access_token"].toString(); // deferred write
        m_access_token = access_token;
        qInfo()<<m_access_token ;
        emit showStatusText("Access Token receieved");
    });

    connect(auth, &QOAuth2AuthorizationCodeFlow::granted, [&]() {
            emit authenticatioReply();
        emit showStatusText("Access granted...");
    });

    // open in default web browser
    connect(auth, &QOAuth2AuthorizationCodeFlow::authorizeWithBrowser, this, &QDesktopServices::openUrl);
    emit showStatusText("Created request for authentication...");
    //connect(auth, &QOAuth2AuthorizationCodeFlow::granted, this, &YouTube::getPlaylists);
    return auth;
}


//fileName, title, tags, description, publish
void YouTube::VideoUploadRequest(const QStringList &videoData) {
    m_video_file_content = YouTube::LoadFile(videoData.at(0));

    if(m_video_file_content.size()<1){
        qInfo()<<"File is empty";
        emit showStatusText("Uploading Video file is empty " + videoData.at(0));
        return;
    }

    QNetworkRequest request;
    {
        assert(m_api_file.length() > 0);
        assert(m_access_token.length() > 0);

        QString api_key = LoadFile(m_api_file);

        QUrl url("https://www.googleapis.com/upload/youtube/v3/videos");
        QUrlQuery query;
        query.addQueryItem("part", "snippet, status");
        query.addQueryItem("key", api_key);
        query.addQueryItem("uploadType", "resumable");
        url.setQuery(query);

        request = QNetworkRequest(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "video/*");
        request.setRawHeader("Authorization", ("Bearer " + m_access_token).toUtf8());
        emit showStatusText("Upload request created...");
    }

    QByteArray body;
    {
        QJsonArray jtags;
        foreach (auto tag , videoData.at(2).split(" "))
            jtags.push_back(QJsonValue::fromVariant(tag));

        QJsonObject snippet;
        snippet.insert("title", QJsonValue::fromVariant(videoData.at(1)));
        snippet.insert("description", QJsonValue::fromVariant(videoData.at(3)));
        snippet.insert("tags", jtags);
        //10 - Music
        snippet.insert("categoryId", QJsonValue::fromVariant(10));

        QJsonObject status;
        status.insert("privacyStatus", "private");

        status.insert("publishAt",videoData.at(4));

        QJsonObject json_arr;
        json_arr.insert("snippet", snippet);
        json_arr.insert("status", status);

        QJsonDocument doc(json_arr);
        body = doc.toJson();
        emit showStatusText("Upload body created...");
    }

    QNetworkAccessManager* downloader = new QNetworkAccessManager(this);
    connect(downloader, &QNetworkAccessManager::finished, this, &YouTube::VideoUploadData);
    downloader->post(request, body);
    emit showStatusText("Posted request to upload...");
}

void YouTube::VideoUploadData(QNetworkReply* reply) {
    auto error = reply->error();
    if (error != QNetworkReply::NoError) {
        QByteArray body = reply->readAll();
        QJsonObject object = QJsonDocument::fromJson(body).object();
        QString error = object["error"].toObject()["message"].toString();
        qInfo()<<(error);
        emit uploadStatus(1);
        emit showStatusText("Failed to upload " + error);
        return;
    }

    QString upload_url = reply->rawHeader("Location");
    QNetworkRequest request(upload_url);

    QNetworkAccessManager * uploader = new QNetworkAccessManager(this);
    connect(uploader, &QNetworkAccessManager::finished, this, &YouTube::VideoUploadResult);

    emit showStatusText("Video file is uploading .....");
    QNetworkReply *upReply = uploader->put(request, m_video_file_content);

    connect(upReply, &QNetworkReply::uploadProgress, [&](qint64 bytesSent, qint64 bytesTotal){
        if(bytesTotal>0)
            emit uploadProgress(bytesSent*100/bytesTotal);
    });
}

void YouTube::VideoUploadResult(QNetworkReply* reply) {
    QByteArray body = reply->readAll();
    QJsonObject object = QJsonDocument::fromJson(body).object();

    auto upload_error = reply->error();
    if (upload_error != QNetworkReply::NoError) {
        QString error = object["error"].toObject()["message"].toString();
        qInfo()<<(error);
        emit showStatusText("Video upload error " + error);
        emit uploadStatus(2);
    } else {
        emit uploadStatus(3);
        videoId = object["id"].toString();
        qInfo()<<videoId;
        emit showStatusText("UploadResult- Video uploaded and ID: " + videoId);

        //set the playlist and thumbnil
        if(!videoId.isEmpty())
            addVideotoPlaylist();
        else
            emit showStatusText("UploadResult - Video ID is empty " + videoId);
    }
}


void YouTube::createPlaylist(const QString &title){
    //https://www.googleapis.com/youtube/v3/playlists
    QNetworkRequest request;
    {
        assert(m_api_file.length() > 0);
        assert(m_access_token.length() > 0);

        QString api_key = LoadFile(m_api_file);

        QUrl url("https://www.googleapis.com/youtube/v3/playlists");
        QUrlQuery query;
        query.addQueryItem("part", "snippet, status");
        query.addQueryItem("key", api_key);
        // query.addQueryItem("channelId", "UCWEr-dL-go4vtYYzzdUHRrQ");

        url.setQuery(query);

        request = QNetworkRequest(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "*");
        request.setRawHeader("Authorization", ("Bearer " + m_access_token).toUtf8());
        emit showStatusText("Create request to creat playlist...");
    }

    QByteArray body;
    {
        QJsonObject snippet;
        snippet.insert("title", QJsonValue::fromVariant(title));

        QJsonObject status;
        status.insert("privacyStatus", "public");

        QJsonObject json_arr;
        json_arr.insert("snippet", snippet);
        json_arr.insert("status", status);

        QJsonDocument doc(json_arr);
        body = doc.toJson();
        emit showStatusText("Request body created...");
    }

    QNetworkAccessManager* downloader = new QNetworkAccessManager(this);
    connect(downloader, &QNetworkAccessManager::finished, [&](QNetworkReply* reply){
        //set the playlist id;
        body = reply->readAll();

        if(reply->error() == QNetworkReply::NoError){
            QJsonObject object = QJsonDocument::fromJson(body).object();
                playListID = object["id"].toString();
        }
        emit networkReply(1,reply);

    });

    emit showStatusText("Playlist creat request sent");
    downloader->post(request,body);
}

void YouTube::setPlaylistID(const QString &playlistid)
{
    playListID = playlistid;
}

void YouTube::setThumnailLocation(const QString &logo)
{
    logoLocation = logo;
}


void YouTube::getPlaylists(){
    QNetworkRequest request;
    {
        assert(m_api_file.length() > 0);
        assert(m_access_token.length() > 0);

        QString api_key = LoadFile(m_api_file);

        QUrl url("https://www.googleapis.com/youtube/v3/playlists");
        QUrlQuery query;
        query.addQueryItem("part", "snippet, id");
        query.addQueryItem("key", api_key);
        query.addQueryItem("mine", "true");
        // query.addQueryItem("channelId", "UCWEr-dL-go4vtYYzzdUHRrQ");


        url.setQuery(query);

        request = QNetworkRequest(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "*");
        request.setRawHeader("Authorization", ("Bearer " + m_access_token).toUtf8());
    }

    QNetworkAccessManager* downloader = new QNetworkAccessManager(this);
    connect(downloader, &QNetworkAccessManager::finished, [&](QNetworkReply* reply){
        emit networkReply(2,reply);
      });

     downloader->get(request);
}


void YouTube::addVideotoPlaylist(){
    //check playlist id
    if(playListID.isEmpty()){
         emit showStatusText("Playlist - Playlist ID is empty!");
        return;
    }
    if(videoId.isEmpty()){
        emit showStatusText("Playlist - Video ID is empty!");
        return;
    }


    QNetworkRequest request;
    {
        assert(m_api_file.length() > 0);
        assert(m_access_token.length() > 0);

        QString api_key = LoadFile(m_api_file);

        QUrl url("https://www.googleapis.com/youtube/v3/playlistItems");
        QUrlQuery query;
        query.addQueryItem("part", "snippet");
        query.addQueryItem("key", api_key);
        url.setQuery(query);

        request = QNetworkRequest(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "*");
        request.setRawHeader("Authorization", ("Bearer " + m_access_token).toUtf8());
        emit showStatusText("Upload request created...");
    }

    QByteArray body;
    {

        QJsonObject snippet;
        snippet.insert("playlistId", QJsonValue::fromVariant(playListID));


        QJsonObject resourceId;
        resourceId.insert("kind", "youtube#video");
        resourceId.insert("videoId", QJsonValue::fromVariant(videoId));
        snippet.insert("resourceId", resourceId);

        QJsonObject json_arr;
        json_arr.insert("snippet", snippet);

        QJsonDocument doc(json_arr);
        body = doc.toJson();
        emit showStatusText("Upload body created...");
    }

    QNetworkAccessManager* downloader = new QNetworkAccessManager(this);
    connect(downloader, &QNetworkAccessManager::finished, this, [&](QNetworkReply *reply){
        emit showStatusText("Added to playlist");
        emit networkReply(3,reply);
        if(reply->error() != QNetworkReply::NoError)
            emit uploadStatus(4);
        else
            emit uploadStatus(5);
        setThumbnail();
    });
    downloader->post(request, body);
    emit showStatusText("Posted request to upload...");
}


void YouTube::setThumbnail(){

    //check playlist id
    if(logoLocation.isEmpty()){
        emit showStatusText("Thumbnail - Logo location is empty!");
        return;
    }
    if(videoId.isEmpty()){
        emit showStatusText("Thumbnail - Video ID is empty!");
        return;
    }

    QNetworkRequest request;
    {
        assert(m_api_file.length() > 0);
        assert(m_access_token.length() > 0);

        QString api_key = LoadFile(m_api_file);

        QUrl url("https://www.googleapis.com/upload/youtube/v3/thumbnails/set");
        QUrlQuery query;
        query.addQueryItem("videoId", videoId);
        query.addQueryItem("key", api_key);
        url.setQuery(query);

        request = QNetworkRequest(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "image/jpeg");
        request.setRawHeader("Authorization", ("Bearer " + m_access_token).toUtf8());
        emit showStatusText("Thumbnail request created...");
    }

    QByteArray body;
    QFile thumb(logoLocation);
    thumb.open(QIODevice::ReadOnly);
    body = thumb.readAll();
    emit showStatusText("Thumbnail body created...");

    QNetworkAccessManager* downloader = new QNetworkAccessManager(this);
    connect(downloader, &QNetworkAccessManager::finished, this, [&](QNetworkReply *reply){
        emit showStatusText("Thumbnail uploaded and set!");
        emit networkReply(4,reply);
        if(reply->error() != QNetworkReply::NoError)
            emit uploadStatus(6);
        else
            emit uploadStatus(7);
    });
    downloader->post(request, body);
    emit showStatusText("Posted request to set Thumbnail...");
}

