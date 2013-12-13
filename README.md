sw_avb_audio_pll_loopback
.........................

A standalone application to test AVB audio PLL board in the loopback configuration

Firmware Overview
=================

The purpose of this repo is to test the AVB audio slice board on a standalone basis.
The test application uses the loopback configuration to test the audio provided as
input to the analog channels is collected at the output channels.

Utilizes the modules developed for sc_avb_dc.

Required software (dependencies)
================================

  * module_i2c_simple (part of sc_i2c repo)
  * module_avb_audio (provided as a part of this repo. This is a simplified version of module_avb_audio available in sc_avb_dc)

