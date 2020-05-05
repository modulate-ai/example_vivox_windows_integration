#ifndef MODULATE_WAV_LOGGER_HPP
#define MODULATE_WAV_LOGGER_HPP

#include <string>
#include <atomic>
#include <fstream>

#include <mutex>
#include <thread>


class WavLogger {
private:
  std::ofstream f;
  size_t data_chunk_pos;
  std::string current_filename;

  std::mutex writer_mutex;
  float* buffer;
  std::atomic<int> head;
  std::atomic<int> tail;

  std::string get_next_filename();
  void open_file(const std::string& filename);
  void close_file();

public:
  const size_t buffer_size;
  size_t sample_rate;
  const std::string log_directory;
  const std::string basename;

  WavLogger(size_t buffer_size, size_t sample_rate,
            const std::string& log_directory, const std::string& basename);
  ~WavLogger();

  // add_audio_nonblocking is not safe to use on multiple threads
  // use only on the audio thread
  bool add_audio_nonblocking(const float* audio, size_t num_samples);

  void write_outstanding_samples_to_file();
  void close_file_and_open_next();
};


class ThreadedWavLogger {
private:
  WavLogger* wav_logger_ptr;

  std::thread logging_thread;
  bool should_finish_logging = true;
  bool thread_running = false;

  std::atomic<int> latest_sample_rate;

  void log_task();

public:
  ThreadedWavLogger(size_t buffer_size, size_t sample_rate,
                    const std::string& log_directory, const std::string& basename) :
    wav_logger_ptr(new WavLogger(buffer_size, sample_rate, log_directory, basename)) {
    latest_sample_rate.store((int)sample_rate);
  };
  ThreadedWavLogger& operator=(const ThreadedWavLogger& other) = delete; // don't copy in order to avoid multiple logging threads
  ThreadedWavLogger(const ThreadedWavLogger& other) = delete;
  ThreadedWavLogger& operator=(ThreadedWavLogger&& other);
  ThreadedWavLogger(ThreadedWavLogger&& other) {*this = std::move(other);};
  ~ThreadedWavLogger();

  // add_audio_nonblocking is not safe to use on multiple threads
  // use only on the audio thread
  inline bool add_audio_nonblocking(const float* audio, size_t num_samples) {
    return wav_logger_ptr->add_audio_nonblocking(audio, num_samples);
  }

  inline void set_sample_rate_nonblocking(int sample_rate) {
    latest_sample_rate.store(sample_rate);
  }

  void start_logging_thread();
  void stop_logging_thread();
};

#endif
