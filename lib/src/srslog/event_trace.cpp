/**
 *
 * \section COPYRIGHT
 *
 * Copyright 2013-2021 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

#include "srsran/srslog/event_trace.h"
#include "sinks/buffered_file_sink.h"
#include "srsran/srslog/srslog.h"
#include <ctime>

#undef trace_duration_begin
#undef trace_duration_end
#undef trace_complete_event

using namespace srslog;

/// Log channel where event traces will get sent.
static log_channel* tracer = nullptr;

/// Tracer sink name.
static constexpr char sink_name[] = "srslog_trace_sink";

void srslog::event_trace_init()
{
  // Nothing to do if the user previously set a custom channel or this is not
  // the first time this function is called.
  if (tracer) {
    return;
  }

  // Default file name where event traces will get stored.
  static constexpr char default_file_name[] = "event_trace.log";

  // Create the default event trace channel.
  //:TODO: handle name reservation.
  sink* s = create_file_sink(default_file_name);
  assert(s && "Default event file sink is reserved");
  tracer = create_log_channel("event_trace_channel", *s);
  assert(tracer && "Default event trace channel is reserved");
}

void srslog::event_trace_init(log_channel& c)
{
  // Nothing to set when a channel has already been installed.
  if (!tracer) {
    tracer = &c;
  }
}

bool srslog::event_trace_init(const std::string& filename, std::size_t capacity)
{
  // Nothing to do if the user previously set a custom channel or this is not
  // the first time this function is called.
  if (tracer) {
    return false;
  }

  auto tracer_sink = std::unique_ptr<sink>(new buffered_file_sink(filename, capacity, get_default_log_formatter()));
  if (!install_custom_sink(sink_name, std::move(tracer_sink))) {
    return false;
  }

  if (sink* s = find_sink(sink_name)) {
    log_channel& c = fetch_log_channel("event_trace_channel", *s, {"TRACE", '\0', false});
    tracer         = &c;
    return true;
  }

  return false;
}

/// Fills in the input buffer with the current time.
static void format_time(char* buffer, size_t len)
{
  std::time_t t = std::time(nullptr);
  std::tm     lt{};
  ::localtime_r(&t, &lt);
  std::strftime(buffer, len, "%FT%T", &lt);
}

namespace srslog {

void trace_duration_begin(const std::string& category, const std::string& name)
{
  if (!tracer) {
    return;
  }

  char fmt_time[24];
  format_time(fmt_time, sizeof(fmt_time));
  (*tracer)("[%s] [TID:%0u] Entering \"%s\": %s", fmt_time, (unsigned)::pthread_self(), category, name);
}

void trace_duration_end(const std::string& category, const std::string& name)
{
  if (!tracer) {
    return;
  }

  char fmt_time[24];
  format_time(fmt_time, sizeof(fmt_time));
  (*tracer)("[%s] [TID:%0u] Leaving \"%s\": %s", fmt_time, (unsigned)::pthread_self(), category, name);
}

} // namespace srslog

/// Private implementation of the complete event destructor.
srslog::detail::scoped_complete_event::~scoped_complete_event()
{
  if (!tracer) {
    return;
  }

  auto end  = std::chrono::steady_clock::now();
  auto diff = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

  if (diff < threshold) {
    return;
  }

  small_str_buffer str;
  // Limit the category and name strings to a predefined length so everything fits in a small string.
  fmt::format_to(str, "{:.32} {:.16}, {}", category, name, diff.count());
  str.push_back('\0');
  (*tracer)(std::move(str));
}
