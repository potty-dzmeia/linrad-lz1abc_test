// Copyright (c) <2016> <Daniel Estevez>
//
// Permission is hereby granted, free of charge, to any person
// obtaining a copy of this software and associated documentation
// files (the "Software"), to deal in the Software without restriction,
// including without limitation the rights to use, copy, modify,
// merge, publish, distribute, sublicense, and/or sell copies of
// the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
// OR OTHER DEALINGS IN THE SOFTWARE.

// DESCRIPTION
//
//  When using Linrad with this file, the SDR receiver and Linrad will be synced
//  to the same frequency.
//  - Changing the frequency in Linrad changes the frequenc of the Transceiver using Hamlib rigctld.
//  - Changing the frequency in the Transceiver using its own controls changes the frequency in Linrad.
//

// INSTALLATION
//
// To generate a Windows version under Windows:
//   1. Rename :
//      users_rigctrld.c    --> wusers_hwaredriver.c
//      users_rigctrld_ex.c --> users_extra.c in
//   2. Install Hamlib for Windows. This will install the hamlib headers and
//      library files in a directory of your choice.
//      This will also install rigctld.exe which is a server program that
//      Linrad will connect to on startup in order to control your transceiver.
//   3. Compile Linrad as usual making sure that the compiler can find
//      Hamlib headers and library files.
//   4. Copy the following Hamlib DLL files to the Linrad directory:
//      - libhamlib-4.dll
//      - libusb-1.0.dll

// USAGE
//
//  1. Run rigctld.exe
//     - Run rigctld.exe using the foolowing command in the command prompt:
//       rigctld -m <Rigmodel_number> -r <Com port>
//       For example when having FTDX10 connected to Com6 use: rigctld -m 1042 -r COM6
//     - To check that rigctld.exe is working properly, run "rigctl -m 2 f". This should
//       return thecurrent dial frequency on your radio.
//  2. Run Linrad
//     - Run Linrad and check that the frequency moves accordingly as you tune around
//       in Linrad and on your transceiver.

#include <hamlib/rig.h>
#include <hamlib/riglist.h>

extern void new_center_frequency_v2(double input_value);
extern void add_mix1_cursor(int num);
extern double frequency_scale_offset; // [100*kHz] Lowest frequency of the whole available waterfall spectrum. It changes when the center frequency is changed (i.e. inside the frequency control box).Width of the waterfall is determined by the sampling rate.
extern unsigned int sc[MAX_SC];
extern double mix1_selfreq[MAX_MIX1]; // [Hz] Selected frequency in the visible waterfall (this the actual frequency that is audiable)  (counted from frequency_scale_offset)
extern float mix1_lowest_fq;          // [Hz] Lowest frequency in the visible waterfall (counted from frequency_scale_offset)
extern float mix1_highest_fq;         // [Hz] Highest frequency in the visible waterfall (counted from frequency_scale_offset)

static int enable_panadapter = 0;
static RIG *rig = NULL;

void users_open_devices(void);
void users_close_devices(void);
void panadapter_update_rig_freq(void);
void new_center_frequency_v2(double new_freq_MHz);
boolean set_linrad_freq(double freq_MHz);
double get_linrad_frequency(void);

