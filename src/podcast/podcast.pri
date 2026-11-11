
HEADERS      += src/podcast/UBPodcastController.h \
                src/podcast/UBAbstractVideoEncoder.h \
                src/podcast/UBPodcastRecordingPalette.h \
                src/podcast/youtube/UBYouTubePublisher.h \
                src/podcast/intranet/UBIntranetPodcastPublisher.h \
                
SOURCES      += src/podcast/UBPodcastController.cpp \
                src/podcast/UBAbstractVideoEncoder.cpp \
                src/podcast/UBPodcastRecordingPalette.cpp \
                src/podcast/youtube/UBYouTubePublisher.cpp \
                src/podcast/intranet/UBIntranetPodcastPublisher.cpp \

win32 {

    SOURCES  += src/podcast/windowsmedia/UBWindowsMediaVideoEncoder.cpp \
                src/podcast/windowsmedia/UBWindowsMediaFile.cpp \
                src/podcast/windowsmedia/UBWaveRecorder.cpp

    HEADERS  += src/podcast/windowsmedia/UBWindowsMediaVideoEncoder.h \
                src/podcast/windowsmedia/UBWindowsMediaFile.h \
                src/podcast/windowsmedia/UBWaveRecorder.h
}

macx {

    SOURCES  += src/podcast/ffmpeg/UBFFmpegVideoEncoder.cpp \
                src/podcast/ffmpeg/UBMicrophoneInput.cpp

    HEADERS  += src/podcast/ffmpeg/UBFFmpegVideoEncoder.h \
                src/podcast/ffmpeg/UBMicrophoneInput.h

    LIBS += -lavformat -lavcodec -lswscale -lswresample -lavutil
}

linux-g++* {
    HEADERS  += src/podcast/ffmpeg/UBFFmpegVideoEncoder.h \
                src/podcast/ffmpeg/UBMicrophoneInput.h

    SOURCES  += src/podcast/ffmpeg/UBFFmpegVideoEncoder.cpp \
                src/podcast/ffmpeg/UBMicrophoneInput.cpp


    DEPENDPATH += /usr/lib/x86_64-linux-gnu

    CONFIG += link_pkgconfig
    PKGCONFIG += libavcodec libavformat libavutil libswscale libswresample
    LIBS += -lz -ldl -lm -lpthread

    FFMPEG_VERSION = $$system(ffmpeg --version|& grep -oP "version.*?\K[0-9]\.[0-9]")
    equals(FFMPEG_VERSION, 2.8) {
        LIBS -= -lswresample
        LIBS += -lavresample
    }


}
