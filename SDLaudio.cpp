
#include "SDLaudio.h"

PacketQueue audioq;
bool FIRST_RUN_FLAG = true;
uint8_t* global_audio_overflow_buffer;
size_t data_in_global_audio_buffer = 0;

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


static void decode(AVCodecContext* dec_ctx, AVPacket* pkt, AVFrame* frame,
    FILE* outfile)
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
    //static AVPacket* pkt = av_packet_alloc();
    static AVPacket* pkt = NULL;
    static int audio_pkt_decode_done = 0;
    static AVFrame frame;

    size_t data_size = 0;

    uint8_t* audio_buffer = new uint8_t[buf_size]();
    size_t buffer_size = buf_size;
    size_t size_in_buf = 0;

    if (!audio_buffer)
        exit(0);

    if (VERBOSE_DEBUG | VERBOSE_DEBUG_AUDIO_DECODE)
        std::cout << "[DEBUG][AUDIO_DECODE] decode loop TOP" << std::endl;

    if (thread_exit)
        return -1;

    bool skip_packet_processing = false;

    /* Get data from global buffer */
    if (VERBOSE_DEBUG | VERBOSE_DEBUG_AUDIO_BUFFER)
    {
        std::cout << "data_in_global_audio_buffer: " << data_in_global_audio_buffer << std::endl;
        std::cout << "buf_size: " << buf_size << std::endl;
    }
    
    if (data_in_global_audio_buffer > 0)
    {
        if (data_in_global_audio_buffer > buf_size)
        {
            if (VERBOSE_DEBUG | VERBOSE_DEBUG_AUDIO_BUFFER)
                std::cout << "too much data in buffer" << std::endl;
            /* If data in global buffer is more than required */
            skip_packet_processing = true;
            memcpy(audio_buffer + size_in_buf, global_audio_overflow_buffer, buffer_size);
            size_in_buf += buffer_size;
            data_in_global_audio_buffer -= buffer_size;
            uint8_t* temp_buffer = new uint8_t[data_in_global_audio_buffer];
            memcpy(temp_buffer, global_audio_overflow_buffer + buffer_size, data_in_global_audio_buffer);
            delete[] global_audio_overflow_buffer;
            global_audio_overflow_buffer = temp_buffer;
        }
        else if (data_in_global_audio_buffer <= buf_size)
        {
            if (VERBOSE_DEBUG | VERBOSE_DEBUG_AUDIO_BUFFER)
                std::cout << "prefilling data from buffer" << std::endl;

            if (data_in_global_audio_buffer < buf_size)
                skip_packet_processing = false;
            else
                skip_packet_processing = true;

            memcpy(audio_buffer + size_in_buf, global_audio_overflow_buffer, data_in_global_audio_buffer);
            size_in_buf += data_in_global_audio_buffer;
            data_in_global_audio_buffer = 0;
            delete[] global_audio_overflow_buffer;
        }
    }

    if (!skip_packet_processing)
    {
        pkt = av_packet_alloc();
        if (pkt <= 0)
            return -1;

        int ret = packet_queue_get(&audioq, pkt, 1);
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
    }
    
    /*FILE* outfile = fopen("./output1.txt", "w");
    decode(aCodecCtx, pkt, &frame,
        outfile);
    fclose(outfile);*/

    int count = 0;
    while (audio_pkt_decode_done >= 0 && !skip_packet_processing)
    {
        ++count;
        audio_pkt_decode_done = avcodec_receive_frame(aCodecCtx, &frame);
            
        if (VERBOSE_DEBUG | VERBOSE_DEBUG_AUDIO_DECODE)
            std::cout << "[DEBUG][AUDIO_DECODE] audio_pkt_decode_done Rf: " << audio_pkt_decode_done << std::endl;

        if (audio_pkt_decode_done == AVERROR(EAGAIN) || audio_pkt_decode_done == AVERROR_EOF)
            break;
        else if (audio_pkt_decode_done < 0)
        {
            std::cout << "Error while decoding audio." << std::endl;
            return -1;
        }

        data_size = av_samples_get_buffer_size(NULL,
            aCodecCtx->channels,
            frame.nb_samples,
            aCodecCtx->sample_fmt,
            1);

        if (VERBOSE_DEBUG | VERBOSE_DEBUG_AUDIO_DECODE)
            std::cout << "[DEBUG][AUDIO_DECODE] data_size: " << data_size  << " | " << buf_size << std::endl;
            
        if (data_size <= 0)
            continue;

        /*FILE* outfile = fopen("./output.mp3", "a");
        decode(aCodecCtx, pkt, &frame,
            outfile);
        fclose(outfile);*/

        //assert(data_size <= buf_size);

        if (data_size <= buffer_size - size_in_buf)
        {
            /* If size of data from decoder is less than space in buffer */
            /* Append to buffer */
            memcpy(audio_buffer + size_in_buf, frame.data[0], data_size); // [0] -> channel | get from aCodecCtx->channels
            size_in_buf += data_size;
        }
        else if (buffer_size - size_in_buf > 0)
        {
            /* If cannot write all data fill buffer and exit */
            memcpy(audio_buffer + size_in_buf, frame.data[0], buffer_size - size_in_buf);

            if (VERBOSE_DEBUG | VERBOSE_DEBUG_AUDIO_BUFFER)
                std::cout << "buffer overflow, storing in global backup" << std::endl;
            
            if (data_in_global_audio_buffer == 0)
            {
                if (VERBOSE_DEBUG | VERBOSE_DEBUG_AUDIO_BUFFER)
                {
                    std::cout << "allocating size in global backup: " << static_cast<size_t>(buffer_size) * 10 << std::endl;
                    std::cout << "data_size: " << data_size << std::endl;
                }
                global_audio_overflow_buffer = new uint8_t[static_cast<size_t>(data_size) * GLOBAL_AUDIO_BUFFER_SIZE_FACTOR];
            }

            memcpy(global_audio_overflow_buffer, frame.data[0] + (buffer_size - size_in_buf), data_size - (buffer_size - size_in_buf));
            data_in_global_audio_buffer += data_size - (buffer_size - size_in_buf);
            size_in_buf += buffer_size - size_in_buf;

            break;
        }
        else
        {
            if (VERBOSE_DEBUG | VERBOSE_DEBUG_AUDIO_BUFFER)
                std::cout << "buffer overflow, storing all in global backup" << std::endl;

            if (data_in_global_audio_buffer == 0)
                global_audio_overflow_buffer = new uint8_t[static_cast<size_t>(data_size) * GLOBAL_AUDIO_BUFFER_SIZE_FACTOR];

            memcpy(global_audio_overflow_buffer + data_in_global_audio_buffer, frame.data[0], data_size);
            data_in_global_audio_buffer += buffer_size;
        }

        //memcpy(audio_buf, frame.data[0], data_size);
        //memcpy(audio_buf, frame.data[0], data_size);
        //if (size_in_buf >= buffer_size)
        //    break;
    }
    if (!skip_packet_processing && pkt->data)
        av_packet_free(&pkt);


    if (VERBOSE_DEBUG | VERBOSE_DEBUG_AUDIO_BUFFER)
        std::cout << "run count: " << count << std::endl;

    //pkt = av_packet_alloc();
    //if (packet_queue_get(&audioq, pkt, 1) < 0) {
    //    return -1;
    //}
    //audio_pkt_data = pkt->data;
    //audio_pkt_size = pkt->size;

    //std::ofstream outdata("./output.txt", std::ios_base::app);
    //if (!outdata) { // file couldn't be opened
    //    std::cerr << "Error: file could not be opened" << std::endl;
    //    exit(1);
    //}
    //outdata << "----------------------------------------" << std::endl  << "| ";
    //for (int i = 0; i < buffer_size; ++i)
    //    outdata << (int)audio_buffer[i] << " | ";
    //outdata << std::endl <<  "----------------------------------------" << std::endl;
    //outdata.close();

    //wav_hdr wav;
    //wav.ChunkSize = buffer_size + sizeof(wav_hdr) - 8;
    //wav.Subchunk2Size = buffer_size + sizeof(wav_hdr) - 44;
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
    //for (int i = 0; i < buffer_size; ++i) {
    //    // TODO: read/write in blocks
    //    d = audio_buffer[i];
    //    out.write(reinterpret_cast<char*>(&d), sizeof(int8_t));
    //}
    //out.close();

    memcpy(audio_buf, audio_buffer, buf_size);

    delete[] audio_buffer;

    /* We have data, return it and come back for more later */
    return data_size;
}

void audio_callback(void* userdata, Uint8* stream, int len)
{
    if (VERBOSE_DEBUG | VERBOSE_DEBUG_AUDIO)
        std::cout << "[AUDIO] audio_callback called" << std::endl;
    AVCodecContext* aCodecCtx = (AVCodecContext*)userdata;
    //uint8_t* audio_buf = new uint8_t[len];
    unsigned int audio_buf_size = 0;
    int audio_size;

    if (VERBOSE_DEBUG | VERBOSE_DEBUG_AUDIO_DECODE)
        std::cout << "[DEBUG][AUDIO_DECODE] calling audio decode" << std::endl;

    audio_size = audio_decode_frame(aCodecCtx, stream, len);

    if (VERBOSE_DEBUG | VERBOSE_DEBUG_AUDIO_DECODE)
        std::cout << "[DEBUG][AUDIO_DECODE] audio decode returned" << std::endl;

    if (audio_size < 0)
    {
        if (VERBOSE_DEBUG | VERBOSE_DEBUG_AUDIO)
            std::cout << "[AUDIO] failed to load data, loading 0s" << std::endl;
        /* If error, output silence */
        memset(stream, 0, len);
    }

    ////delete[] audio_buf;

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