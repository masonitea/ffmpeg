/* CS 3505, Spring 2020
 * Mason Austin
 * .asif file demuxer
 */

#include "avformat.h"
#include "internal.h"
#include "pcm.h"
#include "libavutil/log.h"
#include "libavutil/opt.h"
#include "libavutil/avassert.h"

/**
 * Private data structure used for demuxing
 */
typedef struct ASIFAudioDemuxerContext {
  // Data taken from header of the file
  int sample_rate;
  int channels;
  int samples_per_channel;
} ASIFAudioDemuxerContext;

/**
 * Reads an ASIF file header and stores the header data in a private struct
 * Also creates an AVStream and initializes its AVCodecParameters
 * If the first four bytes do not equal {'a', 's', 'i', 'f'}, an AVERROR_INVALIDDATA is thrown
 */
static int asif_read_header(AVFormatContext *s)
{
  AVIOContext *pb  = s->pb;
  ASIFAudioDemuxerContext *my_data;
  AVStream *st;

  // Set private data field
  my_data = s->priv_data;;

  // Ensure first four bytes are equal to "asif"
  if (avio_rl32(pb) != MKTAG('a', 's', 'i', 'f')){
    av_log(s, AV_LOG_ERROR, "invalid format in ASIF header\n");
    return AVERROR_INVALIDDATA;
  }

  // Read the sample rate
  my_data->sample_rate = avio_rl32(pb);

  // Read the channels
  my_data->channels = avio_rl16(pb);

  // Read samples per channels
  my_data->samples_per_channel = avio_rl32(pb);

  // Set up the stream and its parameters
  st = avformat_new_stream(s, NULL);
  st->codecpar->codec_type            = AVMEDIA_TYPE_AUDIO;
  st->codecpar->codec_id              = AV_CODEC_ID_ASIF;
  st->codecpar->sample_rate           = my_data->sample_rate;
  st->codecpar->channels              = my_data->channels;
  st->codecpar->block_align           = my_data->channels;
  st->codecpar->bits_per_coded_sample = 8;
  st->codecpar->format                = AV_SAMPLE_FMT_U8P;

  return 0;
}

/**
 * Creates one large packet with all data from the buffer.
 * Returns the return value of av_get_packet which indicates success of the creation
 * If the packet failed, the packet is completely wiped
 * If the packet succeeds, it is simply sent to the decoder
 */ 
static int asif_read_packet(AVFormatContext *s, AVPacket *pkt)
{
  AVIOContext *pb = s->pb;
  int ret;          // Indicates success/failure
  int size = 0;     // Size of packet
  ASIFAudioDemuxerContext *my_data;
  
  // Set Variable Data
  my_data = s->priv_data;
  size = my_data->samples_per_channel * my_data->channels;

  // Allocate Data for packet
  ret = av_get_packet(pb, pkt, size);
  
  // If an error has occured
  if(ret < 0){
    av_packet_unref(pkt);
    av_log(s, AV_LOG_INFO, "leaving read packet early, error from av_get_packet\n");
    return ret;
  }
  pkt->stream_index = 0;

  // Indicate Success/Failure
  return ret;
}

AVInputFormat ff_asif_demuxer = {
  .name     = "asif",
  .long_name = NULL_IF_CONFIG_SMALL("ASIF audio file (CS 3505 Spring 20202)"),
  .priv_data_size = sizeof(ASIFAudioDemuxerContext),
  .read_header    = asif_read_header,
  .read_packet    = asif_read_packet,
  .extensions     = "asif",
};
  
