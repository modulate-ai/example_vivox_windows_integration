#include "wav_logger.hpp"

#include <filesystem>

#include <ctime>

// Wav writing code based on a post by user Duthomhas on http://www.cplusplus.com/forum/beginner/166954/
// Retrieved on October 23, 2019

// Fixed the data chunk size value, based on WAVE format documenatation at http://soundfile.sapp.org/doc/WaveFormat/
// plus prolific use of "hexdump -C logs/log0.wav" to examine the wav headers in the log files

#ifndef MODULATE_MAX_LOG_FILE_LENGTH
#define MODULATE_MAX_LOG_FILE_LENGTH (1<<24)
#endif

using namespace std;

namespace little_endian_io
{
  template <typename Word>
  std::ostream& write_word( std::ostream& outs, Word value, unsigned size = sizeof( Word ) )
  {
    for (; size; --size, value >>= 8)
      outs.put( static_cast <char> (value & 0xFF) );
    return outs;
  }
}
using namespace little_endian_io;

WavLogger::WavLogger(size_t _buffer_size, size_t _sample_rate, const string& _log_directory, const string& _basename) :
  buffer_size(_buffer_size),
  sample_rate(_sample_rate),
  log_directory(_log_directory),
  basename(_basename) {
  buffer = new float[buffer_size];
  if(!head.is_lock_free())
    throw std::runtime_error("Atomic integers are not lock-free, cannot create wav logger");
  head.store(0);
  tail.store(0);

  filesystem::create_directories(log_directory);
  current_filename = get_next_filename();
  open_file(current_filename);
}

WavLogger::~WavLogger() {
  write_outstanding_samples_to_file();
  close_file();
}

bool WavLogger::add_audio_nonblocking(const float* audio, size_t num_samples) {
  int tail_lower_bound = tail.load();
  int head_value = head.load();

  // If the buffer can't fit the new samples, just continue and the log will skip
  if((head_value + (int)num_samples) > (tail_lower_bound + (int)buffer_size))
    return false;

  for(int i = 0; i < (int)num_samples; i++) {
    int index = (head_value + i) % buffer_size;
    buffer[index] = audio[i];
  }

  head.fetch_add((int)num_samples);
  return true;
}

string WavLogger::get_next_filename() {
  string filename;

  char time_and_date[20];
  time_t rawtime;
  time(&rawtime);
  struct tm * timeinfo = localtime(&rawtime);
  strftime(time_and_date, 20, "%Y_%m_%d_%H_%M", timeinfo);

  for(size_t i = 0; i < 1000; i++) {
    filename = log_directory + "/" + string(time_and_date) + "_" + to_string(i) + "_" + basename + ".wav";
    bool file_doesnt_exist = !filesystem::exists(filename);
    if(file_doesnt_exist)
      break;
  }
  return filename;
}

void WavLogger::open_file(const string& filename) {
  f = ofstream(filename, ios::binary);

  int bits_per_sample = 16;
  int channels = 1;
  int data_block_size = channels * (bits_per_sample / 8);
  int bytes_per_sec = data_block_size * (int)sample_rate;

  // Header
  f << "RIFF----WAVE"; // ---- to be filled in with filesize-in-bytes - 8
  // Begin the fmt chunk
  f << "fmt "; // format chunk header
  write_word( f,              16, 4 );  // no extension data
  write_word( f,               1, 2 );  // PCM - integer samples
  write_word( f,        channels, 2 );  // one channel (mono file)
  write_word( f,     sample_rate, 4 );  // samples per second (Hz)
  write_word( f,   bytes_per_sec, 4 );  // (Sample Rate * BitsPerSample * Channels) / 8
  write_word( f, data_block_size, 2 );  // data block size (size of two integer samples, one for each channel, in bytes)
  write_word( f, bits_per_sample, 2 );  // number of bits per sample (use a multiple of 8)

  // Begin the data chunk
  data_chunk_pos = f.tellp();
  f << "data----";  // (chunk size to be filled in later)
}

void WavLogger::write_outstanding_samples_to_file() {
  std::lock_guard<std::mutex> lock(writer_mutex);
  int tail_value = tail.load();
  int head_lower_bound = head.load();

  int volume = (1<<15)-1;
  for(; tail_value < head_lower_bound; tail_value++) {
    int index = tail_value % buffer_size;
    write_word(f, (int)(buffer[index] * volume), 2);
  }
  tail.store(tail_value);

  // Start new log file if needed
  size_t file_length = f.tellp();
  if(file_length > MODULATE_MAX_LOG_FILE_LENGTH)
    close_file_and_open_next();

  // Avoid overflow on 32bit ints - TODO: only relevant if we record for over 6 hours...
  if(tail_value > (1<<30)) {
    int reduction_amount = ((1<<30) / (int)buffer_size) * (int)buffer_size;
    tail.fetch_sub(reduction_amount);
    head.fetch_sub(reduction_amount);
  }
}

void WavLogger::close_file_and_open_next() {
  close_file();
  current_filename = get_next_filename();
  open_file(current_filename);
}

void WavLogger::close_file() {
  // (We'll need the final file size to fix the chunk sizes above)
  size_t file_length = f.tellp();

  // Fix the data chunk header to contain the data size
  f.seekp( data_chunk_pos + 4 );
  write_word( f, file_length - (data_chunk_pos + 8), 4);

  // Fix the file header to contain the proper RIFF chunk size, which is (file size - 8) bytes
  f.seekp( 0 + 4 );
  write_word( f, file_length - 8, 4 );
  f.close();
}



ThreadedWavLogger& ThreadedWavLogger::operator=(ThreadedWavLogger&& other) {
  bool other_thread_running = other.thread_running;
  if(other_thread_running)
    other.stop_logging_thread();
  wav_logger_ptr = other.wav_logger_ptr;
  latest_sample_rate.store(other.latest_sample_rate);
  other.wav_logger_ptr = nullptr;
  if(other_thread_running)
    start_logging_thread();
  return *this;
}

ThreadedWavLogger::~ThreadedWavLogger() {
  if(thread_running)
    stop_logging_thread();
  delete wav_logger_ptr;
}

void ThreadedWavLogger::log_task() {
  const float buffer_fraction = 0.25;
  while(!should_finish_logging) {
    const int latest_sample_rate_value = latest_sample_rate.load();
    if(latest_sample_rate_value != wav_logger_ptr->sample_rate) {
      wav_logger_ptr->sample_rate = latest_sample_rate_value;
      wav_logger_ptr->close_file_and_open_next();
    }

    const float delay_seconds = ((float)wav_logger_ptr->buffer_size / (float)wav_logger_ptr->sample_rate) * buffer_fraction;
    wav_logger_ptr->write_outstanding_samples_to_file();
    std::this_thread::sleep_for(std::chrono::milliseconds((int)(delay_seconds * 1000)));
  }
  wav_logger_ptr->write_outstanding_samples_to_file();
}

void ThreadedWavLogger::start_logging_thread() {
  should_finish_logging = false;
  thread_running = true;
  logging_thread = std::thread([this]{log_task();});
}

void ThreadedWavLogger::stop_logging_thread() {
  if(thread_running) {
    should_finish_logging = true;
    logging_thread.join();
    thread_running = false;
  }
}
