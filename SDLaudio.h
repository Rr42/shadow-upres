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

constexpr uint32_t MAX_AUDIO_FRAME_SIZE = 192000U;
constexpr float VOLUME_LEVEL = 0.5f;

void packet_queue_init(PacketQueue* q);
int packet_queue_put(PacketQueue* q, AVPacket* pkt);
int refresh_video(void* opaque);
static int packet_queue_get(PacketQueue* q, AVPacket* pkt, int block);
int audio_decode_frame(AVCodecContext* aCodecCtx, uint8_t* audio_buf, int buf_size);
void audio_callback(void* userdata, Uint8* stream, int len);

extern bool FIRST_RUN_FLAG;
