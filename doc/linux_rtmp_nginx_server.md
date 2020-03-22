# linux搭建RTSP服务器
选择nginx作为服务器，附加rtmp模块，openssl模块。

## 软件下载地址
```
1、Nginx：https://github.com/nginx/nginx
2、OpenSSL：https://github.com/openssl/openssl
3、rtmp：https://github.com/arut/nginx-rtmp-module
# 可能还需要安装这些模块
4、pcre：ftp://ftp.csx.cam.ac.uk/pub/software/programming/pcre/
5、zlib：http://www.zlib.net/
```
## 编译安装

```
#解压
tar -zxvf nginx-release-1.17.9.tar.gz 
tar -zxvf nginx-rtmp-module-1.2.1.tar.gz 
tar -zxvf openssl-OpenSSL_1_1_1e.tar.gz 
tar -zxvf zlib-1.2.11.tar.gz 
tar -zxvf pcre-8.01.tar.gz 
# 编译安装
 cd openssl-OpenSSL_1_1_1e/
 ./config --prefix=`pwd`/libs
 make && make install
 cd ../pcre-8.01/
 ./configure 
 make
 sudo make install
 cd ../zlib-1.2.11/
 ./configure 
 make
 sudo make install
 
 ./auto/configure --add-module=/home/zly/rtmp/nginx-rtmp-module-1.2.1 --with-openssl=/home/zly/rtmp/openssl-OpenSSL_1_1_1e
make
make install
cp /home/zly/rtmp/nginx-rtmp-module-1.2.1/test/nginx.conf conf/nginx.conf

# 测试
#nginx 进程
root      3662  1095  0 23:36 ?        00:00:00 nginx: master process ./nginx
nobody    3663  3662  0 23:36 ?        00:00:00 nginx: worker process
zly       4383  1855  0 23:40 pts/0    00:00:00 grep --color=auto nginx

#test rtsp
 ./ffmpeg.exe -re -i test.flv -vcodec libx264 -acodec aac -f flv rtmp://192.168.1.104/myapp/mystream

# nginx log
2020/03/21 23:51:08 [info] 3663#0: *3 connect: app='myapp' args='' flashver='FMLE/3.0 (compatible; Lavf58.37' swf_url='' tc_url='rtmp://192.168.1.104:1935/myapp' page_url='' acodecs=0 vcodecs=0 object_encoding=0, client: 192.168.1.112, server: 0.0.0.0:1935
2020/03/21 23:51:08 [info] 3663#0: *3 createStream, client: 192.168.1.112, server: 0.0.0.0:1935
2020/03/21 23:51:08 [info] 3663#0: *3 publish: name='mystream' args='' type=live silent=0, client: 192.168.1.112, server: 0.0.0.0:1935
2020/03/21 23:51:36 [info] 3663#0: *3 deleteStream, client: 192.168.1.112, server: 0.0.0.0:1935
2020/03/21 23:51:36 [info] 3663#0: *3 disconnect, client: 192.168.1.112, server: 0.0.0.0:1935
2020/03/21 23:51:36 [info] 3663#0: *3 deleteStream, client: 192.168.1.112, server: 0.0.0.0:1935
2020/03/21 23:51:40 [info] 3663#0: *2 recv() failed (104: Connection reset by peer), client: 192.168.1.112, server: 0.0.0.0:1935
2020/03/21 23:51:40 [info] 3663#0: *2 disconnect, client: 192.168.1.112, server: 0.0.0.0:1935
2020/03/21 23:51:40 [info] 3663#0: *2 deleteStream, client: 192.168.1.112, server: 0.0.0.0:1935

最后使用vlc播放流


```

## 网络流转发

```
	int ret = openInput("rtsp://169.254.51.14:8554/channel=0");
	if (ret >= 0)
	{
		//转发 rtmp://192.168.1.110/myapp/mystream  输出类型应该为 flv
		//保存到本地 E:\\xiazai0321.ts 输出类型设置为 mpegts
		ret = openOutput("rtmp://192.168.1.110/myapp/mystream");
		
	}
```