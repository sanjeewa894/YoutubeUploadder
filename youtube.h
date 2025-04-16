#ifndef YOUTUBE_H
#define YOUTUBE_H

#include <QObject>
#include <QOAuth2AuthorizationCodeFlow>
#include <QDateTime>

/** YouTube video upload class.
    Require API key & OAuth 2.0 client ID from https://console.developers.google.com/apis/credentials */
class YouTube : public QObject {
    Q_OBJECT

public:
    YouTube(QObject* parent = nullptr);
    ~YouTube() override;

    struct ClientID {
        QString auth_uri;
        QString project_id;
        QString auth_provider_x509_cert_url;
        QString client_id;
        QString token_uri;
        QString client_secret;
        QString redirect_uris;
    };


    bool  m_limited_auth = false; ///< limited-input device authentication with login on secondary device (typ. mobile phone)
    QString m_api_file;             ///< file-path to text file containing API key
    QString m_client_file;          ///< file-path to JSON file containing client secrets

    QString m_title;       ///< video title
    QString m_description; ///< video descriptions
         ///<
    QString m_category;    ///< see https://developers.google.com/youtube/v3/docs/videoCategories/list

    QByteArray LoadFile(QString filename);

    /** Parse OAuth 2.0 Client ID JSON file */
    ClientID ParseClientID(QString json_filename);

    QOAuth2AuthorizationCodeFlow* InitOAuth(ClientID client, /*out,async*/QString& access_token);

    void getPlaylists();
    void createPlaylist(const QString &title);
    void setPlaylistID(const QString &playlistid);
    void setThumnailLocation(const QString &logo);

    void VideoUploadRequest(const QStringList &videoData);


    void addVideotoPlaylist();
    void setThumbnail();

private:
    QString videoId;

    /** https://developers.google.com/youtube/v3/docs/videos/insert */
                   ///< step 1
    void VideoUploadData(QNetworkReply* reply);    ///< step 2
    void VideoUploadResult(QNetworkReply* reply);  ///< step 3


    QByteArray m_video_file_content;
    QString  m_access_token;
    QString  m_device_code;       ///< for limited-input auth
    int  m_poll_interval = 0; ///< for limited-input auth
    QString playListID;
    QString logoLocation;
signals:
    void networkReply(int pos, QNetworkReply *);
    void authenticatioReply();
    void uploadStatus(int state);
    void uploadProgress(uint8_t state);
    void showStatusText(const QString &txt);

private slots:
};


#endif // YOUTUBE_H
