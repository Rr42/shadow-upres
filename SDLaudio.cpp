
#include "SDLaudio.h"

PacketQueue audioq;

void packet_queue_init(PacketQueue *q) {
  memset(q, 0, sizeof(PacketQueue));
  q->mutex = SDL_CreateMutex();
  q->cond = SDL_CreateCond();
}

int packet_queue_put(PacketQueue* q, AVPacket* pkt) {

    AVPacketList* pkt1;
    AVPacket pkt_cpy;
    if (av_packet_ref(&pkt_cpy, pkt) < 0) {
        return -1;
    }
    pkt1 = (AVPacketList*)av_malloc(sizeof(AVPacketList));
    if (!pkt1)
        return -1;
    pkt1->pkt = pkt_cpy;
    pkt1->next = NULL;


    SDL_LockMutex(q->mutex);

    if (!q->last_pkt)
        q->first_pkt = pkt1;
    else
        q->last_pkt->next = pkt1;
    q->last_pkt = pkt1;
    q->nb_packets++;
    q->size += pkt1->pkt.size;
    SDL_CondSignal(q->cond);

    SDL_UnlockMutex(q->mutex);
    return 0;
}

static int packet_queue_get(PacketQueue* q, AVPacket* pkt, int block)
{
    AVPacketList* pkt1;
    int ret;

    SDL_LockMutex(q->mutex);

    for (;;) {

        if (thread_exit) {
            ret = -1;
            break;
        }

        pkt1 = q->first_pkt;
        if (pkt1) {
            q->first_pkt = pkt1->next;
            if (!q->first_pkt)
                q->last_pkt = NULL;
            q->nb_packets--;
            q->size -= pkt1->pkt.size;
            *pkt = pkt1->pkt;
            av_free(pkt1);
            ret = 1;
            break;
        }
        else if (!block) {
            ret = 0;
            break;
        }
        else {
            SDL_CondWait(q->cond, q->mutex);
        }
    }
    SDL_UnlockMutex(q->mutex);
    return ret;
}

int audio_decode_frame(AVCodecContext* aCodecCtx, uint8_t* audio_buf, int buf_size) {

    static AVPacket* pkt = av_packet_alloc();
    static uint8_t* audio_pkt_data = NULL;
    static int audio_pkt_size = 0;
    static AVFrame frame;

    int len1, data_size = 0;

    for (;;) {
        if (thread_exit) {
            return -1;
        }

        audio_pkt_size = avcodec_send_packet(aCodecCtx, pkt);
        while (audio_pkt_size > 0) {
            int got_frame = 0;
            len1 = avcodec_receive_frame(aCodecCtx, &frame);
            if (!(len1 == AVERROR(EAGAIN) || len1 == AVERROR_EOF))
                got_frame = 1;
            if (len1 < 0) {
                /* if error, skip frame */
                audio_pkt_size = 0;
                break;
            }
            audio_pkt_data += len1;
            audio_pkt_size -= len1;
            data_size = 0;
            if (got_frame) {
                data_size = av_samples_get_buffer_size(NULL,
                    aCodecCtx->channels,
                    frame.nb_samples,
                    aCodecCtx->sample_fmt,
                    1);
                assert(data_size <= buf_size);
                memcpy(audio_buf, frame.data[0], data_size);
            }
            if (data_size <= 0) {
                /* No data yet, get more frames */
                continue;
            }
            /* We have data, return it and come back for more later */
            return data_size;
        }
        if (pkt->data)
            av_packet_free(&pkt);

        pkt = av_packet_alloc();
        if (packet_queue_get(&audioq, pkt, 1) < 0) {
            return -1;
        }
        audio_pkt_data = pkt->data;
        audio_pkt_size = pkt->size;
    }
}

void audio_callback(void* userdata, Uint8* stream, int len) {
    if (VERBOSE_DEBUG | VERBOSE_DEBUG_AUDIO)
        std::cout << "[AUDIO] audio_callback called" << std::endl;
    AVCodecContext* aCodecCtx = (AVCodecContext*)userdata;
    int len1, audio_size;

    static uint8_t audio_buf[(MAX_AUDIO_FRAME_SIZE * 3) / 2];
    static unsigned int audio_buf_size = 0;
    static unsigned int audio_buf_index = 0;

    while (len > 0) {
        if (audio_buf_index >= audio_buf_size) {
            /* We have already sent all our data; get more */
            audio_size = audio_decode_frame(aCodecCtx, audio_buf, sizeof(audio_buf));
            if (audio_size < 0) {
                /* If error, output silence */
                audio_buf_size = 1024; // arbitrary?
                memset(audio_buf, 0, audio_buf_size);
            }
            else {
                audio_buf_size = audio_size;
            }
            audio_buf_index = 0;
        }
        len1 = audio_buf_size - audio_buf_index;
        if (len1 > len)
            len1 = len;
        memcpy(stream, (uint8_t*)audio_buf + audio_buf_index, len1);
        len -= len1;
        stream += len1;
        audio_buf_index += len1;
    }
}