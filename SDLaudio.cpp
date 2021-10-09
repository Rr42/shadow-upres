
#include "SDLaudio.h"

PacketQueue audioq;
bool FIRST_RUN_FLAG = true;

void packet_queue_init(PacketQueue *q)
{
  memset(q, 0, sizeof(PacketQueue));
  q->mutex = SDL_CreateMutex();
  q->cond = SDL_CreateCond();
}

int packet_queue_put(PacketQueue* q, AVPacket* pkt)
{

    AVPacketList* pkt1;
    AVPacket pkt_cpy;
    if (av_packet_ref(&pkt_cpy, pkt) < 0) {
        return -1;
    }
    pkt1 = static_cast<AVPacketList*>(av_malloc(sizeof(AVPacketList)));
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

    while (true)
    {
        if (thread_exit)
        {
            ret = -1;
            break;
        }

        /* Get first packet */
        pkt1 = q->first_pkt;

        if (VERBOSE_DEBUG | VERBOSE_DEBUG_AUDIO_DECODE)
            std::cout << "[DEBUG][PKT_QUE] " << q->nb_packets << std::endl;

        if (pkt1)
        {
            /* If packet is good change first packet to next packet */
            q->first_pkt = pkt1->next;

            /* If next packet is empty set it to NULL */
            if (!q->first_pkt)
                q->last_pkt = NULL;
            q->nb_packets--;
            q->size -= pkt1->pkt.size;

            if (VERBOSE_DEBUG | VERBOSE_DEBUG_AUDIO_DECODE)
                std::cout << "[DEBUG][PKT_QUE] " << q->nb_packets << std::endl;

            *pkt = pkt1->pkt;
            av_free(pkt1);
            ret = 1;
            break;
        }
        else if (!block)
        {
            ret = 0;
            break;
        }
        else
            SDL_CondWait(q->cond, q->mutex);
    }
    SDL_UnlockMutex(q->mutex);
    return ret;
}


