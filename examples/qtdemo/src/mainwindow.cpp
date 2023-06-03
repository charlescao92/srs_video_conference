#include <map>

#include <QVariant>

#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <opencv2/opencv.hpp>
#include <opencv2/imgproc/imgproc.hpp>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    initWindow();
}

MainWindow::~MainWindow()
{
	stopPush();
	stopPreview();
	stopDevice();
    delete ui;
}

void MainWindow::initWindow()
{
	setWindowTitle(QStringLiteral("srs多人通话—qtdemo"));
    setMinimumSize(1300, 800);
	
	ui->leftWidget->setFixedWidth(250);

	ui->serverLineEdit->setText("charlescao92.cn");
	ui->leaveRoomBtn->setEnabled(false);

	ui->roomIdLineEdit->setText("55555");
    ui->uidLineEdit->setText("111");

	ui->networkQosLabel->setText("");
	ui->statusLabel->setText("");
	ui->cameraRadioButton->setChecked(true);

	ui->beautyDermHSlider->setRange(1, 40);
	ui->beautyDermHSlider->setSingleStep(10);
	ui->beautyDermHSlider->setValue(20);
	ui->beautyDermHSlider->setEnabled(false);

	ui->pushAudioCheckBox->setChecked(true);
	ui->pushVideoCheckBox->setChecked(true);

    connect(ui->startDeviceBtn, &QPushButton::clicked,
        this, &MainWindow::onStartDeviceBtnClicked);
    connect(ui->startPreviewBtn, &QPushButton::clicked,
        this, &MainWindow::onStartPreviewBtnClicked);
    connect(ui->joinRoomBtn, &QPushButton::clicked,
        this, &MainWindow::onJoinRoomBtnClicked);
    connect(ui->leaveRoomBtn, &QPushButton::clicked,
        this, &MainWindow::onLeaveRoomBtnClicked);
	connect(this, &MainWindow::showToastSignal,
		this, &MainWindow::onShowToast, Qt::QueuedConnection);
	connect(this, &MainWindow::pureVideoFrameSignal,
		this, &MainWindow::OnCapturePureVideoFrameSlots, Qt::QueuedConnection);
	connect(this, &MainWindow::pushFailedSignal,
		this, &MainWindow::OnPushFailedSlots, Qt::QueuedConnection);
	connect(this, &MainWindow::pushSuccessSignal,
		this, &MainWindow::OnPushSuccessSlots, Qt::QueuedConnection);

	connect(this, &MainWindow::onParcicipantsInfoSignal,
		this, &MainWindow::onParcicipantsInfoSlots);
	connect(this, &MainWindow::onParcicipantJoinSignal,
		this, &MainWindow::onParcicipantJoinSlots);
	connect(this, &MainWindow::onParcicipantLeaveSignal,
		this, &MainWindow::onParcicipantLeaveSlots);
	
	connect(this, &MainWindow::networkInfoSignal,
		[this](const QString& text) {
			ui->networkQosLabel->setText(text);
	});
	connect(this, &MainWindow::startDeviceFailedSignal,
		this, [this] {
			stopDevice();
			ui->startDeviceBtn->setText(QStringLiteral("启动音视频设备"));
		});
	connect(this, &MainWindow::previewFailedSignal,
		[this] {
			stopPreview();
			ui->startPreviewBtn->setText(QStringLiteral("本地预览"));
		});

	connect(ui->beautyCheckBox, SIGNAL(stateChanged(int)),
		this, SLOT(onBeautyCheckBoxStateChanged(int)));
	connect(ui->beautyDermHSlider, SIGNAL(valueChanged(int)),
		this, SLOT(onBeautyDermHSliderValueChanged(int)));
	connect(ui->pushAudioCheckBox, SIGNAL(stateChanged(int)),
		this, SLOT(onPushAudioCheckBoxStateChanged(int)));
	connect(ui->pushVideoCheckBox, SIGNAL(stateChanged(int)),
		this, SLOT(onPushVideoCheckBoxStateChanged(int)));

	krtc::KRTCEngine::Init(this, this);

	initVideoComboBox();
	initAudioComboBox();

	qRegisterMetaType<MediaFrameSharedPointer>("MediaFrameSharedPointer");
	qRegisterMetaType<std::map<std::string, bool>>("std::map<std::string, bool>");
}

void MainWindow::initVideoComboBox()
{
	int total = krtc::KRTCEngine::GetCameraCount();
	if (total <= 0) {
		return;
	}

	for (int i = 0; i < total; ++i) {
		char device_name[128] = { 0 };
		char device_id[128] = { 0 };
        krtc::KRTCEngine::GetCameraInfo(i, device_name, sizeof(device_name), device_id, sizeof(device_id));

		QVariant useVar;
		DeviceInfo info = { device_name, device_id };
		useVar.setValue(info);
		ui->videoDeviceCbx->addItem(QString::fromStdString(device_name), useVar);
	}
}

