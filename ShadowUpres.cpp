/**************************************************************************************/
/* File name: ShadowUpres.cpp                                                         */
/* Project: ShadowUpres                                                               */
/* File version: v0.1                                                                 */
/* Dev: Rr42 (gethub@rr42)                                                            */
/* Description:                                                                       */
/*  This file contains the 'main' function of the ShadowUpres project.                */
/*  Program execution begins and ends there.                                          */
/**************************************************************************************/

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
#pragma comment(lib, "opencv_world451d.lib")
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

/* Global definitions */
#define MY_AV_ALLIGN 1
#define INBUF_SIZE 4096

/* Function headers */
static void pgm_save(unsigned char* buf, int wrap, int xsize, int ysize, const char* filename);
static void decode(AVCodecContext* dec_ctx, AVFrame* frame, AVPacket* pkt, const char* filename);
static inline int fill_picture(AVPicture* picture, uint8_t* ptr, enum AVPixelFormat pix_fmt, int width, int height);
static void SaveFrame(AVFrame* pFrame, int width, int height, int iFrame);

/* ShadowUpres core function */
int main(int argc, char** argv)
{
    char err_buf[AV_ERROR_MAX_STRING_SIZE];
    int retCode;

    //if (argc <= 1) 
    //{
    //    std::cerr << "Usage: " << argv[0] << " <input file>" << std::endl;
    //    exit(0);
    //}
    //const char* filename = argv[1];
    //const char* outfilename = "./out";

    const char* filename = "E:\\Dev\\ProjectUpres\\ShadowUpres\\x64\\Debug\\test1.mp4";
    //const char* filename = "E:\\Dev\\ProjectUpres\\ShadowUpres\\x64\\Debug\\test1-f1.mp4";
    //const char* filename = "E:\\Dev\\ProjectUpres\\ShadowUpres\\x64\\Debug\\test2.mp4";
    //const char* filename = "E:\\Dev\\ProjectUpres\\ShadowUpres\\x64\\Debug\\test3.mkv";
    const char* outfilename = "E:\\Dev\\ProjectUpres\\ShadowUpres\\x64\\Debug\\out";
    
    /* Extract the decoder from video */
    static AVFormatContext* ifmt_ctx = NULL;
    retCode = avformat_open_input(&ifmt_ctx, filename, NULL, NULL);
    if (retCode < 0)
    {
        //av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
        std::cerr << "Cannot open input file" << std::endl;
        return retCode;
    }

    retCode = avformat_find_stream_info(ifmt_ctx, NULL);
    if (retCode < 0) 
    {
        //av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        std::cerr << "Cannot find stream information" << std::endl;
        return retCode;
    }

    /* Find the first video stream */
    int vstrm_idx = -1;
    int astrm_idx = -1;
    for (unsigned int i = 0; i < ifmt_ctx->nb_streams; i++)
    {
        if (ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
            vstrm_idx = i;
        else if (ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
            astrm_idx = i;

        if (vstrm_idx != -1 && astrm_idx != -1)
            break;
    }
    if (astrm_idx < 0)
        std::cout << "Warning unable to find audio stream: astrm_idx=" << astrm_idx;
    if (vstrm_idx < 0)
    {
        std::cerr << "Failed unable to find video stream: vstrm_idx=" << vstrm_idx;
        return vstrm_idx;
    }

    /* Find the video decoder */
    AVCodecParameters* codec_strm_ctx = ifmt_ctx->streams[vstrm_idx]->codecpar;
    const AVCodec* codec = avcodec_find_decoder(codec_strm_ctx->codec_id);
    if (!codec)
    {
        std::cerr << "Codec not found" << std::endl;
        exit(1);
    }

    /* Create codec context based on codec */
    AVCodecContext* codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx)
    {
        std::cerr << "Could not allocate video codec context" << std::endl;
        exit(1);
    }

    /* For some codecs, such as msmpeg4 and mpeg4, width and height
       MUST be initialized there because this information is not
       available in the bitstream. */

    /* Copy codec parameters to codec context */
    retCode = avcodec_parameters_to_context(codec_ctx, codec_strm_ctx);
    if (retCode < 0)
    {
        std::cerr << "Failed to copy decoder parameters to input decoder context for stream " << vstrm_idx << std::endl;
        return retCode;
    }

    /* Initialize the codec context to use the given codec */
    retCode = avcodec_open2(codec_ctx, codec, NULL);
    if (retCode < 0)
    {
        std::cerr << "Failed to open codec through avcodec_open2 for stream " << vstrm_idx << std::endl;
        return retCode;
    }
    
    const int dst_height = codec_strm_ctx->height;
    const int dst_width = codec_strm_ctx->width;
    std::cout << "W: " << codec_strm_ctx->width << " L: " << codec_strm_ctx->height << std::endl;

    /* initialize SWS context for software scaling */
    struct SwsContext* sws_ctx = sws_getContext(dst_width,
        dst_height,
        static_cast<AVPixelFormat>(codec_strm_ctx->format),
        dst_width,
        dst_height,
        AV_PIX_FMT_RGB24,
        SWS_BILINEAR,
        NULL,
        NULL,
        NULL);

    std::cout << "nb_frames: " << ifmt_ctx->streams[vstrm_idx]->nb_frames << std::endl;
    std::cout << "duration: " << ifmt_ctx->streams[vstrm_idx]->duration << std::endl;
    std::cout << "time_base (s): " << av_q2d(ifmt_ctx->streams[vstrm_idx]->time_base) << std::endl;
    std::cout << "length (s): " << ifmt_ctx->streams[vstrm_idx]->duration * av_q2d(ifmt_ctx->streams[vstrm_idx]->time_base) << std::endl;
    std::cout << "r_frame_rate: " << av_q2d(ifmt_ctx->streams[vstrm_idx]->r_frame_rate) << std::endl;
    std::cout << "codec_tag: " << codec_strm_ctx->codec_tag << std::endl;
    std::cout << "codec_id: " << codec_strm_ctx->codec_id << std::endl;
    std::cout << "codec name: " << codec->name << std::endl;
    std::cout << "codec_ctx id name: " << avcodec_find_decoder(codec_ctx->codec_id)->name << std::endl;

    /* Allocate an AVPacket structure */
    AVPacket* packet = av_packet_alloc();
    if (!packet)
    {
        std::cerr << "Could not allocate video packet" << std::endl;
        return -1;
    }

    /* Allocate an AVFrame structures */
    AVFrame* frame = av_frame_alloc();
    if (!frame)
    {
        std::cerr << "Could not allocate video frame" << std::endl;
        return -1;
    }
    AVFrame* cvframe = av_frame_alloc();
    if (!cvframe)
    {
        std::cerr << "Could not allocate cv video frame" << std::endl;
        return -1;
    }

    std::vector<uint8_t> framebuf(av_image_get_buffer_size(AV_PIX_FMT_RGB24, dst_width, dst_height, MY_AV_ALLIGN));
    fill_picture(reinterpret_cast<AVPicture*>(cvframe), framebuf.data(), AV_PIX_FMT_RGB24, dst_width, dst_height);
    int frameFinished = 0;
    int ittr = 0;
    while (av_read_frame(ifmt_ctx, packet) >= 0)
    {
        /* Is this a packet from the video stream? */
        if (packet->stream_index == vstrm_idx)
        {
            std::cout << "stream index: " << packet->stream_index << " --video frame--" << std::endl;
            /* Decode video frame */
            std::cout << "packet size: " << packet->size << std::endl;
            std::cout << "packet duration: " << packet->duration << std::endl;
            std::cout << "packet length (s): " << packet->duration * av_q2d(ifmt_ctx->streams[vstrm_idx]->time_base) << std::endl;
            std::cout << "packet position: " << packet->pos << std::endl;
            std::cout << "packet buffer size: " << packet->buf->size << std::endl;
            std::cout << "packet data size: " << sizeof(packet->data) / sizeof(packet->data[0]) << std::endl;
            av_packet_rescale_ts(packet, ifmt_ctx->streams[vstrm_idx]->time_base, codec_ctx->time_base);
            frameFinished = avcodec_send_packet(codec_ctx, packet);
            std::cout << "avcodec_send_packet: " << frameFinished << " : " << av_make_error_string(err_buf, AV_ERROR_MAX_STRING_SIZE, frameFinished) << std::endl;
            frameFinished = avcodec_receive_frame(codec_ctx, frame);
            std::cout << "avcodec_receive_frame: " << frameFinished << " : " << av_make_error_string(err_buf, AV_ERROR_MAX_STRING_SIZE, frameFinished) << std::endl;
            /* convert frame to OpenCV matrix */
            sws_scale(sws_ctx, frame->data, frame->linesize, 0, frame->height, cvframe->data, cvframe->linesize);
            {
                cv::Mat image(dst_height, dst_width, CV_8UC3, framebuf.data(), cvframe->linesize[0]);
                cv::imshow("press ESC to exit", image);
                if (cv::waitKey(1) == 0x1b)
                    break;
            }
            /* Did we get a video frame? */
            //if (frameFinished) 
            //{
            //    /* Convert the image from its native format to RGB */
            //    sws_scale(sws_ctx, (uint8_t const* const*)frame->data, frame->linesize, 0, dst_height, cvframe->data, cvframe->linesize);

            //    /* Save the frame to disk */
            //    if (++ittr <= 5)
            //        SaveFrame(cvframe, dst_width, dst_height, ittr);
            //}
        }
        else if (packet->stream_index == astrm_idx)
        {
            std::cout << "stream index: " << packet->stream_index << "--audio frame--" << std::endl;
            /* Do audio stuff */
        }
        av_packet_unref(packet);
    }

    av_frame_free(&cvframe);
    av_frame_free(&frame);
    av_packet_free(&packet);
    sws_freeContext(sws_ctx);
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&ifmt_ctx);

    return 0;
}

