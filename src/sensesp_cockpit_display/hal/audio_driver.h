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
};

}  // namespace sensesp_cockpit_display