void MainWindow::initAudioComboBox()
{
	int total = krtc::KRTCEngine::GetMicCount();
	if (total <= 0) {
		return;
	}

	for (int i = 0; i < total; ++i) {
		char device_name[128] = { 0 };
		char device_id[128] = { 0 };
        krtc::KRTCEngine::GetMicInfo(i, device_name, sizeof(device_name), device_id, sizeof(device_id));

		QVariant useVar;
		DeviceInfo info = { device_name, device_id };
		useVar.setValue(info);
		ui->audioDeviceCbx->addItem(QString::fromStdString(device_name), useVar);
	}
}

void MainWindow::onStartDeviceBtnClicked()
{
	ui->startDeviceBtn->setEnabled(false);
	if (!audio_init_ || !video_init_) {
		if (startDevice()) {
			ui->startDeviceBtn->setText(QStringLiteral("停止音视频设备"));
		}
	}
	else {
		if (stopDevice()) {
			ui->startDeviceBtn->setText(QStringLiteral("启动音视频设备"));
		}
	}
	ui->startDeviceBtn->setEnabled(true);
}

void MainWindow::onStartPreviewBtnClicked()
{
	ui->startPreviewBtn->setEnabled(false);

	if (!krtc_preview_) {
		if (startPreview()) {
			ui->startPreviewBtn->setText(QStringLiteral("停止预览"));
		}
	}
	else {
		if (stopPreview()) {
			ui->startPreviewBtn->setText(QStringLiteral("本地预览"));
		}
	}

	ui->startPreviewBtn->setEnabled(true);
}

void MainWindow::onJoinRoomBtnClicked()
{
	ui->joinRoomBtn->setEnabled(false);
	QString server_addr = ui->serverLineEdit->text();
	QString room_id = ui->roomIdLineEdit->text();
	QString uid = ui->uidLineEdit->text();
	bool success = krtc::KRTCEngine::JoinRoom(server_addr.toStdString(),
		room_id.toStdString(),
		uid.toStdString());
	if (!success) {
		ui->joinRoomBtn->setEnabled(true);
		return;
	}

	if (!krtc_pusher_) {
		if (!startPush(server_addr, room_id, uid)) {
			leaveRoom();
			ui->joinRoomBtn->setEnabled(true);
		}
	}

}

void MainWindow::onLeaveRoomBtnClicked()
{
	leaveRoom();
	ui->joinRoomBtn->setEnabled(true);
}

void MainWindow::onShowToast(const QString& toast, bool err)
{
	if (err) {
		ui->statusLabel->setStyleSheet("color:red;");
	}
	else {
		ui->statusLabel->setStyleSheet("color:blue;");
	}
	ui->statusLabel->setText(toast);
}

void MainWindow::onBeautyCheckBoxStateChanged(int state)
{
	if (state == Qt::CheckState::Checked)
	{
		enable_beauty_ = true;
		ui->beautyDermHSlider->setEnabled(true);
	}
	else
	{
		enable_beauty_ = false;
		ui->beautyDermHSlider->setEnabled(false);
	}

}

void MainWindow::onBeautyDermHSliderValueChanged(int value)
{
	beauty_index_ = value;
}

void MainWindow::onPushAudioCheckBoxStateChanged(int state)
{
	if (!krtc_pusher_) {
		return;
	}
	if (state == Qt::CheckState::Checked)
	{
		krtc_pusher_->SetEnableAudio(true);
	}
	else
	{
		krtc_pusher_->SetEnableAudio(false);
	}
}

void MainWindow::onPushVideoCheckBoxStateChanged(int state)
{
	if (!krtc_pusher_) {
		return;
	}
	if (state == Qt::CheckState::Checked)
	{
		krtc_pusher_->SetEnableVideo(true);
	}
	else
	{
		krtc_pusher_->SetEnableVideo(false);
	}
}

void MainWindow::OnPushFailedSlots(const QString& toast)
{
	leaveRoom();
	stopPush();
	ui->joinRoomBtn->setEnabled(true);
	onShowToast(toast, true);
}

void MainWindow::OnPushSuccessSlots()
{
	ui->leaveRoomBtn->setEnabled(true);
	onShowToast(QStringLiteral("推流成功"), true);
}

void MainWindow::OnCapturePureVideoFrameSlots(MediaFrameSharedPointer videoFrame)
{
	ui->previewOpenGLWidget->updateFrame(videoFrame);
}

