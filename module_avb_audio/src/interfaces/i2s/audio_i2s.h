#ifndef __AUDIO_I2S_CTL_H__
#define __AUDIO_I2S_CTL_H__ 1

#define I2S_LOOPBACK    1

/*
  This unit provides functionality to communicate from AVB media fifos to
  an audio codec using the I2S digital audio interface format.
*/

#include <xclib.h>
#include <xccompat.h>
#ifdef __XC__


typedef struct i2s_ports_t {
  clock mclk;
  clock bclk;
  in port p_mclk;
  out buffered port:32 p_bclk;
  out buffered port:32 p_lrclk;
} i2s_ports_t;

// By enabling this, all channels are filled with an increasing counter
// value instead of the samples themselves.  Useful to check the channel
// synchronization using a network monitor

#define SAMPLE_COUNTER_TEST 0

#define I2S_SINE_TABLE_SIZE 100

extern unsigned int i2s_sine[I2S_SINE_TABLE_SIZE];

void i2s_master_configure_ports(REFERENCE_PARAM(i2s_ports_t, p),
                                out buffered port:32 (&?p_dout)[num_out],
                                unsigned num_out,
                                in buffered port:32 (&?p_din)[num_in],
                                unsigned num_in);

#pragma unsafe arrays
inline void i2s_master_upto_4(const clock mclk,
                              clock bclk,
                              out buffered port:32 p_bclk,
                              out buffered port:32 p_lrclk,
                              out buffered port:32 (&?p_dout)[],
                              int num_out,
                              in buffered port:32 (&?p_din)[],
                              int num_in,
                              int master_to_word_clock_ratio)
{
  int mclk_to_bclk_ratio = master_to_word_clock_ratio / 64;
  unsigned int bclk_val;
  unsigned int lrclk_val = 0xFFFFFFFF;

#if SAMPLE_COUNTER_TEST
  unsigned int sample_counter=0;
#endif

  // This is the master timing clock for the audio system.  Its value is sent
  // to the input and output fifos and is converted into presentation time for
  // clock recovery.
  timer tmr;

#ifdef I2S_SYNTH_FROM
  int sine_count[8] = {0};
  int sine_inc[8] = {0x080, 0x100, 0x180, 0x200, 0x100, 0x100, 0x100, 0x100};
#endif

  // You can output 32 mclk ticks worth of bitclock at a time.
  // So the ratio between the master clock and the word clock will affect
  // how many bitclocks outputs you have to do per word and also the
  // length of the bitclock w.r.t the master clock.
  // In every case you will end up with 32 bit clocks per word.
  switch (mclk_to_bclk_ratio)
  {
  case 2:
    bclk_val = 0xaaaaaaaa; // 10
    break;
  case 4:
    bclk_val = 0xcccccccc; // 1100
    break;
  case 8:
    bclk_val = 0xf0f0f0f0; // 11110000
    break;
  default:
    // error - unknown master clock/word clock ratio
    return;
  }




  // This sections aligns the ports so that the dout/din ports are
  // inputting and outputting in sync,
  // setting the t variable at the end sets when the lrclk will change
  // w.r.t to the bitclock.

  for (int i=0;i<num_out>>1;i++)
    p_dout[i] @ 32 <: 0;

  for (int i=0;i<num_in>>1;i++)
    asm ("setpt res[%0], %1" : : "r"(p_din[i]), "r"(63));

  p_lrclk @ 31 <: 0xFFFFFFFF;

  for (int j=0;j<2;j++) {
    for (int i=0;i<mclk_to_bclk_ratio;i++)  {
      p_bclk <: bclk_val;
    }
  }


  for (int i=0;i<num_out>>1;i++)
    p_dout[i] <: 0;

  // the unroll directives in the following loops only make sense if this
  // function is inlined into a more specific version
  while (1) {

	  unsigned int timestamp;

	  //unsigned int active_fifos = media_input_fifo_enable_req_state();

#pragma xta label "i2s_master_loop"

#ifdef I2S_SYNTH_FROM
    for (int k=I2S_SYNTH_FROM;k<num_in>>1;k++) {
      sine_count[k] += sine_inc[k];
      if (sine_count[k] > I2S_SINE_TABLE_SIZE * 256)
        sine_count[k] -= I2S_SINE_TABLE_SIZE * 256;
    }
#endif

    tmr :> timestamp;

    for (int j=0;j<2;j++) {
#pragma xta endpoint "i2s_master_lrclk_output"
    	// This assumes that there are 32 BCLKs in one half of an LRCLK
    	p_lrclk <: lrclk_val;
    	lrclk_val = ~lrclk_val;

#pragma loop unroll
      for (int k=0;k<mclk_to_bclk_ratio;k++) {

#pragma xta endpoint "i2s_master_bclk_output"
        p_bclk <: bclk_val;
#if I2S_LOOPBACK
        unsigned int sample_in;
#endif //I2S_LOOPBACK
        if (k < num_in>>1) {
#if SAMPLE_COUNTER_TEST
        	if (active_fifos & (1 << (j+k*2))) {
            media_input_fifo_push_sample(input_fifos[j+k*2], sample_counter, timestamp);
        	} else {
        	  media_input_fifo_flush(input_fifos[j+k*2]);
        	}
#else
#if I2S_LOOPBACK == 0
          unsigned int sample_in;
#endif //I2S_LOOPBACK
#pragma xta endpoint "i2s_master_sample_input"
          asm volatile("in %0, res[%1]":"=r"(sample_in):"r"(p_din[k]));

#ifdef I2S_SYNTH_FROM
          if (k >= I2S_SYNTH_FROM) {
            sample_in = i2s_sine[sine_count[k]>>8];
          }
#endif
#if I2S_LOOPBACK
          sample_in = (bitrev(sample_in) >> 8);
#else  //I2S_LOOPBACK
          if (active_fifos & (1 << (j+k*2))) {
            media_input_fifo_push_sample(input_fifos[j+k*2], sample_in, timestamp);
          } else {
            media_input_fifo_flush(input_fifos[j+k*2]);
          }
#endif //I2S_LOOPBACK
#endif
        }

        if (k < num_out>>1) {
          unsigned int sample_out;
#if I2S_LOOPBACK
          sample_out = sample_in;
#else  //I2S_LOOPBACK
          sample_out = media_output_fifo_pull_sample(output_fifos[j+k*2],
                                                     timestamp);
#endif //I2S_LOOPBACK
          sample_out = bitrev(sample_out << 8);
#pragma xta endpoint "i2s_master_sample_output"
          p_dout[k] <: sample_out;
        }
      } // end: for (int k=0;k<mclk_to_bclk_ratio;k++)
    } // end: for (int j=0;j<2;j++)

#if SAMPLE_COUNTER_TEST
    sample_counter++;
#endif

#if I2S_LOOPBACK == 0
    media_input_fifo_update_enable_ind_state(active_fifos, 0xFFFFFFFF);
#endif //I2S_LOOPBACK

  }
}



#pragma unsafe arrays
static inline void i2s_master(i2s_ports_t &ports,
                              in buffered port:32 (&?p_din)[],
                              int num_in,
                              out buffered port:32 (&?p_dout)[],
                              int num_out,
                              int master_to_word_clock_ratio,
                              int clk_ctl_index)
{
  i2s_master_configure_ports(ports,
                             p_dout,
                             num_out>>1,
                             p_din,
                             num_in>>1);

  if (num_in <= 4 && num_out <= 4) {
    i2s_master_upto_4(ports.mclk,
                      ports.bclk,
                      ports.p_bclk,
                      ports.p_lrclk,
                      p_dout,
                      num_out,
                      p_din,
                      num_in,
                      master_to_word_clock_ratio);
  }
}

#endif // __XC__

#endif // __AUDIO_I2S_CTL_H__ 1

