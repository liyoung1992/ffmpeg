# ffmepg����������ʹ��

### ��־ʹ��

```
#include "libavutil/log.h"
av_log_set_level(AV_LOG_DEBUG);
av_log(NULL, AV_LOG_ERROR, "Input file open input failed\n");

```

### �ļ���Ŀ¼����

```
#include <libavformat/avformat.h>
# ɾ��
int ret = avpriv_io_delete("D://test.ts");
if (ret < 0) {
	av_log(NULL, AV_LOG_ERROR, "failed to delete file test.ts");
	return;
}
# �ƶ�
int ret = avpriv_io_move("D://aac.ts", "D://aac_move.ts");
if (ret < 0) {
	av_log(NULL, AV_LOG_ERROR, "failed to move file aac.ts");
	return;
}
# Ŀ¼����
	 // dir
	AVIODirContext* ctx = NULL;
	AVIODirEntry* entry = NULL;
	//ע��Windows�»᷵��-40��Ҳ����Function not implement,
	//windows��֧�ִ˺���
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
		//�ͷ�
		avio_free_directory_entry(&entry);
	}
	avio_close_dir(&ctx);
	return;

```
### ��ӡ����Ƶ��Ϣ

```
#ifdef __cplusplus
// C++��ʹ��av_err2str��
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