#include <platform.h>
#include <print.h>
#include <xccompat.h>
#include <string.h>
#include <xscope.h>
#include "audio_i2s.h"
#include "i2c.h"
#include "audio_clock_CS2100CP.h"
#include "audio_codec_CS4270.h"
#include "board_config.h"
#include "timer.h"

/* Number of input/output audio channels in the demo application */
#define AVB_DEMO_NUM_CHANNELS 4
/** The total number of media inputs (typically number of I2S input channels). */
#define AVB_NUM_MEDIA_INPUTS AVB_DEMO_NUM_CHANNELS
/** The total number of media outputs (typically the number of I2S output channels). */
#define AVB_NUM_MEDIA_OUTPUTS AVB_DEMO_NUM_CHANNELS
// This is the number of master clocks in a word clock
#define MASTER_TO_WORDCLOCK_RATIO 512

#define I2S_CLK_1 XS1_CLKBLK_3
#define I2S_CLK_2 XS1_CLKBLK_4

#if I2C_COMBINE_SCL_SDA
on tile[AVB_I2C_TILE]: port r_i2c = PORT_I2C;
#else
on tile[AVB_I2C_TILE]: struct r_i2c r_i2c = { PORT_I2C_SCL, PORT_I2C_SDA };
#endif

on tile[0]: out buffered port:32 p_fs[1] = { PORT_SYNC_OUT };
on tile[0]: i2s_ports_t i2s_ports =
{
  XS1_CLKBLK_3,
  XS1_CLKBLK_4,
  PORT_MCLK,
  PORT_SCLK,
  PORT_LRCLK
};

on tile[0]: out buffered port:32 p_aud_dout[AVB_DEMO_NUM_CHANNELS/2] = PORT_SDATA_OUT;
on tile[0]: in buffered port:32 p_aud_din[AVB_DEMO_NUM_CHANNELS/2] = PORT_SDATA_IN;

on tile[0]: out port p_audio_shared = PORT_AUDIO_SHARED;


[[distributable]] void audio_hardware_setup(void)
{
  audio_clock_CS2100CP_init(r_i2c, MASTER_TO_WORDCLOCK_RATIO);
  audio_codec_CS4270_init(p_audio_shared, 0xff, 0x48, r_i2c);
  audio_codec_CS4270_init(p_audio_shared, 0xff, 0x49, r_i2c);

  while (1) {
    select {
    }
  }
}

void provide_pll_clock(out buffered port:32 p_fs[])
{
  int toggle = 0;
  int initial = 0;
  int ctr = 0;

  while(1) {
    p_fs[0] <: toggle;
    if (!initial) {
      delay_ticks_longlong(100100);
      initial = 1;
    }
    else
      delay_microseconds(521); //520832

    ctr++;
    if (ctr == 2) {
      ctr = 0;
      toggle = ~toggle;
    }
  }
}

int main(void)
{
  par
  {
    on tile[0]: provide_pll_clock(p_fs);
    on tile[AVB_I2C_TILE]: [[distribute]] audio_hardware_setup();

    // AVB - Audio
    on tile[0]:
    {
      i2s_master(i2s_ports,
                 p_aud_din, AVB_NUM_MEDIA_INPUTS,
                 p_aud_dout, AVB_NUM_MEDIA_OUTPUTS,
                 MASTER_TO_WORDCLOCK_RATIO,
                 0);
    }
  }
    return 0;
}
