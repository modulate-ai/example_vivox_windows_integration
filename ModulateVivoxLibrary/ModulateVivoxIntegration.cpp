//
//  ModulateVivoxIntegration.cpp
//  ModulateVivox
//
//  Created by Carter Huffman on 8/23/19.
//  Copyright © 2019 Modulate. All rights reserved.
//

#include "ModulateVivoxIntegration.hpp"
#include "secret.h" // issuer and secret key
#include "vivox/include/VxcTypes.h" // vx_sdk_config_t

// Note: the VivoxBase class is not included in this example, as it contains code
// distributed as an example application with the Vivox SDK.  This class handles
// typical Vivox setup for voice chat as done in the example commandline application,
// with a small change to include a pointer to the ModulateVivoxIntegration class that
// owns it.  This is done so that the realtime voice conversion callbacks, which are
// given a pointer to the VivoxBase class, can reference the voice skins and other
// objects in ModulateVivoxIntegration - as show in e.g.
// modulate_convert_before_audio_sent
#include "VivoxBase.hpp"

#define MAX_SAMPLES 2048
#define LOGSIZE 100
#define EXPECTED_SAMPLE_RATE 48000
#define MODULATE_CONVERSION_BUFFER_SIZE 4800

ModulateVivoxIntegration::ModulateVivoxIntegration(unsigned int max_segment_size,
                                                   void* starting_voice_skin,
                                                   const char* log_dir) :
  params(modulate_build_default_parameters_struct()),
  float_buffer(new float[MAX_SAMPLES]),
  input_times(new double[LOGSIZE]),
  output_times(new double[LOGSIZE]),
  voice_skin(starting_voice_skin),
  input_wav_logger(48000, 48000, log_dir, "input_log"),
  output_wav_logger(48000, 48000, log_dir, "output_log"),
  realtime_echo_running(false),
  conversion_buffer(new short[MODULATE_CONVERSION_BUFFER_SIZE])
{
  input_wav_logger.start_logging_thread();
  output_wav_logger.start_logging_thread();

  modulate_voice_skin_helper_create(&voice_skin_helper, max_segment_size);
  modulate_voice_skin_helper_reset(voice_skin_helper, EXPECTED_SAMPLE_RATE);

  std::fill_n(input_times, LOGSIZE, 1.0);
  std::fill_n(output_times, LOGSIZE, 1.0);

  t1 = std::chrono::high_resolution_clock::now();
  t2 = std::chrono::high_resolution_clock::now();

  vivox_base_ptr = new VivoxBase(this);
  vivox_config_setup();
}

ModulateVivoxIntegration::~ModulateVivoxIntegration() {
  VivoxBase* vivox_base = (VivoxBase*)vivox_base_ptr;
  delete vivox_base;
  modulate_voice_skin_helper_destroy(&voice_skin_helper);
  delete[] conversion_buffer;
  delete[] float_buffer;
  delete[] input_times;
  delete[] output_times;
}

void ModulateVivoxIntegration::vivox_config_setup() {
  VivoxBase* vivox_base = (VivoxBase*)vivox_base_ptr;
  vx_sdk_config_t config = vivox_base->config_begin_setup(MODULATE_VIVOX_ISSUER, MODULATE_VIVOX_SECRET_KEY);
  // Add in Modulate audio processing
  // config.pf_on_audio_unit_after_capture_audio_read = ModulateVivoxIntegration::modulate_convert_after_audio_capture;
  config.pf_on_audio_unit_before_capture_audio_sent = ModulateVivoxIntegration::modulate_convert_before_audio_sent;
  config.pf_on_audio_unit_before_recv_audio_rendered = ModulateVivoxIntegration::modulate_before_audio_rendered;
  vivox_base->config_finish_setup(config);
}

void ModulateVivoxIntegration::start_realtime_echo() {
  realtime_echo_running.store(true);
}

void ModulateVivoxIntegration::end_realtime_echo() {
  realtime_echo_running.store(false);
}

void ModulateVivoxIntegration::modulate_convert_before_audio_sent(void *callback_handle, const char *session_group_handle, const char *initial_target_uri, short *pcm_frames, int pcm_frame_count, int audio_frame_rate, int channels_per_frame, int speaking) {
  VivoxBase* base = reinterpret_cast<VivoxBase*>(callback_handle);
  ModulateVivoxIntegration* app = reinterpret_cast<ModulateVivoxIntegration*>(base->modulate_integration_ptr);
  app->convert(pcm_frames, pcm_frame_count, audio_frame_rate, channels_per_frame, speaking);
  for(size_t i = 0; i < pcm_frame_count; i++) {
    size_t buffer_ptr = (app->conversion_write_ptr + i) % MODULATE_CONVERSION_BUFFER_SIZE;
    // Record only the first channel in the conversion buffer
    app->conversion_buffer[buffer_ptr] = pcm_frames[i * channels_per_frame];
  }
  app->conversion_write_ptr += pcm_frame_count;
}

