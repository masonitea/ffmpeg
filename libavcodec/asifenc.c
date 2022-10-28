/* CS 3505, Spring 2020
 * Mason Austin
 * .asif file encoder
 */

#include "libavutil/float_dsp.h"
#include "libavutil/attributes.h"
#include "avcodec.h"
#include "bytestream.h"
#include "internal.h"
#include "mathops.h"

uint8_t calculateDelta(uint8_t, uint8_t, int*);

/**
 * A node used to create a LinkedList of bytes
 */
struct Node
{
  uint8_t data;
  struct Node *next;
};

/**
 * Contains Data for the priv_data section in this encoder
 */
typedef struct asif_encoder_data
{
  // Flags
  int firstSample[50];  // If the first sample for each channel has been received
  int nullFrame;        // If send_frame sent in a NULL frame
  int sentPacket;       // If the one huge packet has been sent through
  int* overflowed;      // If the calculateDelta method overflowed

  // Data/Calculation
  int samplesPerChannel;        // Will be written at the start of the packet and for several calculations
  struct Node* channelData[50]; // An array of pointers. Each pointer is the start of a channel's data.
  struct Node* previousSample[50]; // A previous sample used for delta calculation. 

  // OLD (currently kept for asif_encode_frame)
  uint8_t previous; // Previous sample that was calculated
} asif_encoder_data;


/**
 * Initializes asif_encoder_data and hooks it to the AVCodecContext
 * Each int value is set to 0
 * Each array has its values initialized in the loop
 */
static av_cold int asif_encode_init(AVCodecContext *avctx)
{ 
  struct Node *node;
  asif_encoder_data *my_data;
  int* value;

  // Allocate memory and set its value to 0
  value = av_malloc(sizeof(int));
  (*value) = 0;

  // asif encoder struct
  my_data = avctx->priv_data;

  // Initialize the struct's values
  my_data->nullFrame         = 0;
  my_data->sentPacket        = 0;
  my_data->samplesPerChannel = 0;
  my_data->overflowed        = value;

  // Initialize the pointers/lists
  for(int i = 0; i < avctx->channels; i++){
    node = av_malloc(sizeof(struct Node));
    node->next = NULL;

    my_data->channelData[i]    = node;
    my_data->previousSample[i] = node;
    my_data->firstSample[i]    = 0;
  }
  
  // Set the frame_size
  avctx->frame_size = 100000;
  return 0;
}

/**
 * Creates one large packet if all data is ready.
 * Writes the samples per channel first, then follows the classic asif file format
 * Since all data was stored in nodes that were manually allocated, they must be manually freed after usage.
 * 
 * Returns a 0 indicates success
 * Returns a AVERROR(EAGAIN) means more data is required before the packet can be sent
 * Returns a AVERROR_EOF means the packet has already been sent
 */
static int asif_receive_packet(AVCodecContext *avctx, AVPacket *avpkt)
{
  asif_encoder_data *my_data = avctx->priv_data; // private data struct
  int totalSamples;                              // total samples in the packet
  int ret;                                       // captures ff_alloc_packet2 return value
  uint8_t *dst;                                  // stores the AVPacket's data pointer
  int count;                                     // Where the next byte should be stored in dst
  struct Node* current;                          // Used to traverse nodes
  unsigned int val = my_data->samplesPerChannel; // Used to write the samples per channel
  uint8_t prev;                                  // Previous node's value

  // If the packet has been sent
  if(my_data->sentPacket == 1){
    return AVERROR_EOF;
  }

  // If the packet is ready to be sent
  if(my_data->nullFrame == 1){
    totalSamples = my_data->samplesPerChannel * avctx->channels; // How many total samples there are
    
    // Allocate Packet Data (it's totalsamples + 4 since 4 bytes is needed for the samples per channel)
    if((ret = ff_alloc_packet2(avctx, avpkt, totalSamples+4, totalSamples+4)) < 0){
      return ret;
    }
    dst = avpkt->data;

    // Samples per channel is written first
    // This code is taken from avio_wl32 from aviobuf.c
    dst[0] = (uint8_t) val;
    dst[1] = (uint8_t)(val >> 8);
    dst[2] = (uint8_t)(val >> 16);
    dst[3] = val >> 24;

    // Write data from linkedlists to dst
    for(int i = 0; i < avctx->channels; i++){
      // First sample for a channel is stored as it is
      count = 0;
      current = my_data->channelData[i];
      dst[val*i + 4 + count] = current->data;
      count++;
      while(current->next != NULL){
	// Writes the RAW DATA. DELETE LATER.
	//dst[count] = current->data;
	//count++;
	//current = current->next;

	// Writes the DELTA value
	prev = current->data;
	current = current->next;
	dst[val*i + 4 + count] = calculateDelta(current->data, prev, my_data->overflowed);
	count++;
      }
      // RAW DATA. DELETE LATER. dst[count] = current->data;
      // Writes the Delta value for the last packet
      dst[val*i + 4 + count] = calculateDelta(current->data, prev, my_data->overflowed);
      count++;
    }
    
    // Free all of the node pointer's memory after extracting the data
    av_free(my_data->overflowed);
    for(int i = 0; i < avctx->channels; i++){
      while(my_data->channelData[i]->next != NULL){
	current = my_data->channelData[i]->next;
	av_free(my_data->channelData[i]);
	my_data->channelData[i] = current;
      }
    }
    
    // Set flag
    my_data->sentPacket = 1;

    // Indicate success
    return 0;
  }

  // Need more data before creating packet
  return AVERROR(EAGAIN);
}

