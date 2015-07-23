# MAA_IOS_H2SDK

##功能简介

本SDK基于开源的多协议传输库libcurl和最新发布的HTTP/2 RFC标准（RFC7540）的开源实现nghttp2，并结合网宿科技MAA移动加速服务平台，为iOS上的APP应用提供HTTP/2网络加速服务。

libcurl：http://curl.haxx.se/libcurl

nghttp2：http://www.nghttp2.org

RFC7540：https://datatracker.ietf.org/doc/rfc7540


##使用方法

SDK嵌入步骤：

（1）导入静态库文件及头文件。

（2）定义并初始化一个curl_maa_config结构变量。

（3）调用curl_maa_global_init()进行MAA全局初始化。

（4）调用curl_maa_on_network_type_changed()设置访问网络类型。

（5）为Easy Handler设置CURLOPT_MAA_ENABLE属性。

（6）为Easy Handler设置CURLOPT_MAA_HTTPS_SWITCH属性。（可选）

（7）在结束前调用curl_maa_global_cleanup()进行MAA全局清理。

具体使用方法可参见Demo示例。

##技术支持

如果您对本SDK感兴趣，欢迎与我们做进一步的交流。

网宿科技MAA移动加速：http://maa.chinanetcenter.com