void MainWindow::OnPullVideoFrameSlots(MediaFrameSharedPointer videoFrame)
{
}

void MainWindow::onParcicipantsInfoSlots(const std::map<std::string, bool>& infos)
{
	// 当前简单显示拉流
	// TODO 手动新建窗口来添加删除、opengl渲染

	QString room_id = ui->roomIdLineEdit->text();
	QString server_addr = ui->serverLineEdit->text();

	for (auto iter = infos.begin(); iter != infos.end(); ++iter) {
		if (!iter->second) {
			continue;
		}

		if (widget_index_ > 5) {
			printf("显示窗口已经满了");
			return;
		}

		for (auto iter1 = pullWidgetInfo.begin(); iter1 != pullWidgetInfo.end(); ++iter1) {
			if (iter1->second == iter->first) {
				continue;
			}
		}

		QString channel = room_id + "/" + QString::fromStdString(iter->first);
		QString widget_name = "widget_" + QString::number(widget_index_);
		QWidget* pull_widget = findChild<QWidget*>(widget_name);
		if (pull_widget) {
			krtc::IMediaHandler* puller = krtc::KRTCEngine::CreatePuller(
				server_addr.toUtf8().constData(),
				channel.toUtf8().constData(),
				pull_widget->winId());

			pullWidgetInfo[widget_index_] = iter->first;
			krtc_pullers_[iter->first] = puller;

			puller->Start();
		}

		++widget_index_;
	}

}

void MainWindow::onParcicipantJoinSlots(const QString& uid, bool publishing)
{
	if (!publishing) {
		return;
	}

	QString room_id = ui->roomIdLineEdit->text();
	QString server_addr = ui->serverLineEdit->text();

	QString channel = room_id + "/" + uid;
	QString widget_name = "widget_" + widget_index_;
	QWidget* pull_widget = findChild<QWidget*>(widget_name);
	if (pull_widget) {
		krtc::IMediaHandler* puller = krtc::KRTCEngine::CreatePuller(
			server_addr.toUtf8().constData(),
			channel.toUtf8().constData(),
			pull_widget->winId());

		pullWidgetInfo[widget_index_] = uid.toStdString();
		krtc_pullers_[uid.toStdString()] = puller;

		puller->Start();
	}

	++widget_index_;
}

void MainWindow::onParcicipantLeaveSlots(const QString& uid, bool publishing)
{
	if (!publishing) {
		return;
	}

	for (auto iter = pullWidgetInfo.begin(); iter != pullWidgetInfo.end(); ++iter) {
		if (iter->second == uid.toStdString()) {
			pullWidgetInfo.erase(iter);
			break;
		}
	}

	auto iter = krtc_pullers_.find(uid.toStdString());
	if (iter != krtc_pullers_.end()) {
		krtc_pullers_.erase(iter);
	}

	--widget_index_; // 当前只是按后加入通话的人先离开的来测试，TODO支持随时加入的人离开
}


bool MainWindow::startDevice() {
	if (!video_init_) {
		if (ui->cameraRadioButton->isChecked()) {
			int index = ui->videoDeviceCbx->currentIndex();
			QVariant useVar = ui->videoDeviceCbx->itemData(index);
			std::string device_id = useVar.value<DeviceInfo>().device_id;
			cam_source_ = krtc::KRTCEngine::CreateCameraSource(device_id.c_str());
			cam_source_->Start();
		}
		else if (ui->desktopRadioButton->isChecked()) {
			desktop_source_ = krtc::KRTCEngine::CreateScreenSource();
			desktop_source_->Start();
		}
		
	}

	if (!audio_init_) {
		int index = ui->audioDeviceCbx->currentIndex();
		QVariant useVar = ui->audioDeviceCbx->itemData(index);

		std::string device_id = useVar.value<DeviceInfo>().device_id;
		mic_source_ = krtc::KRTCEngine::CreateMicSource(device_id.c_str());
		mic_source_->Start();
	}

	return true;
}

bool MainWindow::stopDevice() {
	if (video_init_ && cam_source_) {
		cam_source_->Stop();
		cam_source_->Destroy();
		cam_source_ = nullptr;
		video_init_ = false;
	}

	if (video_init_ && desktop_source_) {
		desktop_source_->Stop();
		desktop_source_->Destroy();
		desktop_source_ = nullptr;
		video_init_ = false;
	}

	if (audio_init_ && mic_source_) {
		mic_source_->Stop();
		mic_source_->Destroy();
		mic_source_ = nullptr;
		audio_init_ = false;
	}

	return true;
}

