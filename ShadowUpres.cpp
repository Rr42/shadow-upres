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
#include "SDLaudio.h"

std::atomic<int> thread_exit = 0;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/* TEST CODE - START */
float sine_freq = 200.0f;
float audio_volume = 1.0f;
float audio_frequency;

void SineAudioCallback(void* userdata, Uint8* stream, int len) {
    float* buf = (float*)stream;
    for (int i = 0; i < len / 4; ++i) {
        buf[i] = (float)(audio_volume * sin(2 * M_PI * i * audio_frequency));
    }
    return;
}

int main1(int argc, char* argv[])
{
    if (SDL_Init(SDL_INIT_AUDIO)) {
        return 1;
    }

    std::cout << "[SDL] Audio driver: " << SDL_GetCurrentAudioDriver() << std::endl;

    SDL_AudioSpec want, have;
    SDL_zero(want);

    want.freq = 5000;
    want.format = AUDIO_F32;
    want.channels = 2;
    want.samples = 4096;
    want.callback = SineAudioCallback;

    std::cout << "[SDL] Desired - frequency: " << want.freq
        << ", format: f " << SDL_AUDIO_ISFLOAT(want.format) << " s " << SDL_AUDIO_ISSIGNED(want.format) << " be " << SDL_AUDIO_ISBIGENDIAN(want.format) << " sz " << SDL_AUDIO_BITSIZE(want.format)
        << ", channels: " << (int)want.channels << ", samples: " << want.samples << std::endl;


    SDL_AudioDeviceID dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, SDL_AUDIO_ALLOW_ANY_CHANGE);

    if (!dev) {
        SDL_Quit();
        return 1;
    }


    std::cout << "[SDL] Desired - frequency: " << have.freq
        << ", format: f " << SDL_AUDIO_ISFLOAT(have.format) << " s " << SDL_AUDIO_ISSIGNED(have.format) << " be " << SDL_AUDIO_ISBIGENDIAN(have.format) << " sz " << SDL_AUDIO_BITSIZE(have.format)
        << ", channels: " << (int)have.channels << ", samples: " << have.samples << std::endl;

    audio_frequency = sine_freq / have.freq;

    SDL_PauseAudioDevice(dev, 0);
    SDL_Delay(10000);

    SDL_CloseAudioDevice(dev);
    SDL_Quit();

    return 0;
}
/* TEST CODE - END */
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

