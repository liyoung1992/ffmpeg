# ffmepg初级开发及使用

### 日志使用

```
#include "libavutil/log.h"
av_log_set_level(AV_LOG_DEBUG);
av_log(NULL, AV_LOG_ERROR, "Input file open input failed\n");

```

### 文件及目录操作

```
#include <libavformat/avformat.h>
# 删除
int ret = avpriv_io_delete("D://test.ts");
if (ret < 0) {
	av_log(NULL, AV_LOG_ERROR, "failed to delete file test.ts");
	return;
}
# 移动
int ret = avpriv_io_move("D://aac.ts", "D://aac_move.ts");
if (ret < 0) {
	av_log(NULL, AV_LOG_ERROR, "failed to move file aac.ts");
	return;
}
# 目录操作
	 // dir
	AVIODirContext* ctx = NULL;
	AVIODirEntry* entry = NULL;
	//注意Windows下会返回-40，也就是Function not implement,
	//windows不支持此函数
	int ret = avio_open_dir(&ctx, "D:/wamp", NULL);
	if (ret < 0) {
		av_log(NULL, AV_LOG_ERROR, "failed to open dir: %d\n",(ret));
		return;
	}
	while (true)
	{
		ret = avio_read_dir(ctx, &entry);
		if (ret < 0) {
			av_log(NULL, AV_LOG_ERROR, "failed read dir:%d\n", (ret));
			avio_close_dir(&ctx);
			return;
		}
		if (!entry) {
			break;
		}
		av_log(NULL, AV_LOG_INFO, "%12" PRId64 "%s \n", entry->size,entry->name);
		//释放
		avio_free_directory_entry(&entry);
	}
	avio_close_dir(&ctx);
	return;

```
### 打印音视频信息

```
#ifdef __cplusplus
// C++中使用av_err2str宏
char av_error[AV_ERROR_MAX_STRING_SIZE] = { 0 };
#define av_err2str(errnum)     av_make_error_string(av_error, AV_ERROR_MAX_STRING_SIZE, errnum)
#endif

	AVFormatContext* fmt_ctx = NULL;
	int ret = avformat_open_input(&fmt_ctx, "D:\\test.mp4", NULL, NULL);
	if (ret < 0) {
		av_log(NULL, AV_LOG_ERROR, "failed to open file,%s\n",av_err2str(ret));
	}
	av_dump_format(fmt_ctx, 0, "D:\\test.mp4", 0);
	avformat_close_input(&fmt_ctx);

```