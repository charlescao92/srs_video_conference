#ifndef MAINWIDGET_H
#define MAINWIDGET_H

#include <atomic>

#include <QMainWindow>
#include <QMetaType>

#include "krtc/krtc.h"
#include "YUVOpenGLWidget.h"

#include "defs.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow,
                   public krtc::KRTCEngineObserver,
                   public krtc::KRTCMsgObserver
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onStartDeviceBtnClicked();
    void onStartPreviewBtnClicked();
    void onJoinRoomBtnClicked();
    void onLeaveRoomBtnClicked();
    void onShowToast(const QString& toast, bool err);
    void onBeautyCheckBoxStateChanged(int state);
    void onBeautyDermHSliderValueChanged(int value);
    void onPushAudioCheckBoxStateChanged(int state);
    void onPushVideoCheckBoxStateChanged(int state);
    void OnPushFailedSlots(const QString& toast);
    void OnPushSuccessSlots();
    void OnCapturePureVideoFrameSlots(MediaFrameSharedPointer videoFrame);
    void OnPullVideoFrameSlots(MediaFrameSharedPointer videoFrame);
    void onParcicipantsInfoSlots(const std::map<std::string, bool>& infos);
    void onParcicipantJoinSlots(const QString& uid, bool publishing);
    void onParcicipantLeaveSlots(const QString& uid, bool publishing);

signals:
    void showToastSignal(const QString& toast, bool err);
    void networkInfoSignal(const QString& text);
    void startDeviceFailedSignal();
    void previewFailedSignal();
    void pushSuccessSignal();
    void pushFailedSignal(const QString& toast);
    void pureVideoFrameSignal(MediaFrameSharedPointer videoFrame);
    void onParcicipantsInfoSignal(const std::map<std::string, bool>& infos);
    void onParcicipantJoinSignal(const QString& uid, bool publishing);
    void onParcicipantLeaveSignal(const QString& uid, bool publishing);

private:
    Ui::MainWindow *ui;
    
    void initWindow();
    void initVideoComboBox();
    void initAudioComboBox();

    bool startDevice();
    bool stopDevice();

    bool startPreview();
    bool stopPreview();

    bool startPush(const QString& url, const QString& room_id, const QString& uid);
    bool stopPush();

    void stopPull();

    void showToast(const QString& toast, bool err);

    void leaveRoom();

private:
    // KRTCEngineObserver
    virtual void OnAudioSourceSuccess() override;
    virtual void OnAudioSourceFailed(krtc::KRTCError err) override;
    virtual void OnVideoSourceSuccess() override;
    virtual void OnVideoSourceFailed(krtc::KRTCError err) override;
    virtual void OnPreviewSuccess() override;
    virtual void OnPreviewFailed(krtc::KRTCError err) override;
    virtual void OnPushNetworkInfo(uint64_t rtt_ms, uint64_t packets_lost, double fraction_lost) override;
    virtual void OnPushSuccess() override;
    virtual void OnPushFailed(krtc::KRTCError err) override;
    virtual void OnPullSuccess() override;
    virtual void OnPullFailed(krtc::KRTCError err) override;
    virtual void OnCapturePureVideoFrame(std::shared_ptr<krtc::MediaFrame> video_frame) override;
    virtual void OnPullVideoFrame(std::shared_ptr<krtc::MediaFrame> video_frame) override;
    virtual krtc::MediaFrame* OnPreprocessVideoFrame(krtc::MediaFrame* origin_frame);

    // KRTCMsgObserver
    virtual void OnParcicipantsInfo(const std::map<std::string, bool>& infos);
    virtual void OnParcicipantJoin(const std::string& uid, bool publishing);
    virtual void OnParcicipantLeave(const std::string& uid, bool publishing);

private:
    krtc::IVideoHandler* cam_source_ = nullptr;
    krtc::IVideoHandler* desktop_source_ = nullptr;
    krtc::IAudioHandler* mic_source_ = nullptr;
    std::atomic<bool> audio_init_{ false };
    std::atomic<bool> video_init_{ false };
    krtc::IMediaHandler* krtc_preview_ = nullptr;
    krtc::IMediaHandler* krtc_pusher_ = nullptr;

    bool enable_beauty_ = false;
    int beauty_index_ = 20;

    std::map<std::string, krtc::IMediaHandler*> krtc_pullers_;
    std::map<int, std::string> pullWidgetInfo; // <拉流窗口索引，uid>

    int widget_index_ = 1;

};

#endif // MAINWINDOW_H