/**
 * Will take data from an AVFRAME and store it into the avctx's priv_data parameter
 * Each sample is stored as it comes in and is stored by using a Linked List
 * If a null pointer comes in, a flag is set letting receive_packet know all data has been received
 */
static int asif_send_frame(AVCodecContext *avctx, const AVFrame *frame)
{
  uint8_t sample;
  asif_encoder_data *my_data = avctx->priv_data;

  // If the frame is null, set the nullFrame flag
  if(frame == NULL){
    my_data->nullFrame = 1;
    return 0;
  }
  // If the frame is not null, the frame contains data that must be stored
  else{
    // A loop that stores all data from all channels
    for(int i = 0; i < frame->channels; i++){
      uint8_t *rawData = frame->extended_data[i]; // This channels data
      // A loop that stores all data from one channel
      for(int j = 0; j < frame->nb_samples; j++){
	sample = rawData[j]; // The sample
	// If this is the first sample. Avoids creating a new node (which was does in initialization)
	if(my_data->firstSample[i] == 0){
	  my_data->firstSample[i] = 1; // Set the flag
	  my_data->previousSample[i]->data = sample;
	}
	// If this is not the first sample. 
	else{
	  // Allocate a new node
	  struct Node* newNode;
	  newNode = av_malloc(sizeof(struct Node));
	  newNode->data = sample;
	  newNode->next = NULL;
	  
	  // Link the new node
	  my_data->previousSample[i]->next = newNode;
	  my_data->previousSample[i] = newNode;
	}
      }
    }
  }

  // Update samplesPerChannel after receiving this frame.
  my_data->samplesPerChannel = my_data->samplesPerChannel + frame->nb_samples;

  // Indicate success
  return 0;
}

/**
 * Calculates the Delta for two given uint8_t
 * This delta is treated as a signed integer and utilizes wrapping around the wheel
 * The value is clamped if it goes outside the range of a signed integer (-128 - 127)
 * The overflow from the clamping is stored in the pointer overflow which is passed in
 * Overflow is added to the delta in the beginning, if there was no overflow last time it should just be 0.
 */
uint8_t calculateDelta(uint8_t sample, uint8_t prev, int* overflow){
  // Get the raw delta
  uint8_t delta = sample - prev;
  // Add overflow to the delta
  if((*overflow) >= 0){
    delta = delta + (*overflow);
  }
  else{
    delta = delta - (*overflow);
  }

  // If the delta is positive, make sure it is clamped to 127
  if(sample >= prev){
    if(delta > 127){
      (*overflow) = delta - 127;
      delta = 127;
    }
  }
  // If the delta is negative, make sure it is clamped to 128
  else{
    if(delta < 128){
      (*overflow) = 128 - delta;
      delta = 128;
    }
  }
  
  // Return the delta after it has been clamped (if needed)
  return delta;
}

AVCodec ff_asif_encoder = {
  .id             = AV_CODEC_ID_ASIF,
  .type           = AVMEDIA_TYPE_AUDIO, 
  .name           = "asif",
  .long_name      = NULL_IF_CONFIG_SMALL("ASIF audio file (CS 3505 Spring 20202)"),
  .priv_data_size = sizeof(asif_encoder_data),
  .init           = asif_encode_init,
  .encode2        = NULL, 
  .send_frame     = asif_send_frame,
  .receive_packet = asif_receive_packet,
  .capabilities   = AV_CODEC_CAP_DELAY,
  .sample_fmts    = (const enum AVSampleFormat[]){ AV_SAMPLE_FMT_U8P, 
                                                   AV_SAMPLE_FMT_NONE },
};
