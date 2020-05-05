#ifndef __MODULATE_H__
#define __MODULATE_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
  This file is Confidential and Proprietary to Modulate, Inc.
  This file may not be shared or distributed without permission from Modulate.
  Email <> with requests, bug reports, or questions!
  Last updated May 4, 2020.
*/

// The version of the modulate library - this updates anytime
// Modulate ships a new libmodulate.a with bugfixes, performance improvements,
// API changes, etc.
#define MODULATE_VERSION 20200504

// The voice skin neural network version.  This updates when there is an
// architectural change to the voice skin models that Modulate builds, which
// are not backwards compatible with older versions of the library
// A voice skin file with a newer version will not be compatible
// with an older library
#define MODULATE_VOICE_SKIN_VERSION 20191030

// Modulate voice skins convert streams of 24kHz audio samples (in floating point format,
// with sample values in [-1, 1]) to output audio streams in the same format.
// If the input contains speech, the output will contain that speech
// in the target voice (with some latency in the audio stream - currently at
// around 100ms, but future models will have <10ms latency).

// All Modulate functions internally catch exceptions, print the exception
// contents to std::cerr, and then return error codes.
// An error code of 0 means that no exceptions were thrown
// Currently, the only non-zero error code is "1" which means that there
// was an exception.  In the future these might become more detailed
// but for now, check std::cerr for details

// modulate_voice_skin_create allocates a voice skin and populates
// its model weights from the voice skin file given by filename.
// A pointer to the voice skin is placed in voice_skin_ptr.
// The max_frame_size argument is the size, in samples, of
// the largest 24kHz audio frame that you will ask the voice skin
// to convert.
// You must call the reset() function before a newly created object is used
// or else the first several output frames will contain noise as the
// initialized state is cleared
// Example usage:
//   void* voice_skin = 0;
//   unsigned int max_frame_size = 240; // 10ms
//   int error_code = modulate_voice_skin_create(max_frame_size, "voice_skin.mod", &voice_skin);
//   if(error_code)
//     exit(1);  // Couldn't create the voice skin - abort!
//   error_code = modulate_voice_skin_reset(voice_skin);
//   if(error_code)
//     exit(1);  // Couldn't reset the voice skin - abort!
int modulate_voice_skin_create(unsigned int max_frame_size,
                               const char* filename,
                               void** voice_skin_ptr);

// deallocates the internal memory for the voice skin,
// and sets *voice_skin_ptr = 0;
int modulate_voice_skin_destroy(void** voice_skin_ptr);


// Parameters for customizing Modulate's voice outputs.  These values
// affect the strengths of the described effects on the sound of the
// output voice, which allow for additional on-the-fly customization
// of a voice skin by the end user.
// For each parameter, a 0 value applies no effect
// (and in fact skips that code altogether), while a 1 value is the full filter
// * radio_strength - the Radio filter is a band-limiting filter which
//   mimics a radio broadcast
// * presence_strength - the Presence filter commands attention by way of
//   doubling the voice via compressor and convolution.
// * bass_boost_strength - the Bass Boost filter magnifies the low frequencies
//   of a voice
// * intimidator_strength - the Intimidator filter empowers the voice with
//   chorus effect capabilities, allowing the speaker to strike terror
//   into the hearts of friends and foes, equally.
// * helm_strength - the Helm filter mimics the effect of wearing a
//   medieval bascinet of light steel, visor down.
// * vivid_strength - the Vivid filter is a V-shaped EQ which mitigates
//   the effect of lower-tier headsets and earbuds
// * disable_postfilter - Internal Use Only
typedef struct {
  float radio_strength;
  float presence_strength;
  float bass_booster_strength;
  float intimidator_strength;
  float helm_strength;
  float vivid_strength;
  int disable_postfilter;
} modulate_parameters;
static inline modulate_parameters modulate_build_default_parameters_struct() {
  modulate_parameters params = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0};
  return params;
}

// generate takes frame_size samples from input_audio in the input speaker's
// voice and converts them to frame_size samples in the voice skin's voice.
// It places the resulting samples in output_audio.
//
// Note: we cache internal state, so it is currently safe to use the same
// array for input_audio and output_audio - however, this could change in the
// future, so if you rely on this behavior please check this comment on updates!
// IMPORTANT: This function cannot be run unless the voice skin is authenticated!
// Please see modulate_voice_skin_create_authentication_message and
// modulate_voice_skin_check_authentication_message for details
int modulate_voice_skin_generate(void* voice_skin,
                                 const float* input_audio,
                                 unsigned int frame_size,
                                 float* output_audio,
                                 const modulate_parameters* parameters);

// Call reset before running generate on a new audio stream
// This resets the internal state to its default, which is the same
// as if it had been running on all-zeros input for a long time.
// This must also be called *before generate is used for the first time*
int modulate_voice_skin_reset(void* voice_skin);

// Returns the internal max_frame_size, which is the maximum size of
// audio frames at 24kHz converted by the voice skin.
// This is the same value that was passed into modulate_voice_skin_create
int modulate_voice_skin_get_max_frame_size(void* voice_skin,
                                             unsigned int* max_frame_size);