bool MainWindow::startPreview() {
	if (!cam_source_ && !desktop_source_) {
		showToast(QStringLiteral("预览失败：没有视频源"), true);
		return false;
	}

    krtc_preview_ = krtc::KRTCEngine::CreatePreview();
	krtc_preview_->Start();

	return true;
}

bool MainWindow::stopPreview() {
	if (!krtc_preview_) {
		return false;
	}

	krtc_preview_->Stop();
	krtc_preview_ = nullptr;

	showToast(QStringLiteral("停止本地预览成功"), false);

	return true;
}

bool MainWindow::startPush(const QString& url, const QString& room_id, const QString& uid)
{
	if (!cam_source_ && !desktop_source_) {
		showToast(QStringLiteral("推流失败：没有视频源"), true);
		return false;
	}

	QString channel = room_id + "/" + uid;
	krtc_pusher_ = krtc::KRTCEngine::CreatePusher(url.toUtf8().constData(), 
		channel.toUtf8().constData());
	krtc_pusher_->Start();

	return true;
}

bool MainWindow::stopPush() {
	if (!krtc_pusher_) {
		return true;
	}

	krtc_pusher_->Stop();
	krtc_pusher_->Destroy();
	krtc_pusher_ = nullptr;

	return true;
}

void MainWindow::stopPull()
{
	for (auto iter = krtc_pullers_.begin(); iter != krtc_pullers_.end();) {
		iter->second->Stop();
		iter->second->Destroy();
		krtc_pullers_.erase(iter++);
	}
	widget_index_ = 1;
}

void MainWindow::showToast(const QString& toast, bool err)
{
	emit showToastSignal(toast, err);
}

void MainWindow::leaveRoom()
{
	ui->leaveRoomBtn->setEnabled(false);
	krtc::KRTCEngine::LeaveRoom();
	stopPush();
	stopPull();
}

void MainWindow::OnAudioSourceSuccess() {
	audio_init_ = true;
	showToast(QStringLiteral("麦克风启动成功"), false);
}

void MainWindow::OnAudioSourceFailed(krtc::KRTCError err)
{
	QString err_msg = QStringLiteral("麦克风启动失败: ") + QString::fromStdString(krtc::KRTCEngine::GetErrString(err));
	showToast(err_msg, true);

	emit startDeviceFailedSignal();
}

void MainWindow::OnVideoSourceSuccess() {
	video_init_ = true;
	showToast(QStringLiteral("摄像头启动成功"), false);
}

void MainWindow::OnVideoSourceFailed(krtc::KRTCError err)
{
	QString err_msg = QStringLiteral("摄像头启动失败: ") + QString::fromStdString(krtc::KRTCEngine::GetErrString(err));
	showToast(err_msg, true);
	emit startDeviceFailedSignal();
}

void MainWindow::OnPreviewSuccess() {
	showToast(QStringLiteral("本地预览成功"), false);
}

void MainWindow::OnPreviewFailed(krtc::KRTCError err) {
	QString err_msg = QStringLiteral("本地预览失败: ") + QString::fromStdString(krtc::KRTCEngine::GetErrString(err));
	showToast(err_msg, true);

	if (krtc_preview_) {
		krtc_preview_->Stop();
		krtc_preview_ = nullptr;
	}

	emit previewFailedSignal();

}

void MainWindow::OnPushSuccess() {
	emit pushSuccessSignal();
}

void MainWindow::OnPushFailed(krtc::KRTCError err) {
	QString err_msg = QStringLiteral("推流失败: ") +
		QString::fromStdString(krtc::KRTCEngine::GetErrString(err));
	emit pushFailedSignal(err_msg);
}

void MainWindow::OnPullSuccess() {
	showToast(QStringLiteral("拉流成功"), false);
}

void MainWindow::OnPullFailed(krtc::KRTCError err) {
	QString err_msg = QStringLiteral("拉流失败: ") + QString::fromStdString(krtc::KRTCEngine::GetErrString(err));
	showToast(err_msg, true);

}

