#pragma once

#include "ShadowUpres.h"

#include <fstream>

typedef struct PacketQueue {
    AVPacketList* first_pkt, * last_pkt;
    int nb_packets;
    int size;
    SDL_mutex* mutex;
    SDL_cond* cond;
} PacketQueue;

extern PacketQueue audioq;

//constexpr uint16_t SDL_AUDIO_BUFFER_SIZE = 8192U;
constexpr uint16_t SDL_AUDIO_BUFFER_SIZE = 1024U;
constexpr uint32_t MAX_AUDIO_FRAME_SIZE = 192000U;

void packet_queue_init(PacketQueue* q);
int packet_queue_put(PacketQueue* q, AVPacket* pkt);
int refresh_video(void* opaque);
static int packet_queue_get(PacketQueue* q, AVPacket* pkt, int block);
int audio_decode_frame(AVCodecContext* aCodecCtx, uint8_t* audio_buf, int buf_size);
void audio_callback(void* userdata, Uint8* stream, int len);

extern bool FIRST_RUN_FLAG;

extern uint8_t* global_audio_overflow_buffer;
extern size_t data_in_global_audio_buffer;