#define MODULATE_AUTHENTICATION_MESSAGE_LENGTH 618 // length of string for (2^2048-1) in base 10
// Given an API key, create an authentication message to be validated by Modulate's
// authentication server.  The authentication message will be stored in the message
// paramter, which must have at least enough capacity for MODULATE_AUTHENTICATION_MESSAGE_LENGTH
// characters.  The message_length parameter is the size of the message buffer -
// if this is less than MODULATE_AUTHENTICATION_MESSAGE_LENGTH an error will occur.
// If you do not have an API key, please email <> to request one!
// One example usage might be:
//     char msg[MODULATE_AUTHENTICATION_MESSAGE_LENGTH];
//     int error_code = modulate_voice_skin_create_authentication_message(voice_skin, api_key, msg, sizeof(msg));
//     if(error_code)
//       exit(error_code);
int modulate_voice_skin_create_authentication_message(void* voice_skin,
                                                      const char* api_key,
                                                      char* message,
                                                      unsigned int message_length);

// This function enables the voice skin to be used, by passing in a validated
// authentication check message from Modulate's authentication server.
// This authentication check message corresponds to the most recently generated
// authentication message from modulate_voice_skin_create_authentication_message.
// The authentication procedure is stateful on a per-voice-skin basis: if you
// create an authentication message it will erase any previously created
// authentication messages, so please only create one authentication message per
// voice skin, validate it with Modulate's server, and then pass the response
// into this function.
int modulate_voice_skin_check_authentication_message(void* voice_skin,
                                                     const char* message);

// This function checks whether the voice skin has been authenticated yet,
// and puts 0 into is_authenticated if not, 1 into is_authenticated if so
int modulate_voice_skin_check_authenticated(void* voice_skin,
                                            int* is_authenticated);

#define MODULATE_SKIN_NAME_MAX_LENGTH 111 // Max skin name length is 110, plus \0
// Get the name associated with this voice skin, and put it in *name
// name should point to a buffer with space for at least MODULATE_SKIN_NAME_MAX_LENGTH chars
int modulate_voice_skin_get_skin_name(void* voice_skin, char* name);

// Get the value of MODULATE_VERSION that this library was compiled with
unsigned int modulate_get_version(void);

// Get the value of MODULATE_VOICE_SKIN_VERSION that this library was compiled with
unsigned int modulate_get_voice_skin_version(void);

// Enable the internal logger to create a text log file in log_dir
// and begin writing log messages to it
int modulate_start_text_logging_in_directory(const char* log_dir);

/*-----------High Level API-----------*/
// This high-level API is intended to take care of sample rate
// variability, by using an internal sample rate converter.
// This works reasonably intelligently to minimize latency, avoid memory allocations,
// etc., but it assumes that it can receive audio at any sample rate
// at any time.  If you're in a high-performance, realtime scenario
// where you can control both of those factors directly, it may make more sense
// to use the low-level API.

// Typical usage involves creating a voice skin helper, then resetting it
// based on the expected sample rate (this sets the internal sample rate converter
// state so that buffers can be pushed through predictably - if you don't do this,
// or if the sample rate changes, nothing will crash, but you may get some stuttering)

// Creates a voice skin helper, allocating an internal buffer for sample-rate
// converted samples to be used by the voice skin.  The max_frame_size
// parameter is the maximum frame size at 24kHz, and should be the same
// (or greater) as the max_frame_size passed into
// modulate_voice_skin_create.
// A pointer to the allocated voice skin helper will be placed in
// voice_skin_helper
int modulate_voice_skin_helper_create(void** voice_skin_helper,
                                      unsigned int max_frame_size);

// deallocates the internal memory for the voice skin helper,
// and sets *voice_skin_helper = 0;
int modulate_voice_skin_helper_destroy(void** voice_skin_helper);

// generate takes frame_size samples from input_audio in the input speaker's
// voice and converts them to frame_size samples in the voice skin's voice.
// It places the resulting samples in output_audio.
//
// Note: we cache internal state, so it is currently safe to use the same
// array for input_audio and output_audio - however, this could change in the
// future, so if you rely on this behavior please check this comment on updates!
// IMPORTANT: This function cannot be run unless the voice skin is authenticated!
// Please see modulate_voice_skin_create_authentication_message and
// modulate_voice_skin_check_authentication_message for details
int modulate_voice_skin_helper_generate(void* voice_skin,
                                        void* voice_skin_helper,
                                        const float* input_audio,
                                        float* output_audio,
                                        unsigned int num_samples,
                                        unsigned int sample_rate,
                                        const modulate_parameters* parameters);

// Call reset before running generate on a new audio stream
// This resets the internal state to its default, which is the same
// as if it had been running on all-zeros input for a long time.
// This must also be called *before generate is used for the first time*
// The internal state includes sample rate converter buffers, which
// are filled based on the expected_sample_rate parameter.
// Using a different sample rate in modulate_voice_skin_helper_generate
// won't crash anything, but may cause glitches in the audio for the
// first few frames after the sample rate change.
int modulate_voice_skin_helper_reset(void* voice_skin_helper,
                                     unsigned int expected_sample_rate);

#ifdef __cplusplus
}
#endif

#endif
