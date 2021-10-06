/**************************************************************************************/
/* File name: ShadowUpres.cpp                                                         */
/* Project: ShadowUpres                                                               */
/* File version: v0.1                                                                 */
/* Dev: Rr42 (gethub@rr42)                                                            */
/* Description:                                                                       */
/*  This file contains the 'main' function of the ShadowUpres project.                */
/*  Program execution begins and ends there.                                          */
/**************************************************************************************/

/* Core include */
#include "ShadowUpres.h"

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

    /* Hardware decoding 
        ref1: https://github.com/FFmpeg/FFmpeg/blob/release/4.1/doc/examples/hw_decode.c 
        ref2: https://stackoverflow.com/questions/57211846/decoding-to-specific-pixel-format-in-ffmpeg-with-c */
    std::cout << "Listing compatable hardware types: ";
    enum AVHWDeviceType type = AV_HWDEVICE_TYPE_NONE;
    while ((type = av_hwdevice_iterate_types(type)) != AV_HWDEVICE_TYPE_NONE)
        std::cout << av_hwdevice_get_type_name(type) << ", ";
    std::cout << std::endl;
    
    /* Initialize SDI */
    retCode = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER);
    if (retCode != 0)
    {
        /* Error while initializing SDL */
        std::cout << "Could not initialize SDL - " << SDL_GetError() << std::endl ;
        return retCode;
    }

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

    /* Create a SDL window with the specified position, dimensions, and flags */
    SDL_Window* screen = SDL_CreateWindow(
        "SDL Video Player",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        codec_ctx->width / 2,
        codec_ctx->height / 2,
        SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI
    );
    if (!screen)
    {
        /* Could not set video mode */
       std::cout << "SDL: could not set video mode - exiting" << std::endl;
        return -1;
    }

    /* Set the swap interval to update synchronized with the vertical retrace */
    /* Valid arguments:
        0  -> for immediate updates
        1  -> for updates synchronized with the vertical retrace
        -1 -> for adaptive vsync
    */
    SDL_GL_SetSwapInterval(1);

    /* A structure that contains a rendering state */
    SDL_Renderer* renderer = NULL;

    /* Use this function to create a 2D rendering context for a window */
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");
    renderer = SDL_CreateRenderer(screen, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE);   // [3]

    /* A structure that contains an efficient, driver-specific representation of pixel data */
    SDL_Texture* texture = NULL;

    /* Use this function to create a texture for a rendering context */
    texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGB24,
        //SDL_PIXELFORMAT_YV12,
        SDL_TEXTUREACCESS_STREAMING,
        //SDL_TEXTUREACCESS_STATIC,
        codec_ctx->width,
        codec_ctx->height
    );

    /* Initialize SWS context for software scaling */
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

    /* CV MODE */
    std::vector<uint8_t> framebuf(av_image_get_buffer_size(AV_PIX_FMT_RGB24, dst_width, dst_height, MY_AV_ALLIGN));
    if (OPENCV_MODE)
    {        
        fill_picture(reinterpret_cast<AVPicture*>(cvframe), framebuf.data(), AV_PIX_FMT_RGB24, dst_width, dst_height);
    }
    /* SDL MODE */
    int numBytes;
    uint8_t* buffer = NULL;
    if (SDL_MODE)
    {
        numBytes = av_image_get_buffer_size(
            AV_PIX_FMT_RGB24,
            codec_ctx->width,
            codec_ctx->height,
            32
        );
        buffer = (uint8_t*)av_malloc(numBytes * sizeof(uint8_t));

        av_image_fill_arrays(
            cvframe->data,
            cvframe->linesize,
            buffer,
            AV_PIX_FMT_RGB24,
            codec_ctx->width,
            codec_ctx->height,
            32
        );
    }

    int frameFinished = 0;
    int ittr = 0;
    bool FLAG_EXIT = false;
    while (av_read_frame(ifmt_ctx, packet) >= 0)
    {
        /* Is this a packet from the video stream? */
        if (packet->stream_index == vstrm_idx)
        {
            if (VERBOSE_DEBUG)
            {
                std::cout << "stream index: " << packet->stream_index << " --video frame--" << std::endl;
                std::cout << "packet size: " << packet->size << std::endl;
                std::cout << "packet duration: " << packet->duration << std::endl;
                std::cout << "packet length (s): " << packet->duration * av_q2d(ifmt_ctx->streams[vstrm_idx]->time_base) << std::endl;
                std::cout << "packet position: " << packet->pos << std::endl;
                std::cout << "packet buffer size: " << packet->buf->size << std::endl;
                std::cout << "packet data size: " << sizeof(packet->data) / sizeof(packet->data[0]) << std::endl;
            }
            /* Decode video frame */
            av_packet_rescale_ts(packet, ifmt_ctx->streams[vstrm_idx]->time_base, codec_ctx->time_base);
            frameFinished = avcodec_send_packet(codec_ctx, packet);
            if (VERBOSE_DEBUG)
                std::cout << "avcodec_send_packet: " << frameFinished << " : " << av_make_error_string(err_buf, AV_ERROR_MAX_STRING_SIZE, frameFinished) << std::endl;

            /* Used to handle quit event */
            SDL_Event event;

            /* Get clip fps */
            double vfps = av_q2d(ifmt_ctx->streams[vstrm_idx]->r_frame_rate);
            /* Get clip sleep time in ms (truncate) */
            /* [TBD] Move var declarations outside loop */
            long sleep_time = static_cast<long>(1000.0 / vfps);
            if (VERBOSE_DEBUG)
                std::cout << "sleep_time: " << sleep_time << std::endl;
            /* Decoding loop in case a packet has multiple frames */
            while (frameFinished >= 0)
            {
                /* Get decoded frame */
                frameFinished = avcodec_receive_frame(codec_ctx, frame);
                /* Check for errors */
                if (frameFinished == AVERROR(EAGAIN) || frameFinished == AVERROR_EOF)
                    break;
                else if (frameFinished < 0)
                {
                    std::cout << "Error while decoding." << std::endl;
                    return -1;
                }
                if (VERBOSE_DEBUG)
                    std::cout << "avcodec_receive_frame: " << frameFinished << " : " << av_make_error_string(err_buf, AV_ERROR_MAX_STRING_SIZE, frameFinished) << std::endl;
                /* Convert frame to OpenCV matrix */
                sws_scale(sws_ctx, frame->data, frame->linesize, 0, frame->height, cvframe->data, cvframe->linesize);
                /* CV MODE */
                if(OPENCV_MODE)
                {
                    /* Create CV matrix and move to GPU memory */
                    cv::UMat* uimage = NULL;
                    uimage = new cv::UMat(cv::Mat(dst_height, dst_width, CV_8UC3, framebuf.data(), cvframe->linesize[0]).getUMat(cv::ACCESS_READ));
                    /* Display image */
                    cv::imshow("press ESC to exit 2", *uimage);
                    /* Wait for 1 ms to check for key perss */
                    if (cv::waitKey(1) == 0x1b)
                    {
                        FLAG_EXIT = true;
                        break;
                    }
                    /* Wait for some time before next frame, 10ms */
                    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time - 10));
                    /* Use SDL to display, Ref: https://github.com/rambodrahmani/ffmpeg-video-player/blob/master/tutorial02/tutorial02.c */
                }
                /* SDL MODE */
                if(SDL_MODE)
                {
                    /* sleep: usleep won't work when using SDL_CreateWindow */
                    //usleep(sleep_time);
                    /* Use SDL_Delay in milliseconds to allow for cpu scheduling */
                    if (VERBOSE_DEBUG)
                        std::cout << "Start SDL sleep: " << sleep_time - 10 << std::endl;
                    SDL_Delay(sleep_time - 10);
                    /* The simplest struct in SDL. It contains only four shorts. x, y which 
                     holds the position and w, h which holds width and height.It's important 
                     to note that 0, 0 is the upper-left corner in SDL. So a higher y-value 
                     means lower, and the bottom-right corner will have the coordinate x + w, y + h. */
                    SDL_Rect rect;
                    rect.x = 0;
                    rect.y = 0;
                    rect.w = codec_ctx->width;
                    rect.h = codec_ctx->height;
                    if (VERBOSE_DEBUG)
                    {
                        std::cout << "Frame " << av_get_picture_type_char(frame->pict_type);
                        std::cout << " (" << codec_ctx->frame_number << ") pts " << frame->pts;
                        std::cout << " dts " << frame->pkt_dts << " key_frame " << frame->key_frame;
                        std::cout << " [coded_picture_number " << frame->coded_picture_number;
                        std::cout << ", display_picture_number " << frame->display_picture_number;
                        std::cout << ", " << codec_ctx->width << "x" << codec_ctx->height << "]" << std::endl;
                    }

                    /* Update a rectangle with new pixel data */
                    SDL_UpdateTexture(
                        texture,
                        &rect,
                        cvframe->data[0],
                        cvframe->linesize[0]
                    );

                    /* Clear the current rendering target with the drawing color */
                    SDL_RenderClear(renderer);

                    /* Copy a portion of the texture to the current rendering target */
                    SDL_RenderCopy(
                        renderer,   /* the rendering context */
                        texture,    /* the source texture */
                        NULL,       /* the source SDL_Rect structure or NULL for the entire texture */
                        NULL        /* the destination SDL_Rect structure or NULL for the entire rendering */
                                    /* target; the texture will be stretched to fill the given rectangle */
                    );

                    /* Update the screen with any rendering performed since the previous call */
                    SDL_RenderPresent(renderer);
                }
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
            if (VERBOSE_DEBUG)
                std::cout << "stream index: " << packet->stream_index << "--audio frame--" << std::endl;
            /* Do audio stuff */
            /* Ref: https://github.com/leandromoreira/ffmpeg-libav-tutorial#audio---what-you-listen */
        }
        av_packet_unref(packet);
        if (FLAG_EXIT)
            break;
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

        std::cout << "saving frame " << dec_ctx->frame_number << std::endl;
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