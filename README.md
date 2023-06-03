# srs_video_conference
基于native webrtc m96实现srs多人通话视频会议的sdk 
https://www.yuque.com/caokunchao/rtendq/avu1a24fzsxdmcr3 


# 完成
1. 支持加入会议和离开会议
2. 支持使用域名的方式访问
3. 获取所有参会者的信息
4. 支持推流视频h264硬件编码
5. 回调支持视频编码前的处理，demo提供了磨皮美颜
6. 推流过程中，允许暂停往网络发送本地音频流，而是静音帧
7. 推流过程中，允许暂停往网络发送本地视频流，而是黑屏帧
8. 支持获取网络质量GetStats
   
# 待做
1. sdk中提供每个参会者的yuv裸流
2. demo改用和预览的一样使用QOpenGLWidget来渲染
3. 优化demo显示，使用伸缩窗口方式，以支持显示更多的参会人界面
