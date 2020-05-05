//
//  ModulateVivoxIntegration.hpp
//  ModulateVivox
//
//  Created by Carter Huffman on 8/23/19.
//  Copyright © 2019 Modulate. All rights reserved.
//

#ifndef ModulateVivoxIntegration_hpp
#define ModulateVivoxIntegration_hpp

#include <stdio.h>
#include <chrono>
#include <iostream>
#include <cmath>
#include "modulate/modulate.h"

#include "wav_logger.hpp"

class ModulateVivoxIntegration {
private:
  modulate_parameters params;
  void* voice_skin;
  void* voice_skin_helper;
  float* float_buffer;

  // Logging storage
  double* input_times;
  double* output_times;
  size_t log_ptr = 0;

  std::chrono::time_point<std::chrono::high_resolution_clock> t1;
  std::chrono::time_point<std::chrono::high_resolution_clock> t2;

  ThreadedWavLogger input_wav_logger;
  ThreadedWavLogger output_wav_logger;

  // VivoxBase is a class to manage interaction with the vivox servers
  // This likely isn't very interesting to investigate, as most apps
  // will already have their own setup for talking to vivox
  void* vivox_base_ptr;
  // The vivox_config_setup function shows how to hook Modulate into vivox's low-level
  // raw audio callbacks, in order to apply voice skins while chatting
  void vivox_config_setup();

  std::atomic<bool> realtime_echo_running;
  short* conversion_buffer;
  size_t conversion_write_ptr = 0;
  size_t conversion_read_ptr = 0;

  // Vivox-SDK comptible functions to do Modulate voice conversion
  // and enable realtime echo
  static void modulate_convert_before_audio_sent(void* callback_handle, const char* session_group_handle, const char* initial_target_uri, short* pcm_frames, int pcm_frame_count, int audio_frame_rate, int channels_per_frame, int speaking);
  static void modulate_before_audio_rendered(void *callback_handle, const char *session_group_handle, const char *initial_target_uri, short *pcm_frames, int pcm_frame_count, int audio_frame_rate, int channels_per_frame, int is_silence);

public:
  ModulateVivoxIntegration(unsigned int segment_size,
                           void* voice_skin,
                           const char* log_dir);
  ~ModulateVivoxIntegration();

  void convert(short *pcm_frames,
               int pcm_frame_count,
               int audio_frame_rate,
               int channels_per_frame,
               int speaking);

  // TODO, make these threadsafe and wait-free for the convert function
  void set_voice_skin(void* new_voice_skin) {voice_skin = new_voice_skin;};
  void set_radio_strength(float new_radio_strength) {params.radio_strength = new_radio_strength;};
  void set_presence_strength(float new_presence_strength) {params.presence_strength = new_presence_strength;};
  void set_bass_booster_strength(float new_bass_booster_strength) {params.bass_booster_strength = new_bass_booster_strength;};
  void set_intimidator_strength(float new_intimidator_strength) {params.intimidator_strength = new_intimidator_strength;};
  void set_helm_strength(float new_helm_strength) {params.helm_strength = new_helm_strength;};
  void set_vivid_strength(float new_vivid_strength) {params.vivid_strength = new_vivid_strength;};
  double get_average_performance_ratio();

  void start_realtime_echo();
  void end_realtime_echo();

  // Vivox Connection Management
  // Some base functions to enable the ModulateChat demo app to connect to vivox servers
  // These are probably not interesting to investigation, as most applications have more
  // sophisticated chat connection / session management functionality already
  void connect();
  void login();
  void add_session(const char* channel_name, bool is_echo = false);
  void remove_session(const char* channel_name, bool is_echo = false);
  bool check_connected();
  bool check_logged_in();
  void start();
  void stop();

};

#endif /* ModulateVivoxIntegration_hpp */
