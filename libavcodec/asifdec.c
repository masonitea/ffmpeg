/* CS 3505, Spring 2020
 * Mason Austin
 * .asif file decoder
 */

#include "libavutil/attributes.h"
#include "libavutil/float_dsp.h"
#include "avcodec.h"
#include "bytestream.h"
#include "internal.h"
#include "mathops.h"

/**
 * Takes data from a given avpkt and stores it into the frame
 * Since .asif files are delta encoded, the data from the packet must be decoded
 * Once a sample is decoded, it is placed into the frame
 */
static int asif_decode(AVCodecContext *avctx, void *data, int *got_frame_ptr, AVPacket *avpkt)
{
  int ret;                 // Indicates success for allocating data
  AVFrame *frame;          // The frame that needs to be populated
  int samples_per_channel; // Number of samples for each channel
  const uint8_t *pktData;  // Data coming in from the packet
  uint8_t *dst;            // Where samples are stored
  uint8_t prev = 0;        // Used for delta decoding

  // Assign some of the variables accordingly
  pktData = avpkt->data;
  frame = data;
  samples_per_channel = avpkt->size / avctx->channels;
  
  // Populate the frame samples, channel, format, ect. so that
  // the frame has the correct format information
  frame->nb_samples = samples_per_channel;
  frame->channels   = avctx->channels;
  frame->format     = AV_SAMPLE_FMT_U8P;

  // Allocate data for the buffer
  if((ret = ff_get_buffer(avctx, frame, 0)) < 0){
    return ret;
  }

  // Move in the data samples
  for(int i = 0; i < avctx->channels; i++){
    dst = frame->extended_data[i];
    prev = 0;
    // Decodes each sample before storing it
    // This works by taking the previous result and adding it to the current delta.
    for(int j = 0; j < samples_per_channel; j++){
      uint8_t delta = pktData[i * samples_per_channel + j];
      uint8_t result = delta + prev;
      dst[j] = result;
      prev = result;
    }
  }

  // Indicate success
  *got_frame_ptr = 1;
  return avpkt->size;
}

AVCodec ff_asif_decoder = {
  .name           = "asif",
  .long_name      = NULL_IF_CONFIG_SMALL("Audio file (CS 3505 SPRING 20202)"),
  .type           = AVMEDIA_TYPE_AUDIO,
  .id             = AV_CODEC_ID_ASIF,
  .decode         = asif_decode,
  .capabilities   = AV_CODEC_CAP_DR1
};