void panadapter_update_rig_freq(void)
{
  static double rig_freq = 23000000.0; // in Hz
  static rmode_t rig_mode = RIG_MODE_USB;
  static double linrad_freq = 23000000.0;        // in Hz
  static boolean block_frequency_change = FALSE; // to avoid frequency loops when hwfreq is outside waterfall range

  double rig_freq_current; // in Hz
  rmode_t rig_mode_current;
  pbwidth_t width;
  double linrad_freq_current; // in Hz

  if (!enable_panadapter)
    return;

  if (rig_get_freq(rig, RIG_VFO_CURR, &rig_freq_current) != RIG_OK)
  {
    printf("rig_get_freq() failed\n");
    return;
  }

  if (rig_get_mode(rig, RIG_VFO_CURR, &rig_mode_current, &width) != RIG_OK)
  {
    printf("rig_get_mode() failed\n");
    return;
  }

  linrad_freq_current = get_linrad_frequency(); // in Hz

  // round to nearest Hz
  linrad_freq_current = rint(linrad_freq_current);
  rig_freq_current = rint(rig_freq_current);

  // Rig frequency has changed - update Linrad
  if ((rig_freq_current != rig_freq || rig_mode_current != rig_mode) && block_frequency_change == FALSE)
  {
    printf("Rig Frequency change from %f to %f\n", rig_freq, rig_freq_current);
    printf("----------------------------------\n");

    if (set_linrad_freq(rig_freq_current) == FALSE) // If false: hwfreq was outside the waterfall range and the center frequency was set instead.
    {
      block_frequency_change = TRUE;
      return;
    }

    rig_freq = rig_freq_current;
    rig_mode = rig_mode_current;
    linrad_freq = rig_freq; // keep local track of linrad frequency
    return;
  }
  // Linrad frequency has changed - update Rig
  else if (linrad_freq_current != linrad_freq)
  {
    if (block_frequency_change == TRUE)
    {

      block_frequency_change = FALSE;
      linrad_freq = linrad_freq_current;
      return;
    }

    linrad_freq = linrad_freq_current;
    rig_set_freq(rig, RIG_VFO_CURR, (freq_t)linrad_freq);
    rig_freq = linrad_freq;

    return;
  }
}

// Returns the hardware frequency in Hz (i.e. the center frequency visualized in the Baseband Window)
double get_linrad_frequency(void)
{
  if (rx_mode == MODE_SSB)
    return (mix1_selfreq[0] + bg.bfo_freq) + 100000 * frequency_scale_offset;
  else
    return mix1_selfreq[0] + 100000 * frequency_scale_offset;
}

// Sets the hardware frequency (i.e. the center frequency visualized in the Baseband Window)
boolean set_linrad_freq(double freq_Hz)
{
  double new_mix1_selfreq; // in Hz

  printf("Setting Linrad hw frequency to %f Hz\n", freq_Hz);

  // The hardware frequency (i.e. the center frequency visualized in the Baseband Window) is calculated the following way (for more info see  frequency_readout()):
  // --------> hwfreq=0.001*(mix1_selfreq[0])+100*frequency_scale_offset;
  // In order to set the hwfreq to be the same as the rig freq, we need to set mix1_selfreq[0] taking into account the frequency_scale_offset
  if (rx_mode == MODE_SSB)
    new_mix1_selfreq = 1000.0 * ((freq_Hz - bg.bfo_freq) / 1000.0 - 100.0 * frequency_scale_offset);
  else
    new_mix1_selfreq = 1000.0 * (freq_Hz / 1000.0 - 100.0 * frequency_scale_offset);

  // If the hwfreq is outside the current visible waterfall range, set the center frequency to be the "freq_Hz"
  if (new_mix1_selfreq <= mix1_lowest_fq || new_mix1_selfreq >= mix1_highest_fq)
  {
    if (new_mix1_selfreq <= mix1_lowest_fq)
      // print the difference
      printf("Requested frequency %f Hz is below the lowest waterfall frequency %f Hz. difference is %f Hz\n", new_mix1_selfreq, mix1_lowest_fq, mix1_lowest_fq - new_mix1_selfreq);
    else
      printf("Requested frequency %f Hz is above the highest waterfall frequency %f Hz. difference is %f Hz\n", new_mix1_selfreq, mix1_highest_fq, new_mix1_selfreq - mix1_highest_fq);

    printf("mix1_lowest_fq is %f Hz; mix1_highest_fq is %f Hz; Difference is %f Hz\n", mix1_lowest_fq, mix1_highest_fq, mix1_highest_fq - mix1_lowest_fq);
    printf("frequency_scale_offset is %f in [100*kHz]\n", frequency_scale_offset);

    // Set the center frequency inside the Frequency Control Box
    // Such that the requested hwfreq "freq_Hz" is at the center of the visible waterfall
    // We have to take into account cases where the visible water fall is set such that:
    // - center frequency is lower than the lowest visible waterfall frequency
    // - center frequency is higher than the highest visible waterfall frequency
    // - center frequency is inside the visible waterfall frequency range

    // find the center of the visible waterfall frequency range
    double waterfall_center_freq_Hz = 0.5 * (mix1_lowest_fq + mix1_highest_fq) + 100000 * frequency_scale_offset;
    // calculate the difference between visible "waterfall_center_freq_Hz" and the overall spectrum center frequency "fg.passband_center"
    double center_freq_difference_Hz = fg.passband_center * 1000000.0 - waterfall_center_freq_Hz;
    // center_freq_difference_Hz is negative when fg.passband_center is lower than waterfall_center_freq_Hz

    // calculate the new center frequency to be set inside the Frequency Control Box
    double new_center_freq_MHz = (freq_Hz + center_freq_difference_Hz) / 1000000.0;

    new_center_frequency_v2(new_center_freq_MHz);
    printf("Set Linrad center frequency to %f MHz to have hw frequency at %f Hz\n", new_center_freq_MHz, freq_Hz);
    // new_center_frequency_v2(freq_Hz / 1000000.0); // convert to MHz

    return FALSE;
  }

  // To Do: check if new_center_frequency_v2() and mix1_selfreq[0] update could be combined in a single call to avoid redundant updates

  mix1_selfreq[0] = new_mix1_selfreq;
  add_mix1_cursor(0);      // update cursor position
  sc[SC_SHOW_CENTER_FQ]++; // force update of frequency display
  return TRUE;
}