/* Function definitions */
static void pgm_save(unsigned char* buf, int wrap, int xsize, int ysize, const char* filename)
{
    FILE* f = fopen(filename, "wb");
    fprintf(f, "P5\n%d %d\n%d\n", xsize, ysize, 255);
    for (int i = 0; i < ysize; i++)
        fwrite(buf + i * static_cast<int64_t>(wrap), 1, xsize, f);
    fclose(f);
}

static void decode(AVCodecContext* dec_ctx, AVFrame* frame, AVPacket* pkt, const char* filename)
{
    char buf[1024];
    int retCode;

    retCode = avcodec_send_packet(dec_ctx, pkt);
    if (retCode < 0)
    {
        std::cerr << "Error sending a packet for decoding" << std::endl;
        exit(1);
    }

    while (retCode >= 0)
    {
        retCode = avcodec_receive_frame(dec_ctx, frame);
        if (retCode == AVERROR(EAGAIN) || retCode == AVERROR_EOF)
            return;
        else if (retCode < 0)
        {
            std::cerr << "Error during decoding" << std::endl;
            exit(1);
        }

        printf("saving frame %3d\n", dec_ctx->frame_number);
        fflush(stdout);

        /* the picture is allocated by the decoder. no need to
           free it */
        snprintf(buf, sizeof(buf), "%s-%d", filename, dec_ctx->frame_number);
        pgm_save(frame->data[0], frame->linesize[0], frame->width, frame->height, buf);
    }
}

static inline int fill_picture(AVPicture* picture, uint8_t* ptr, enum AVPixelFormat pix_fmt, int width, int height)
{
    AVFrame* frame = reinterpret_cast<AVFrame*>(picture);
    return av_image_fill_arrays(frame->data, frame->linesize, ptr, pix_fmt, width, height, MY_AV_ALLIGN);
}

static void SaveFrame(AVFrame* pFrame, int width, int height, int iFrame)
{
    char szFilename[32];

    /* Open file */
    sprintf(szFilename, "frame%d.ppm", iFrame);
    FILE* pFile = fopen(szFilename, "wb");
    if (pFile == NULL)
        return;

    /* Write header */
    fprintf(pFile, "P6\n%d %d\n255\n", width, height);

    /* Write pixel data */
    for (int y = 0; y < height; y++)
        fwrite(pFrame->data[0] + y * static_cast<int64_t>(pFrame->linesize[0]), 1, static_cast<int64_t>(width) * 3, pFile);

    /* Close file */
    fclose(pFile);
}