QT       += core gui multimedia widgets openglwidgets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets openglwidgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

DEFINES += __STDC_CONSTANT_MACROS
DEFINES += _MM_PREFETCHW_H
DEFINES += SDL_MAIN_HANDLED

INCLUDEPATH += $$PWD/soundtouch/include

SOURCES += \
    $$PWD/SoundTouch/source/SoundTouch/mmx_optimized.cpp \
    $$PWD/SoundTouch/source/SoundTouch/sse_optimized.cpp \
    $$PWD/SoundTouch/source/SoundTouch/cpu_detect_x86.cpp \
    $$PWD/SoundTouch/source/SoundTouch/SoundTouch.cpp \
    $$PWD/SoundTouch/source/SoundTouch/TDStretch.cpp \
    $$PWD/SoundTouch/source/SoundTouch/AAFilter.cpp \
    $$PWD/SoundTouch/source/SoundTouch/BPMDetect.cpp \
    $$PWD/SoundTouch/source/SoundTouch/FIFOSampleBuffer.cpp \
    $$PWD/SoundTouch/source/SoundTouch/FIRFilter.cpp \
    $$PWD/SoundTouch/source/SoundTouch/InterpolateCubic.cpp \
    $$PWD/SoundTouch/source/SoundTouch/InterpolateLinear.cpp \
    $$PWD/SoundTouch/source/SoundTouch/InterpolateShannon.cpp \
    $$PWD/SoundTouch/source/SoundTouch/PeakFinder.cpp \
    $$PWD/SoundTouch/source/SoundTouch/RateTransposer.cpp \
    filtersettings.cpp \
    superresolution.cpp

win32 {
INCLUDEPATH += $$PWD/ffmpeg-n6.0-22/include
LIBS += $$PWD/ffmpeg-n6.0-22/lib/avformat.lib   \
        $$PWD/ffmpeg-n6.0-22/lib/avcodec.lib    \
        $$PWD/ffmpeg-n6.0-22/lib/avdevice.lib   \
        $$PWD/ffmpeg-n6.0-22/lib/avfilter.lib   \
        $$PWD/ffmpeg-n6.0-22/lib/avutil.lib     \
        $$PWD/ffmpeg-n6.0-22/lib/postproc.lib   \
        $$PWD/ffmpeg-n6.0-22/lib/swresample.lib \
        $$PWD/ffmpeg-n6.0-22/lib/swscale.lib
INCLUDEPATH += $$PWD/SDL2-2.0.10/include
LIBS += $$PWD/SDL2-2.0.10/lib/x64/SDL2.lib
}


# ===== ONNX Runtime 配置 =====
INCLUDEPATH += $${PWD}/onnxruntime-win-x64-gpu-1.26.0/include/
LIBS += -L$${PWD}/onnxruntime-win-x64-gpu-1.26.0/lib/ -lonnxruntime
LIBS += -L$${PWD}/onnxruntime-win-x64-gpu-1.26.0/lib/ -lonnxruntime_providers_cuda
LIBS += -L$${PWD}/onnxruntime-win-x64-gpu-1.26.0/lib/ -lonnxruntime_providers_tensorrt
LIBS += -L$${PWD}/onnxruntime-win-x64-gpu-1.26.0/lib/ -lonnxruntime_providers_shared

SOURCES += \
    ctrlbar.cpp \
    displayvideo.cpp \
    ffmsg_queue.cpp \
    ffplay_def.cpp \
    ffplayer.cpp \
    ijkmediaplayer.cpp \
    main.cpp \
    mainwindow.cpp \
    playlist.cpp \
    thumbnail_extractor.cpp \
    titlebar.cpp \
    video_list_item.cpp \
    videoglwidget.cpp

HEADERS += \
    ctrlbar.h \
    displayvideo.h \
    ffmsg.h \
    ffmsg_queue.h \
    ffplay_def.h \
    ffplayer.h \
    filtersettings.h \
    superresolution.h \
    ijkmediaplayer.h \
    mainwindow.h \
    playlist.h \
    thumbnail_extractor.h \
    titlebar.h \
    video_list_item.h \
    videoglwidget.h

FORMS += \
    ctrlbar.ui \
    displayvideo.ui \
    filtersettings.ui \
    mainwindow.ui \
    playlist.ui \
    titlebar.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    res.qrc