void MainWindow::OnCapturePureVideoFrame(std::shared_ptr<krtc::MediaFrame> frame)
{
	int src_width = frame->fmt.sub_fmt.video_fmt.width;
	int src_height = frame->fmt.sub_fmt.video_fmt.height;
	int stridey = frame->stride[0];
	int strideu = frame->stride[1];
	int stridev = frame->stride[2];
	int size = stridey * src_height + (strideu + stridev) * ((src_height + 1) / 2);

	MediaFrameSharedPointer video_frame(new krtc::MediaFrame(size));
	video_frame->fmt.sub_fmt.video_fmt.type = krtc::SubMediaType::kSubTypeI420;
	video_frame->fmt.sub_fmt.video_fmt.width = src_width;
	video_frame->fmt.sub_fmt.video_fmt.height = src_height;
	video_frame->stride[0] = stridey;
	video_frame->stride[1] = strideu;
	video_frame->stride[2] = stridev;
	video_frame->data_len[0] = stridey * src_height;
	video_frame->data_len[1] = strideu * ((src_height + 1) / 2);
	video_frame->data_len[2] = stridev * ((src_height + 1) / 2);
	video_frame->data[0] = new char[video_frame->data_len[0]];
	video_frame->data[1] = new char[video_frame->data_len[1]];
	video_frame->data[2] = new char[video_frame->data_len[2]];
	memcpy(video_frame->data[0], frame->data[0], frame->data_len[0]);
	memcpy(video_frame->data[1], frame->data[1], frame->data_len[1]);
	memcpy(video_frame->data[2], frame->data[2], frame->data_len[2]);

	emit pureVideoFrameSignal(video_frame);
}

void MainWindow::OnPullVideoFrame(std::shared_ptr<krtc::MediaFrame> frame)
{
}

static void CopyYUVToImage(uchar* dst, uint8_t* pY, uint8_t* pU, uint8_t* pV, int width, int height)
{
	uint32_t size = width * height;
	memcpy(dst, pY, size);
	memcpy(dst + size, pU, size / 4);
	memcpy(dst + size + size / 4, pV, size / 4);
}

static void CopyImageToYUV(uint8_t* pY, uint8_t* pU, uint8_t* pV, uchar* src, int width, int height)
{
	uint32_t size = width * height;
	memcpy(pY, src, size);
	memcpy(pU, src + size, size / 4);
	memcpy(pV, src + size + size / 4, size / 4);
}

krtc::MediaFrame* MainWindow::OnPreprocessVideoFrame(krtc::MediaFrame* origin_frame)
{
	if (!enable_beauty_) {
		return origin_frame;
	}

	int width = origin_frame->fmt.sub_fmt.video_fmt.width;
	int height = origin_frame->fmt.sub_fmt.video_fmt.height;

	// yuv转为cv::Mat
	cv::Mat yuvImage;
	yuvImage.create(height * 3 / 2, width, CV_8UC1);
	CopyYUVToImage(yuvImage.data, 
		(uint8_t*)origin_frame->data[0],
		(uint8_t*)origin_frame->data[1],
		(uint8_t*)origin_frame->data[2],
		width, height);
	cv::Mat rgbImage;
	cv::cvtColor(yuvImage, rgbImage, cv::COLOR_YUV2BGR_I420);

	// 美颜
	cv::Mat dst_image;
#if 0
    cv::Mat element = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3), cv::Point(-1, -1));
	cv::Mat image_erode;
    erode(rgbImage, image_erode, element, cv::Point(-1, -1), 1);	//腐蚀
    erode(image_erode, dst_image, element, cv::Point(-1, -1), 1);	//腐蚀
#else
	bilateralFilter(rgbImage, dst_image, beauty_index_, beauty_index_ * 2, beauty_index_ * 2);
#endif
	// cv::Mat转为yuv
	cv::Mat dstYuvImage;
	cv::cvtColor(dst_image, dstYuvImage, cv::COLOR_BGR2YUV_I420);
	CopyImageToYUV(
		(uint8_t*)origin_frame->data[0],
		(uint8_t*)origin_frame->data[1],
		(uint8_t*)origin_frame->data[2], 
		dstYuvImage.data,
		width, height);

	return std::move(origin_frame);
}

void MainWindow::OnPushNetworkInfo(uint64_t rtt_ms, uint64_t packets_lost, double fraction_lost)
{
	char str[256] = { 0 };
	snprintf(str, sizeof(str), "推流信息：rtt=%lldms 累计丢包数=%lld 丢包指数=%f",
		rtt_ms, packets_lost, fraction_lost);
	QString networkInfo = QString::fromLocal8Bit(str);
	emit networkInfoSignal(networkInfo);
}

void MainWindow::OnParcicipantsInfo(const std::map<std::string, bool>& infos)
{
	emit onParcicipantsInfoSignal(infos);
}

void MainWindow::OnParcicipantJoin(const std::string& uid, bool publishing)
{
	if (!publishing) {
		return;
	}

	emit onParcicipantJoinSignal(QString::fromStdString(uid), publishing);
}

void MainWindow::OnParcicipantLeave(const std::string& uid, bool publishing)
{
	if (!publishing) {
		return;
	}
	emit onParcicipantLeaveSignal(QString::fromStdString(uid), publishing);
}