static void decode(AVCodecContext* dec_ctx, AVPacket* pkt, AVFrame* frame, FILE* outfile)
{
    int i, ch;
    int ret, data_size;

    /* send the packet with the compressed data to the decoder */
    ret = avcodec_send_packet(dec_ctx, pkt);
    if (ret < 0) {
        fprintf(stderr, "Error submitting the packet to the decoder\n");
        exit(1);
    }

    /* read all the output frames (in general there may be any number of them */
    while (ret >= 0) {
        ret = avcodec_receive_frame(dec_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return;
        else if (ret < 0) {
            fprintf(stderr, "Error during decoding\n");
            exit(1);
        }
        data_size = av_get_bytes_per_sample(dec_ctx->sample_fmt);
        if (data_size < 0) {
            /* This should not occur, checking just for paranoia */
            fprintf(stderr, "Failed to calculate data size\n");
            exit(1);
        }
        for (i = 0; i < frame->nb_samples; i++)
            for (ch = 0; ch < dec_ctx->channels; ch++)
                fwrite(frame->data[ch] + data_size * i, 1, data_size, outfile);
    }
}

typedef struct WAV_HEADER {
    /* RIFF Chunk Descriptor */
    uint8_t RIFF[4] = { 'R', 'I', 'F', 'F' }; // RIFF Header Magic header
    uint32_t ChunkSize;                     // RIFF Chunk Size
    uint8_t WAVE[4] = { 'W', 'A', 'V', 'E' }; // WAVE Header
    /* "fmt" sub-chunk */
    uint8_t fmt[4] = { 'f', 'm', 't', ' ' }; // FMT header
    uint32_t Subchunk1Size = 16;           // Size of the fmt chunk
    uint16_t AudioFormat = 1; // Audio format 1=PCM,6=mulaw,7=alaw,     257=IBM
                              // Mu-Law, 258=IBM A-Law, 259=ADPCM
    uint16_t NumOfChan = 1;   // Number of channels 1=Mono 2=Sterio
    uint32_t SamplesPerSec = 16000;   // Sampling Frequency in Hz
    uint32_t bytesPerSec = 16000 * 2; // bytes per second
    uint16_t blockAlign = 2;          // 2=16-bit mono, 4=16-bit stereo
    uint16_t bitsPerSample = 16;      // Number of bits per sample
    /* "data" sub-chunk */
    uint8_t Subchunk2ID[4] = { 'd', 'a', 't', 'a' }; // "data"  string
    uint32_t Subchunk2Size;                        // Sampled data length
} wav_hdr;

int audio_decode_frame(AVCodecContext* aCodecCtx, uint8_t* audio_buf, int buf_size)
{
    if (VERBOSE_DEBUG | VERBOSE_DEBUG_AUDIO_DECODE)
        std::cout << "[DEBUG][AUDIO_DECODE] decode start" << std::endl;

    static AVPacket* pkt = NULL;
    static int audio_pkt_decode_done = 0;
    static AVFrame frame;

    int data_size = 0;

    if (VERBOSE_DEBUG | VERBOSE_DEBUG_AUDIO_DECODE)
        std::cout << "[DEBUG][AUDIO_DECODE] decode loop TOP" << std::endl;

    if (thread_exit)
        return -1;

    pkt = av_packet_alloc();
    if (pkt <= 0)
        return -1;

    int ret = 0;
    if (SDL_AUDIO_MODE)
        ret = packet_queue_get(&audioq, pkt, 1);
    else
        ret = packet_queue_get(&audioq, pkt, 0);

    if (VERBOSE_DEBUG | VERBOSE_DEBUG_AUDIO_DECODE)
        std::cout << "[DEBUG][AUDIO_DECODE] " << ret << std::endl;
    if (ret <= 0)
        return -1;

    if (VERBOSE_DEBUG | VERBOSE_DEBUG_AUDIO_DECODE)
    {
        std::cout << "[DEBUG][AUDIO_DECODE] pkt data size: " << AV_NUM_DATA_POINTERS << std::endl;
        std::cout << "[DEBUG][AUDIO_DECODE] pkt->size: " << pkt->size << std::endl;
    }

    /* Send packet to decoder */
    audio_pkt_decode_done = avcodec_send_packet(aCodecCtx, pkt);

    if (VERBOSE_DEBUG | VERBOSE_DEBUG_AUDIO_DECODE)
        std::cout << "[DEBUG][AUDIO_DECODE] audio_pkt_decode_done Sf: " << audio_pkt_decode_done << std::endl;

    int count = 0;
    while (audio_pkt_decode_done >= 0)
    {
        ++count;
        audio_pkt_decode_done = avcodec_receive_frame(aCodecCtx, &frame);
            
        if (VERBOSE_DEBUG | VERBOSE_DEBUG_AUDIO_DECODE)
            std::cout << "[DEBUG][AUDIO_DECODE] audio_pkt_decode_done Rf: " << audio_pkt_decode_done << std::endl;

        if (audio_pkt_decode_done == AVERROR(EAGAIN) || audio_pkt_decode_done == AVERROR_EOF)
            break;
        else if (audio_pkt_decode_done < 0)
        {
            std::cout << "[ERROR][AUDIO_DECODE] Error while decoding audio." << std::endl;
            return -1;
        }

        data_size = av_samples_get_buffer_size(NULL,
            aCodecCtx->channels,
            frame.nb_samples,
            aCodecCtx->sample_fmt,
            1);

        if (VERBOSE_DEBUG | VERBOSE_DEBUG_AUDIO_DECODE)
        {
            std::cout << "[DEBUG][AUDIO_DECODE] nb_samples: " << frame.nb_samples << std::endl;
            std::cout << "[DEBUG][AUDIO_DECODE] data_size: " << data_size << " | buf_size: " << buf_size << std::endl;
        }
            
        if (data_size <= 0)
            continue;

        assert(data_size*aCodecCtx->channels <= buf_size);

        /* If size of data from decoder is less than space in buffer */
        /* Append to buffer */
        for (int i = 0; i < data_size; ++i)
            for (int ch = 0; ch < aCodecCtx->channels; ++ch)
                audio_buf[i] = frame.data[ch][i]*VOLUME_LEVEL;
        //memcpy(audio_buffer + size_in_buf, frame.data[0], data_size); // [0] -> channel | get from aCodecCtx->channels
    }
    if (pkt->data)
        av_packet_free(&pkt);

    if (VERBOSE_DEBUG | VERBOSE_DEBUG_AUDIO_BUFFER)
        std::cout << "[DEBUG][AUDIO_DECODE] run count: " << count << std::endl;

    //std::ofstream outdata("./output.txt", std::ios_base::app);
    //if (!outdata) 
    //{ 
    //    // file couldn't be opened
    //    std::cerr << "Error: file could not be opened" << std::endl;
    //    exit(1);
    //}
    //outdata << "----------------------------------------" << std::endl  << "| ";
    //for (int i = 0; i < buf_size; ++i)
    //    outdata << (int)audio_buf[i] << " | ";
    //outdata << std::endl <<  "----------------------------------------" << std::endl;
    //outdata.close();

    //wav_hdr wav;
    //wav.ChunkSize = buf_size + sizeof(wav_hdr) - 8;
    //wav.Subchunk2Size = buf_size + sizeof(wav_hdr) - 44;
    //std::ofstream out;
    //if (FIRST_RUN_FLAG)
    //{
    //    out.open("test.wav", std::ios::binary);
    //    out.write(reinterpret_cast<const char*>(&wav), sizeof(wav));
    //    FIRST_RUN_FLAG = false;
    //}
    //else
    //{
    //    out.open("test.wav", std::ios::binary | std::ios_base::app);
    //}
    //int8_t d;
    //for (int i = 0; i < buf_size; ++i) 
    //{
    //    // TODO: read/write in blocks
    //    d = audio_buf[i];
    //    out.write(reinterpret_cast<char*>(&d), sizeof(int8_t));
    //}
    //out.close();

    /* We have data, return it and come back for more later */
    return data_size;
}

void audio_callback(void* userdata, Uint8* stream, int len)
{
    if (VERBOSE_DEBUG | VERBOSE_DEBUG_AUDIO)
        std::cout << "[AUDIO] audio_callback called" << std::endl;
    AVCodecContext* aCodecCtx = (AVCodecContext*)userdata;
    uint8_t* audio_buf = new uint8_t[len];
    unsigned int audio_buf_size = 0;
    int audio_size;

    if (VERBOSE_DEBUG | VERBOSE_DEBUG_AUDIO_DECODE)
        std::cout << "[DEBUG][AUDIO_DECODE] calling audio decode" << std::endl;

    audio_size = audio_decode_frame(aCodecCtx, stream, len);
    //SDL_MixAudio(stream, audio_buf, len, SDL_MIX_MAXVOLUME);

    if (VERBOSE_DEBUG | VERBOSE_DEBUG_AUDIO_DECODE)
        std::cout << "[DEBUG][AUDIO_DECODE] audio decode returned" << std::endl;

    if (audio_size < 0)
    {
        if (VERBOSE_DEBUG | VERBOSE_DEBUG_AUDIO)
            std::cout << "[AUDIO] failed to load data, loading 0s" << std::endl;
        /* If error, output silence */
        memset(stream, 0, len);
    }

    delete[] audio_buf;

    //if (VERBOSE_DEBUG | VERBOSE_DEBUG_AUDIO)
    //    std::cout << "[AUDIO] data written: " << audio_size << std::endl;

    //AVCodecContext* aCodecCtx = (AVCodecContext*)userdata;
    //int len1, audio_size;
    //
    //static uint8_t audio_buf[(MAX_AUDIO_FRAME_SIZE * 3) / 2];
    //static unsigned int audio_buf_size = 0;
    //static unsigned int audio_buf_index = 0;
    //
    //while (len > 0) {
    //    if (audio_buf_index >= audio_buf_size) {
    //        /* We have already sent all our data; get more */
    //        audio_size = audio_decode_frame(aCodecCtx, audio_buf, sizeof(audio_buf));
    //        if (audio_size < 0) {
    //            /* If error, output silence */
    //            audio_buf_size = 1024; // arbitrary?
    //            memset(audio_buf, 0, audio_buf_size);
    //        }
    //        else {
    //            audio_buf_size = audio_size;
    //        }
    //        audio_buf_index = 0;
    //    }
    //    len1 = audio_buf_size - audio_buf_index;
    //    if (len1 > len)
    //        len1 = len;
    //    memcpy(stream, (uint8_t*)audio_buf + audio_buf_index, len1);
    //    len -= len1;
    //    stream += len1;
    //    audio_buf_index += len1;
    //}
}