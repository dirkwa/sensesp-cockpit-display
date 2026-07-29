#pragma once

#include <cstddef>
#include <cstdint>

namespace sensesp_cockpit_display {

/// Abstract mono audio-out sink, mirroring DisplayDriver / TouchDriver:
/// one interface, one per-board implementation. play_pcm() is
/// non-blocking by contract so it is safe to call from the LVGL
/// event_loop task. The same interface is the intended sink for a
/// future streamed voice feed.
class AudioDriver {
 public:
  virtual ~AudioDriver() = default;

  virtual void init() = 0;

  /// True once init() brought the codec up successfully. False if the
  /// board has no audio or init failed (play_pcm then no-ops). Lets a
  /// status endpoint report audio health without racing the boot log.
  virtual bool ready() const = 0;

  /// Sample rate the driver expects play_pcm() buffers to be in.
  /// Callers synthesise / resample to this. 16 kHz is plenty for
  /// alert tones and speech.
  virtual uint32_t sample_rate() const = 0;

  /// Enqueue `frames` signed-16-bit mono samples for playback. Copies
  /// the buffer and returns immediately; the samples play on the
  /// driver's audio task. Passing a null buffer or zero frames is a
  /// no-op. If the queue is full the buffer is dropped (a chime is
  /// disposable — never block the caller waiting for audio).
  virtual void play_pcm(const int16_t* samples, size_t frames) = 0;

  /// Output volume 0-100. Applied at the codec. Default no-op.
  virtual void set_volume(uint8_t /*pct*/) {}

  /// Mute / unmute without tearing down the codec. When muted the
  /// power amplifier is held disabled so a quiet helm stays quiet.
  /// Default no-op.
  virtual void set_enabled(bool /*on*/) {}

  // --- Streaming playback (voice / TTS) -----------------------------------
  //
  // A continuous stream, unlike play_pcm()'s disposable fixed clips: no
  // length cap, no per-chunk silence tail (that would gap the audio), and
  // write_stream() BLOCKS to apply backpressure instead of dropping. The
  // three calls bracket one audio-start / audio-chunk* / audio-stop span.
  //
  // Default no-ops so speaker-less boards still compile.

  /// Begin a stream at `rate` Hz / `bits` per sample / `channels`. The
  /// driver may reconfigure the codec to match `rate`. Returns false if
  /// the format can't be played (caller should abandon the stream).
  virtual bool begin_stream(uint32_t /*rate*/, uint8_t /*bits*/,
                            uint8_t /*channels*/) {
    return false;
  }

  /// Write `frames` signed-16-bit mono samples into the active stream.
  /// BLOCKS until the codec accepts them (this is the flow control that
  /// paces the sender). Returns frames written, 0 if no stream is active.
  virtual size_t write_stream(const int16_t* /*samples*/, size_t /*frames*/) {
    return 0;
  }

  /// End the active stream: drain the codec so the last chunk doesn't loop
  /// on the DMA ring, then leave the codec open for the next stream/clip.
  virtual void end_stream() {}

  // --- Microphone capture (voice-in) --------------------------------------
  //
  // Default: no capture. A board with a wired mic overrides these.

  /// True if this board can capture (a mic is wired to the codec ADC and
  /// init brought it up). record_pcm() no-ops on a capture-less board.
  virtual bool can_capture() const { return false; }

  /// Sample rate of record_pcm() buffers. 16 kHz matches Whisper's native
  /// rate, so a Wyoming mic stream needs no resampling.
  virtual uint32_t capture_rate() const { return 16000; }

  /// Read up to `max_frames` signed-16-bit mono samples into `out`,
  /// BLOCKING until that many are captured (or a codec error). Returns the
  /// number of frames read (0 on error / no capture). Call in a loop from a
  /// dedicated task while streaming mic audio.
  virtual size_t record_pcm(int16_t* /*out*/, size_t /*max_frames*/) {
    return 0;
  }

  /// Start / stop the ADC capture path. start_capture() must be called
  /// before record_pcm(); stop_capture() releases the ADC when voice-in
  /// ends so idle draws no capture bandwidth. Idempotent. Default no-op.
  virtual void start_capture() {}
  virtual void stop_capture() {}
};

}  // namespace sensesp_cockpit_display