/* ShadowUpres core function */
int main(int argc, char** argv)
{
    char err_buf[AV_ERROR_MAX_STRING_SIZE];
    int retCode;

    //if (argc <= 1) 
    //{
    //    std::cerr << "[ERROR] Usage: " << argv[0] << " <input file>" << std::endl;
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
    std::cout << "[DEBUG] Listing compatable hardware types: | ";
    AVHWDeviceType type = AV_HWDEVICE_TYPE_NONE;
    while ((type = av_hwdevice_iterate_types(type)) != AV_HWDEVICE_TYPE_NONE)
        std::cout << av_hwdevice_get_type_name(type) << " | ";
    std::cout << std::endl;

    /* Initialize SDL */
    retCode = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER);
    if (retCode != 0)
    {
        /* Error while initializing SDL */
        std::cerr << "[ERROR] Could not initialize SDL - " << SDL_GetError() << std::endl ;
        return retCode;
    }

    std::cout << "[DEBUG] Listing compatable audio drivers and divices: " << std::endl;
    SDL_AudioQuit();
    int adrcount = SDL_GetNumAudioDrivers();
    const char* driver_name;
    for (int i = 0; i < adrcount; ++i)
    {
        driver_name = SDL_GetAudioDriver(i);
        if (SDL_AudioInit(driver_name))
        {
            std::cerr << "[ERROR] Audio driver failed to initialize: " << driver_name << std::endl;
            continue;
        }
        std::cout << "         Driver [" << i << "]: " << driver_name << std::endl;
        int adcount = SDL_GetNumAudioDevices(0);
        for (int i = 0; i < adcount; ++i)
            std::cout << "          Device [" << i << "]: " << SDL_GetAudioDeviceName(i, 0) << std::endl;
        SDL_AudioQuit();
    }

    /* Select audio driver to use */
    driver_name = SDL_GetAudioDriver(AUDIO_DRIVER_ID);
    retCode = SDL_AudioInit(driver_name);
    if (retCode)
    {
        std::cerr << "[ERROR] Audio driver failed to initialize: " << driver_name << std::endl;
        return retCode;
    }

    std::cout << "[AUDIO] Current audio driver: " << SDL_GetCurrentAudioDriver() << std::endl;
    std::cout << "[VIDEO] Current video driver: " << SDL_GetCurrentVideoDriver() << std::endl;

    /* Extract the decoder from video */
    static AVFormatContext* ifmt_ctx = NULL;
    retCode = avformat_open_input(&ifmt_ctx, filename, NULL, NULL);
    if (retCode < 0)
    {
        //av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
        std::cerr << "[ERROR] Cannot open input file" << std::endl;
        return retCode;
    }

    retCode = avformat_find_stream_info(ifmt_ctx, NULL);
    if (retCode < 0) 
    {
        //av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        std::cerr << "[ERROR] Cannot find stream information" << std::endl;
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
    if (vstrm_idx < 0)
    {
        std::cerr << "[ERROR] Failed unable to find video stream: vstrm_idx=" << vstrm_idx;
        return vstrm_idx;
    }
    if (astrm_idx < 0)
    {
        std::cout << "[WARNING] Warning unable to find audio stream: astrm_idx=" << astrm_idx;
        return astrm_idx;
    }

    /* Find the video decoder */
    AVCodecParameters* codec_strm_ctx = ifmt_ctx->streams[vstrm_idx]->codecpar;
    const AVCodec* codec = avcodec_find_decoder(codec_strm_ctx->codec_id);
    if (!codec)
    {
        std::cerr << "[ERROR] Video codec not found" << std::endl;
        exit(1);
    }
    /* Find the audio decoder */
    AVCodecParameters* acodec_strm_ctx = ifmt_ctx->streams[astrm_idx]->codecpar;
    const AVCodec* acodec = avcodec_find_decoder(acodec_strm_ctx->codec_id);
    if (!acodec)
    {
        std::cerr << "[ERROR] Audio codec not found" << std::endl;
        exit(1);
    }

    /* Create video codec context based on codec */
    AVCodecContext* codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx)
    {
        std::cerr << "[ERROR] Could not allocate video codec context" << std::endl;
        exit(1);
    }
    /* Create audio codec context based on acodec */
    AVCodecContext* acodec_ctx = avcodec_alloc_context3(acodec);
    if (!acodec_ctx)
    {
        std::cerr << "[ERROR] Could not allocate audio codec context" << std::endl;
        exit(1);
    }

    /* For some codecs, such as msmpeg4 and mpeg4, width and height
       MUST be initialized there because this information is not
       available in the bitstream. */

    /* Copy video codec parameters to codec context */
    retCode = avcodec_parameters_to_context(codec_ctx, codec_strm_ctx);
    if (retCode < 0)
    {
        std::cerr << "[ERROR] Failed to copy video decoder parameters to input decoder context for stream " << vstrm_idx << std::endl;
        return retCode;
    }
    /* Copy audio codec parameters to codec context */
    retCode = avcodec_parameters_to_context(acodec_ctx, acodec_strm_ctx);
    if (retCode < 0)
    {
        std::cerr << "[ERROR] Failed to copy audio decoder parameters to input decoder context for stream " << vstrm_idx << std::endl;
        return retCode;
    }

    /* Initialize the video codec context to use the given codec */
    retCode = avcodec_open2(codec_ctx, codec, NULL);
    if (retCode < 0)
    {
        std::cerr << "[ERROR] Failed to open video codec through avcodec_open2 for stream " << vstrm_idx << std::endl;
        return retCode;
    }
    /* Initialize the audio codec context to use the given acodec */
    retCode = avcodec_open2(acodec_ctx, acodec, NULL);
    if (retCode < 0)
    {
        std::cerr << "[ERROR] Failed to open audio codec through avcodec_open2 for stream " << vstrm_idx << std::endl;
        return retCode;
    }

    /* Set audio settings from codec info */
    SDL_AudioSpec wanted_spec, aspec;
    SDL_zero(wanted_spec);
    wanted_spec.freq = acodec_ctx->sample_rate;
    wanted_spec.format = acodec_ctx->sample_fmt;
    wanted_spec.channels = acodec_ctx->channels;
    //wanted_spec.silence = 0;
    wanted_spec.samples = SDL_AUDIO_BUFFER_SIZE;
    wanted_spec.callback = audio_callback;
    wanted_spec.userdata = acodec_ctx;
    std::cout << "[AUDIO] Audio codec info:" << std::endl;
    std::cout << "          want:" << std::endl;
    std::cout << "           freq: " << wanted_spec.freq << std::endl;
    std::cout << "           format: " << wanted_spec.format << std::endl;
    std::cout << "           channels: " << (int)wanted_spec.channels << std::endl;

    /* Open DSL audio */
    //retCode = SDL_OpenAudio(&wanted_spec, &aspec);
    SDL_AudioDeviceID adevID;
    adevID = SDL_OpenAudioDevice(
        SDL_GetAudioDeviceName(AUDIO_DEVICE_ID, 0),
        0,
        &wanted_spec,
        &aspec,
        SDL_AUDIO_ALLOW_ANY_CHANGE);
    std::cout << "          got:" << std::endl;
    std::cout << "           freq: " << aspec.freq << std::endl;
    std::cout << "           format: " << aspec.format << std::endl;
    std::cout << "           channels: " << (int)aspec.channels << std::endl;

    if (adevID < 0)
    {
        std::cerr << "[ERROR] SDL_OpenAudio: " << SDL_GetError() << std::endl;
        return adevID;
    }

    /* Unpause SDL audio */
    packet_queue_init(&audioq);
    //SDL_PauseAudio(0);
    SDL_PauseAudioDevice(adevID, 0);
    
    const int dst_height = codec_strm_ctx->height;
    const int dst_width = codec_strm_ctx->width;
    std::cout << "[VIDEO] Screen info [WxL]: " << codec_strm_ctx->width << "x" << codec_strm_ctx->height << std::endl;

    /* Create a SDL window with the specified position, dimensions, and flags */
    SDL_Window* screen = SDL_CreateWindow(
        "SDL Video Player",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        codec_ctx->width / 2,
        codec_ctx->height / 2,
        SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE
    );
    if (!screen)
    {
        /* Could not set video mode */
       std::cerr << "[ERROR] SDL: could not set video mode - exiting" << std::endl;
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

    std::cout << "[DEBUG] Video stream info:" << std::endl;
    std::cout << "         nb_frames: " << ifmt_ctx->streams[vstrm_idx]->nb_frames << std::endl;
    std::cout << "         duration: " << ifmt_ctx->streams[vstrm_idx]->duration << std::endl;
    std::cout << "         time_base (s): " << av_q2d(ifmt_ctx->streams[vstrm_idx]->time_base) << std::endl;
    std::cout << "         length (s): " << ifmt_ctx->streams[vstrm_idx]->duration * av_q2d(ifmt_ctx->streams[vstrm_idx]->time_base) << std::endl;
    std::cout << "         r_frame_rate: " << av_q2d(ifmt_ctx->streams[vstrm_idx]->r_frame_rate) << std::endl;
    std::cout << "         codec_tag: " << codec_strm_ctx->codec_tag << std::endl;
    std::cout << "         codec_id: " << codec_strm_ctx->codec_id << std::endl;
    std::cout << "         codec name: " << codec->name << std::endl;
    std::cout << "         codec_ctx id name: " << avcodec_find_decoder(codec_ctx->codec_id)->name << std::endl;
    std::cout << "        Audio stream info:" << std::endl;
    std::cout << "         nb_frames: " << ifmt_ctx->streams[astrm_idx]->nb_frames << std::endl;
    std::cout << "         duration: " << ifmt_ctx->streams[astrm_idx]->duration << std::endl;
    std::cout << "         time_base (s): " << av_q2d(ifmt_ctx->streams[astrm_idx]->time_base) << std::endl;
    std::cout << "         length (s): " << ifmt_ctx->streams[astrm_idx]->duration * av_q2d(ifmt_ctx->streams[astrm_idx]->time_base) << std::endl;
    std::cout << "         sample_rate: " << acodec_ctx->sample_rate << std::endl;
    std::cout << "         sample time (s): " << 1.0/acodec_ctx->sample_rate << std::endl;
    std::cout << "         codec_tag: " << acodec_strm_ctx->codec_tag << std::endl;
    std::cout << "         codec_id: " << acodec_strm_ctx->codec_id << std::endl;
    std::cout << "         codec name: " << acodec->name << std::endl;
    std::cout << "         codec_ctx id name: " << avcodec_find_decoder(acodec_ctx->codec_id)->name << std::endl;

    /* Allocate an AVPacket structure */
    AVPacket* packet = av_packet_alloc();
    if (!packet)
    {
        std::cerr << "[ERROR] Could not allocate video packet" << std::endl;
        return -1;
    }

    /* Allocate an AVFrame structures */
    AVFrame* frame = av_frame_alloc();
    if (!frame)
    {
        std::cerr << "[ERROR] Could not allocate video frame" << std::endl;
        return -1;
    }
    AVFrame* cvframe = av_frame_alloc();
    if (!cvframe)
    {
        std::cerr << "[ERROR] Could not allocate cv video frame" << std::endl;
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
    if (SDL_VIDEO_MODE)
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
    /* Vars. for general fuinctions */
    double vfps = 0;
    double afps = 0;
    long sleep_time = 0;
    /* Vars. for CV MODE */
    cv::UMat* uimage = NULL;
    /* Vars. for SDL MODE */
    SDL_Rect rect;
    /* Used to handle quit event */
    SDL_Event event;
    SDL_Thread* refresh_thread = SDL_CreateThread(refresh_video, NULL, NULL);
    const uint8_t* SDL_key_status;
    int* numkeys;
    int screen_w = codec_ctx->width;
    int screen_h = codec_ctx->height;
    while (av_read_frame(ifmt_ctx, packet) >= 0)
    {
        /* Is this a packet from the video stream? */
        if (packet->stream_index == vstrm_idx && !SKIP_VIDEO_PROCESSING)
        {
            if (VERBOSE_DEBUG | VERBOSE_DEBUG_VIDEO)
            {
                std::cout << "[VIDEO] stream index: " << packet->stream_index << " --video frame--" << std::endl;
                std::cout << "[VIDEO] packet size: " << packet->size << std::endl;
                std::cout << "[VIDEO] packet duration: " << packet->duration << std::endl;
                std::cout << "[VIDEO] packet length (s): " << packet->duration * av_q2d(ifmt_ctx->streams[vstrm_idx]->time_base) << std::endl;
                std::cout << "[VIDEO] packet position: " << packet->pos << std::endl;
                std::cout << "[VIDEO] packet buffer size: " << packet->buf->size << std::endl;
                std::cout << "[VIDEO] packet data size: " << AV_NUM_DATA_POINTERS << std::endl;
            }
            /* Decode video frame */
            av_packet_rescale_ts(packet, ifmt_ctx->streams[vstrm_idx]->time_base, codec_ctx->time_base);
            frameFinished = avcodec_send_packet(codec_ctx, packet);
            if (VERBOSE_DEBUG | VERBOSE_DEBUG_VIDEO)
                std::cout << "[VIDEO] avcodec_send_packet: " << frameFinished << " : " << av_make_error_string(err_buf, AV_ERROR_MAX_STRING_SIZE, frameFinished) << std::endl;

            /* Get video clip fps */
            vfps = av_q2d(ifmt_ctx->streams[vstrm_idx]->r_frame_rate);
            /* Get clip sleep time in ms (truncate) */
            sleep_time = static_cast<long>(1000.0 / vfps);
            if (VERBOSE_DEBUG | VERBOSE_DEBUG_VIDEO)
                std::cout << "[VIDEO] sleep_time: " << sleep_time << std::endl;
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
                    std::cout << "Error while decoding video." << std::endl;
                    return -1;
                }
                if (VERBOSE_DEBUG | VERBOSE_DEBUG_VIDEO)
                    std::cout << "[VIDEO] avcodec_receive_frame: " << frameFinished << " : " << av_make_error_string(err_buf, AV_ERROR_MAX_STRING_SIZE, frameFinished) << std::endl;
                /* Convert frame to OpenCV matrix */
                sws_scale(sws_ctx, frame->data, frame->linesize, 0, frame->height, cvframe->data, cvframe->linesize);
                /* CV MODE */
                if(OPENCV_MODE)
                {
                    /* Create CV matrix and move to GPU memory */
                    uimage = new cv::UMat(cv::Mat(dst_height, dst_width, CV_8UC3, framebuf.data(), cvframe->linesize[0]).getUMat(cv::ACCESS_READ));
                    /* Display image */
                    cv::imshow("press ESC to exit 2", *uimage);
                    delete uimage;
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
                if(SDL_VIDEO_MODE)
                {
                    //SDL_WaitEvent(&event);
                    SDL_PollEvent(&event);            
                    switch (event.type)
                    {
                    case REFRESH_EVENT:
                        /* The simplest struct in SDL. It contains only four shorts. x, y which
                         holds the position and w, h which holds width and height.It's important
                         to note that 0, 0 is the upper-left corner in SDL. So a higher y-value
                         means lower, and the bottom-right corner will have the coordinate x + w, y + h. */
                            rect.x = 0;
                            rect.y = 0;
                            rect.w = screen_w;
                            rect.h = screen_h;
                            if (VERBOSE_DEBUG | VERBOSE_DEBUG_VIDEO)
                            {
                                std::cout << "[VIDEO] Frame " << av_get_picture_type_char(frame->pict_type);
                                std::cout << " (" << codec_ctx->frame_number << ") pts " << frame->pts;
                                std::cout << " dts " << frame->pkt_dts << " key_frame " << frame->key_frame;
                                std::cout << " [coded_picture_number " << frame->coded_picture_number;
                                std::cout << ", display_picture_number " << frame->display_picture_number;
                                std::cout << ", " << screen_w << "x" << screen_h << "]" << std::endl;
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
                    
                            /* Use SDL_Delay in milliseconds to allow for cpu scheduling */
                            if (VERBOSE_DEBUG | VERBOSE_DEBUG_VIDEO)
                                std::cout << "[VIDEO] Start SDL video sleep: " << sleep_time - 10 << std::endl;
                            SDL_Delay(sleep_time - 10);
                            break;
                    case SDL_WINDOWEVENT:
                        /* [BUG] Cannot handle resizing (crashes above a size) (works with like commented) */
                        //SDL_GetWindowSize(screen, &screen_w, &screen_h);
                        //screen_w == codec_ctx->width
                        //screen_h == codec_ctx->height
                        break;
                    case SDL_KEYDOWN:
                    case SDL_KEYUP:
                        numkeys = new int(0);
                        SDL_key_status = SDL_GetKeyboardState(numkeys);
                        if (numkeys != NULL )
                            if (*numkeys >= SDL_SCANCODE_ESCAPE)
                                if (SDL_key_status[SDL_SCANCODE_ESCAPE])
                                {
                                    FLAG_EXIT = true;
                                    thread_exit = 1;
                                }
                        delete numkeys;
                        break;
                    case SDL_QUIT:
                        thread_exit = 1;
                    case BREAK_EVENT:
                        FLAG_EXIT = true;
                        break;
                    default:
                        break;
                    }

                    if (FLAG_EXIT)
                        break;
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
            if (VERBOSE_DEBUG | VERBOSE_DEBUG_AUDIO)
                std::cout << "[AUDIO] stream index: " << packet->stream_index << " --audio frame--" << std::endl;
            /* Do audio stuff */
            /* Ref: https://github.com/leandromoreira/ffmpeg-libav-tutorial#audio---what-you-listen */
            /* Get audio clip fps */
            afps = av_q2d(ifmt_ctx->streams[vstrm_idx]->r_frame_rate);
            if (VERBOSE_DEBUG | VERBOSE_DEBUG_AUDIO)
                std::cout << "[AUDIO] audio clip fps: " << afps << std::endl;
            /* Get clip sleep time in ms (truncate) */
            sleep_time = static_cast<long>(1000.0 / afps);
            if (VERBOSE_DEBUG | VERBOSE_DEBUG_AUDIO)
                std::cout << "[AUDIO] audio sleep_time: " << sleep_time << std::endl;
            /* SDL MODE */
            if (SDL_AUDIO_MODE)
            {
                //SDL_WaitEvent(&event);
                SDL_PollEvent(&event);
                switch (event.type)
                {
                case REFRESH_EVENT:
                    packet_queue_put(&audioq, packet);
                    /* Use SDL_Delay in milliseconds to allow for cpu scheduling */
                    if (VERBOSE_DEBUG | VERBOSE_DEBUG_AUDIO)
                        std::cout << "[AUDIO] Start SDL audio sleep: " << sleep_time - 10 << std::endl;
                    SDL_Delay(sleep_time - 10);
                    break;
                case SDL_KEYDOWN:
                case SDL_KEYUP:
                    numkeys = new int(0);
                    SDL_key_status = SDL_GetKeyboardState(numkeys);
                    if (numkeys != NULL)
                        if (*numkeys >= SDL_SCANCODE_ESCAPE)
                            if (SDL_key_status[SDL_SCANCODE_ESCAPE])
                            {
                                FLAG_EXIT = true;
                                thread_exit = 1;
                            }
                    delete numkeys;
                    break;
                case SDL_QUIT:
                    thread_exit = 1;
                case BREAK_EVENT:
                    FLAG_EXIT = true;
                    break;
                default:
                    break;
                }
            }

            if (FLAG_EXIT)
               break;
        }

        av_packet_unref(packet);
        if (FLAG_EXIT)
            break;
    }

    std::cout << "Cleaning up..." << std::endl;
    thread_exit = 1;
    if (SDL_AUDIO_MODE)
        SDL_CloseAudioDevice(adevID);
    SDL_Quit();
    av_frame_free(&cvframe);
    av_frame_free(&frame);
    av_packet_free(&packet);
    sws_freeContext(sws_ctx);
    avcodec_free_context(&codec_ctx);
    avcodec_free_context(&acodec_ctx);
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
        std::cerr << "[ERROR] Error sending a packet for decoding" << std::endl;
        exit(1);
    }

    while (retCode >= 0)
    {
        retCode = avcodec_receive_frame(dec_ctx, frame);
        if (retCode == AVERROR(EAGAIN) || retCode == AVERROR_EOF)
            return;
        else if (retCode < 0)
        {
            std::cerr << "[ERROR] Error during decoding" << std::endl;
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

static int refresh_video(void* opaque)
{
    thread_exit = 0;
    while (thread_exit == 0) {
        SDL_Event event;
        event.type = REFRESH_EVENT;
        SDL_PushEvent(&event);
        SDL_Delay(40);
    }
    thread_exit = 0;
    //Break
    SDL_Event event;
    event.type = BREAK_EVENT;
    SDL_PushEvent(&event);
    return 0;
}

void cleanUpOnFail(int retCode)
{
    SDL_Quit();
    exit(retCode);
}