void ModulateVivoxIntegration::modulate_before_audio_rendered(void *callback_handle, const char *session_group_handle, const char *initial_target_uri, short *pcm_frames, int pcm_frame_count, int audio_frame_rate, int channels_per_frame, int is_silence) {
  VivoxBase* base = reinterpret_cast<VivoxBase*>(callback_handle);
  ModulateVivoxIntegration* app = reinterpret_cast<ModulateVivoxIntegration*>(base->modulate_integration_ptr);
  if(app->realtime_echo_running.load()) {
    size_t i = 0;
    for(; i < pcm_frame_count; i++) {
      size_t buffer_ptr = app->conversion_read_ptr + i;
      if(buffer_ptr > app->conversion_write_ptr) {
        return;
      }
      buffer_ptr = buffer_ptr % MODULATE_CONVERSION_BUFFER_SIZE;
      for(size_t j = 0; j < channels_per_frame; j++)
        pcm_frames[i*channels_per_frame + j] += app->conversion_buffer[buffer_ptr];
    }
    app->conversion_read_ptr += i;
  } else {
    // Don't let the read pointer fall behind the write pointer
    app->conversion_read_ptr = app->conversion_write_ptr;
  }
}

void ModulateVivoxIntegration::convert(short *pcm_frames,
                                       int pcm_frame_count,
                                       int audio_frame_rate,
                                       int channels_per_frame,
                                       int speaking) {
  // If we're not yet authenticated, return silence
  int is_authenticated;
  modulate_voice_skin_check_authenticated(voice_skin, &is_authenticated);
  if(!is_authenticated) {
    memset(pcm_frames, 0, sizeof(short)*pcm_frame_count*channels_per_frame);
    return;
  }

  // Get only the first channel of audio
  for(size_t i = 0; i < pcm_frame_count; i++)
    float_buffer[i] = float(pcm_frames[i*channels_per_frame]) / (1<<15);

  input_wav_logger.set_sample_rate_nonblocking(audio_frame_rate);
  input_wav_logger.add_audio_nonblocking(float_buffer, pcm_frame_count);

  int error_code = 0;
  // Convert from the input voice to a new voice
  error_code = modulate_voice_skin_helper_generate(voice_skin,
                                                   voice_skin_helper,
                                                   float_buffer,
                                                   float_buffer,
                                                   pcm_frame_count,
                                                   audio_frame_rate,
                                                   &params);
  if(error_code) {
    std::cerr<<"Modulate voice skin helper generate non-zero error code "<<error_code<<std::endl;
    return;
  }

  output_wav_logger.set_sample_rate_nonblocking(audio_frame_rate);
  output_wav_logger.add_audio_nonblocking(float_buffer, pcm_frame_count);

  // Populate all channels with result
  for(size_t i = 0; i < pcm_frame_count; i++) {
    pcm_frames[i*channels_per_frame] = (short)(float_buffer[i] * ((1<<15) - 1));
    for(size_t channel = 1; channel < channels_per_frame; channel++)
      pcm_frames[i*channels_per_frame + channel] = pcm_frames[i*channels_per_frame];
  }
}

double ModulateVivoxIntegration::get_average_performance_ratio() {
  double num = 0;
  for(size_t i = 0; i < LOGSIZE; i++)
    num += input_times[i];
  double den = 0;
  for(size_t i = 0; i < LOGSIZE; i++)
    den += output_times[i];
  return num/den;
}

void ModulateVivoxIntegration::connect() {
  VivoxBase* vivox_base = (VivoxBase*)vivox_base_ptr;
  vivox_base->Lock();
  vivox_base->connect();
  vivox_base->Unlock();
}
void ModulateVivoxIntegration::login() {
  VivoxBase* vivox_base = (VivoxBase*)vivox_base_ptr;
  vivox_base->Lock();
  vivox_base->login();
  vivox_base->Unlock();
}
void ModulateVivoxIntegration::add_session(const char* channel_name, bool is_echo) {
  VivoxBase* vivox_base = (VivoxBase*)vivox_base_ptr;
  vivox_base->Lock();
  vivox_base->add_session(channel_name, is_echo);
  vivox_base->Unlock();
}
void ModulateVivoxIntegration::remove_session(const char* channel_name, bool is_echo) {
  VivoxBase* vivox_base = (VivoxBase*)vivox_base_ptr;
  vivox_base->Lock();
  vivox_base->remove_session(channel_name, is_echo);
  vivox_base->Unlock();

}
bool ModulateVivoxIntegration::check_connected() {
  VivoxBase* vivox_base = (VivoxBase*)vivox_base_ptr;
  vivox_base->Lock();
  bool ret = vivox_base->check_connected();
  vivox_base->Unlock();
  return ret;
}
bool ModulateVivoxIntegration::check_logged_in() {
  VivoxBase* vivox_base = (VivoxBase*)vivox_base_ptr;
  vivox_base->Lock();
  bool ret = vivox_base->check_logged_in();
  vivox_base->Unlock();
  return ret;
}

void ModulateVivoxIntegration::start() {
  VivoxBase* vivox_base = (VivoxBase*)vivox_base_ptr;
  vivox_base->Start();
}

void ModulateVivoxIntegration::stop() {
  VivoxBase* vivox_base = (VivoxBase*)vivox_base_ptr;
  vivox_base->Start();
}
