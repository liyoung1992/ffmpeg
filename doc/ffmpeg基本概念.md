# FFmpeg基本组成
框架的基本组成包含AVFormat,AVCodec，AVFilter，AVDevice,AVUtil等
## 封装模块AVFormat
AVFormat实现了目前多媒体中的大多数封装格式，包括封装和解封装。
## 编解码模块AVCodec
AVCodec实现了目前多媒体领域的绝大多数编解码格式，即支持编码，也支持解码（编码器的实现）
## 滤镜模块AVFilter
AVFilter提供了一个通用的音频、视频、字幕等滤镜处理框架

### 其他

- AVUtil公共函数，包含hash器，解码器
- libavdevice 设备接口
- libswresample 混音和重采样
- libswscale 色彩转换和缩放功能

# ffmpeg基本流程
![image](https://upload-images.jianshu.io/upload_images/11591878-810021da4c347e09.png?imageMogr2/auto-orient/strip|imageView2/2/w/1067/format/webp)
