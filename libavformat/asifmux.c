/* CS3505 Spring 2020
 * Mason Austin
 * .asif file muxer
 */

#include "avformat.h"
#include "rawenc.h"

/*
 * The text "asif"
 * Used as the first four bytes in an asif file
 */
const uint8_t ff_asif_text[4] = {
  'a', 's', 'i', 'f'
};

/**
 * Writes the header for a .asif file
 * This is the letters "asif" followed by sample rate and number of channels.
 * Samples per channel is missing as it cannot be calculated here
 * It is instead calculate in the encoder and is written immediately in the packet
 */
static int asif_write_header(AVFormatContext *s)
{
  // Write the letters "asif" to the file
  avio_write(s->pb, ff_asif_text, sizeof(ff_asif_text));
  // Write the sample rate to the file
  avio_wl32(s->pb, s->streams[0]->codecpar->sample_rate);
  // Write the number of channels to the file
  avio_wl16(s->pb, s->streams[0]->codecpar->channels);

  // Indicate success
  return 0;
}

/**
 * Writes the data from the packet to the output file
 */
static int asif_write_packet(AVFormatContext *s, AVPacket *pkt)
{
  // Write the data from the packet into the file
  avio_write(s->pb, pkt->data, pkt->size);
  return 0;
}

AVOutputFormat ff_asif_muxer = {
  .name         = "asif",
  .long_name    = NULL_IF_CONFIG_SMALL("ASIF audio file (CS 3505 Spring 20202)"),
  .audio_codec  = AV_CODEC_ID_ASIF,
  .video_codec  = AV_CODEC_ID_NONE,
  .write_packet = asif_write_packet,
  .write_header = asif_write_header,
  .flags        = AVFMT_NOTIMESTAMPS,
  .extensions   = "asif",
};