// Sets the center frequency inside the Frequency Control Box
// Note: This is a copy paste of new_center_frequency() from freq_control.c , with the only difference being that it takes input value as argument. Make sure to update this function in case of changes in new_center_frequency()
void new_center_frequency_v2(double new_freq_MHz)
{
  double dt1;
  if (new_freq_MHz < 0)
    goto errinp;
  new_freq_MHz *= 10000L;
  new_freq_MHz = rint(new_freq_MHz);
  new_freq_MHz = new_freq_MHz / 10000L;
  if ((ui.converter_mode & CONVERTER_USE) != 0)
  {
    if ((ui.converter_mode & CONVERTER_LO_BELOW) != 0)
    {
      if ((ui.converter_mode & CONVERTER_UP) == 0)
      {
        dt1 = new_freq_MHz - converter_offset_mhz;
      }
      else
      {
        dt1 = new_freq_MHz + converter_offset_mhz;
      }
    }
    else
    {
      dt1 = converter_offset_mhz - new_freq_MHz;
    }
  }
  else
  {
    dt1 = new_freq_MHz;
  }
  if (dt1 < 0)
    goto errinp;
  if (ui.rx_addev_no == PERSEUS_DEVICE_CODE)
  {
    if (dt1 * 1000000 + 0.5 >= PERSEUS_SAMPLING_CLOCK / 2)
    {
    errinp:;
      sc[SC_SHOW_CENTER_FQ]++;
      return;
    }
  }
  fg.passband_center = dt1;
  set_hardware_rx_frequency();
  sc[SC_SHOW_CENTER_FQ]++;
  make_modepar_file(GRAPHTYPE_FG);
}

void users_close_devices(void)
{
  if (enable_panadapter && rig != NULL)
  {
    rig_close(rig);
    rig_cleanup(rig);
    rig = NULL;
  }

  enable_panadapter = 0;
}

void users_open_devices(void)
{
  if (enable_panadapter || rig != NULL)
    printf("Panadapter support already enabled\n");

  rig_set_debug_level(RIG_DEBUG_ERR);
  rig = rig_init(RIG_MODEL_NETRIGCTL);
  if (rig == NULL)
  {
    printf("rig_init() failed\n");
    enable_panadapter = 0;
    return;
  }

  if (rig_open(rig) != RIG_OK)
  {
    printf("rig_open() failed\n");
    rig_cleanup(rig);
    enable_panadapter = 0;
    rig = NULL;
    return;
  }

  enable_panadapter = 1;
  printf("Panadapter support enabled via rigctld\n");
}

void userdefined_u(void) {};
void userdefined_q(void) {};
void update_users_rx_frequency(void) {};
void users_eme(void) {};
void show_user_parms(void) {};
void mouse_on_users_graph(void) {};
void init_users_control_window(void) {};
void users_init_mode(void) {};

void users_set_band_no(void) {};
#include "wse_sdrxx.c"
