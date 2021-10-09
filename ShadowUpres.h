#pragma once

/* External libraries to link */
/* Windows libs */
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "strmiids.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "security.lib")
#pragma comment(lib, "Setupapi.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "version.lib")
#pragma comment(lib, "Imm32.lib")
/* Opencv libs */
#ifdef _DEBUG
#pragma comment(lib, "opencv_world451d.lib")
#else
#pragma comment(lib, "opencv_world451.lib")
#endif
/* ffmpeg libs */
#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avfilter.lib")
#pragma comment(lib, "avdevice.lib")
#pragma comment(lib, "vpxmt.lib")
#pragma comment(lib, "swscale.lib")
#pragma comment(lib, "swresample.lib")
#pragma comment(lib, "opus.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "avcodec.lib")
/* SDL2 libs */
#pragma comment(lib, "SDL2main.lib")
#pragma comment(lib, "SDL2.lib")

#define _CRT_SECURE_NO_DEPRECATE

/* Includes */
/* General */
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
/* FFmpeg */
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}
/* OpenCV */
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
/* SDL2 */
#include <sdl2/SDL.h>
#include <sdl2/SDL_thread.h>

/* Global definitions */
constexpr uint8_t MY_AV_ALLIGN = 1;
constexpr uint16_t INBUF_SIZE = 4096U;
constexpr uint8_t VERBOSE_DEBUG = 0;
constexpr uint8_t VERBOSE_DEBUG_VIDEO = 0;
constexpr uint8_t VERBOSE_DEBUG_AUDIO = 0;
constexpr uint8_t VERBOSE_DEBUG_AUDIO_DECODE = 0;
constexpr uint8_t VERBOSE_DEBUG_AUDIO_BUFFER = 0;
constexpr uint8_t SKIP_VIDEO_PROCESSING = 0;
constexpr uint8_t SKIP_AUDIO_PROCESSING = 0;
constexpr uint8_t OPENCV_MODE = 0;
constexpr uint8_t SDL_VIDEO_MODE = 1;
constexpr uint8_t SDL_AUDIO_MODE = 1;

//Refresh Event
constexpr uint16_t REFRESH_EVENT = SDL_USEREVENT + 1U;
//Break
constexpr uint16_t BREAK_EVENT = SDL_USEREVENT + 2U;

constexpr uint8_t AUDIO_DRIVER_ID = 1;
constexpr uint8_t AUDIO_DEVICE_ID = 0;

extern std::atomic<int> thread_exit;

/* Function headers */
static void pgm_save(unsigned char* buf, int wrap, int xsize, int ysize, const char* filename);
static void decode(AVCodecContext* dec_ctx, AVFrame* frame, AVPacket* pkt, const char* filename);
static inline int fill_picture(AVPicture* picture, uint8_t* ptr, enum AVPixelFormat pix_fmt, int width, int height);
static void SaveFrame(AVFrame* pFrame, int width, int height, int iFrame);
static int refresh_video(void* opaque);
static void cleanUpOnFail(int retCode = 0);
