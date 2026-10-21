#ifndef _BURY_POINT_hpp_
#define _BURY_POINT_hpp_
#include <string>
#include <iostream>
#include <atomic>

// BURY_API 三态：
//   BURY_IMPORT_FROM_DLL —— 仅 Windows exe shim：状态宿主是 Snapmaker_Orca.dll，必须跨镜像读写；
//   BURY_EXPORTS         —— 直接编入 Snapmaker_Orca.dll 的形态，导出 bury 符号供 shim 导入；
//   默认（空）           —— app_common 静态库及其它一切 TU：普通符号引用，可绑定同镜像内任意声明形态的定义。
// 默认值刻意不再是 dllimport：后者会让引用退化为 __imp_ 形式，强制消费者必须提供导入库。
#ifdef _WIN32
    #ifdef BURY_IMPORT_FROM_DLL
        #define BURY_API __declspec(dllimport)
    #elif defined(BURY_EXPORTS)
        #define BURY_API __declspec(dllexport)
    #else
        #define BURY_API
    #endif
#else
    #define BURY_API  extern
#endif

#define BURY_POINT "bury_point"

#define BP_START_SOFT "bury_point_start_soft"
#define BP_SOFT_WORKS_TIME "bury_point_soft_works_time"

#define BP_DEIVCE_CONNECT "bury_point_device_connect"
#define BP_CONNECT_DEVICE_ID "bury_point_device_id"
#define BP_CONNECT_NET_TYPE "bury_point_net_type"

#define BP_LOGIN "bury_point_login"
#define BP_LOGIN_USER_ID "bury_point_user_id"
#define BP_LOGIN_HTTP_CODE "bury_point_http_code"

#define BP_VIDEO_START "bury_point_video_start"
#define BP_VIDEO_STATUS "bury_point_video_status"

#define BP_DEVICE_ERROR "bury_point_device_error"
#define BP_DEVICE_ERROR_STATUS "bury_point_error_status"

#define BP_UPLOAD "bury_point_upload"

#define BP_UPLOAD_AND_PRINT "bury_point_upload_and_print"

#define BP_COLOR_PAINTING "bury_point_color_painting"

#define BP_VIDEO_ABNORMAL "bury_point_video_abnormal"

#define BP_SLICE_DURATION "bury_point_slice_duration"
#define BP_SLICE_DURATION_TIME "bury_point_slice_duration_time"

#define BP_WEB_VIEW "bury_point_webview"

#define BP_LOCAL_SERVER "bury_point_local_server"
#define BP_LOCAL_SERVER_STATUS "bury_point_local_server_status"
#define BP_LOCAL_SERVER_ERROR_CODE "bury_point_local_server_error_code"


//device connect error info
#define DEVICE_CONNECT_ERR "error_device_connect"
#define DEVICE_SUBSCRIBE_ERR "error_device_subscribe"
#define DEVICE_PBLISH_ERR "error_device_publish"
#define DEVICE_SET_ENGINE_ERR "error_device_set_engine"


//webview bury point

    BURY_API bool get_sentry_flags();
    BURY_API void set_sentry_flags(bool flags);

    BURY_API bool               get_privacy_policy();
    BURY_API void               set_privacy_policy(bool isAgree);
    BURY_API std::string get_timestamp_seconds();
    BURY_API long long   get_time_timestamp();
    BURY_API std::string get_works_time(const long long& timestamp);

#endif