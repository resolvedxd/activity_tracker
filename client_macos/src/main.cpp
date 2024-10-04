#include <ApplicationServices/ApplicationServices.h>
#include <CoreGraphics/CoreGraphics.h>
#include <curl/curl.h>
#include <math.h>

#include <ctime>
#include <iostream>
#include <string>
#include <thread>

constexpr int POST_INTERVAL = 5;
constexpr uint32_t DISPLAY_DPI[][2] = {{4251086178, 127}};
constexpr char* SERVER_URL = (char*)"http://localhost:3030/add_activity";

CGPoint previous_mouse_pos = {0, 0};
float distance_moved = 0;
int keys_pressed = 0;
int left_clicks = 0;
int right_clicks = 0;

CGEventFlags oldflags = 0;
time_t last_post = std::time(0);

CURL* curl;

struct vec2_t {
  double x, y;
};
struct display_t {
  vec2_t origin;
  vec2_t size;
  uint32_t display_serial;

  static bool in_bounds(display_t display, CGPoint mouse_pos) {
    if (display.origin.x < mouse_pos.x && display.origin.x + display.size.x > mouse_pos.x &&
        display.origin.y < mouse_pos.y && display.origin.y + display.size.y > mouse_pos.y)
      return true;
    return false;
  }
};
std::vector<display_t> displays;

bool update_displays() {
  displays.clear();
  uint32_t display_count;
  CGGetActiveDisplayList(0, nullptr, &display_count);
  CGDirectDisplayID display_ids[display_count];
  CGGetActiveDisplayList(display_count, display_ids, &display_count);

  for (uint32_t i = 0; i < display_count; ++i) {
    CGDirectDisplayID display = display_ids[i];
    auto display_bounds = CGDisplayBounds(display);
    auto display_serial = CGDisplaySerialNumber(display);

    displays.push_back({{display_bounds.origin.x, display_bounds.origin.y},
                        {display_bounds.size.width, display_bounds.size.height},
                        display_serial});
  }

  for (auto display : displays) {
    std::cout << "display " << std::to_string(display.display_serial) << " origin:" << display.origin.x << ","
              << display.origin.y << " size:" << display.size.x << "," << display.size.y << std::endl;
  }
  return true;
};

CGEventRef event_tap_cb(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void* refcon) {
  switch (type) {
    case kCGEventLeftMouseUp:
      left_clicks++;
      break;
    case kCGEventRightMouseUp:
      right_clicks++;
      break;
    case kCGEventMouseMoved: {
      CGPoint current_mouse_pos = CGEventGetLocation(event);
      if (previous_mouse_pos.x == 0 && previous_mouse_pos.y == 0) {
        previous_mouse_pos = current_mouse_pos;
      } else {
        float delta_x = current_mouse_pos.x - previous_mouse_pos.x;
        float delta_y = current_mouse_pos.y - previous_mouse_pos.y;
        int dpi = 96;
        if (displays.size() > 0) {
          for (display_t display : displays) {
            if (display_t::in_bounds(display, current_mouse_pos)) {
              for (auto dpi_ : DISPLAY_DPI) {
                if (dpi_[0] == display.display_serial) {
                  dpi = dpi_[1];
                  break;
                }
              }
            };
          }
        }
        distance_moved += (float(abs(delta_x) + abs(delta_y)) / float(dpi));
        previous_mouse_pos = current_mouse_pos;
      }
      break;
    }

    case kCGEventKeyUp:
      keys_pressed++;
      break;

    case kCGEventFlagsChanged: {
      if (oldflags == 0) {
        oldflags = CGEventGetFlags(event);
      } else {
        CGEventFlags flags = CGEventGetFlags(event);
#define was_flag_pressed(fl) (flags & fl && !(oldflags & fl))
        if (was_flag_pressed(kCGEventFlagMaskShift))
          keys_pressed++;
        else if (was_flag_pressed(kCGEventFlagMaskCommand))
          keys_pressed++;
        else if (was_flag_pressed(kCGEventFlagMaskControl))
          keys_pressed++;
        else if (was_flag_pressed(kCGEventFlagMaskAlternate))
          keys_pressed++;
#undef was_flag_pressed
      }
    }

    default:
      break;
  }

  // Return the event
  return event;
}

struct json_field {
  std::string name;
  std::string value;
};

std::string json_serialize(json_field* fields, int fields_len) {
  std::string output = "{";
  for (int i = 0; i < fields_len; i++) {
    json_field field = fields[i];
    output += "\"" + field.name + "\":\"" + field.value + "\"";
    if (i < fields_len - 1) output += ",";
  }
  output += "}";
  return output;
}

void run_timer() {
  using namespace std::chrono_literals;

  while (true) {
    time_t t = std::time(0);
    if (t % POST_INTERVAL == 0) {
      update_displays();

      if (curl) {
        CURLcode res;
        curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "token: asd");
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_URL, SERVER_URL);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POST, 1);

        json_field fields[] = {{"time", std::to_string(std::time(0))}, {"int", std::to_string(POST_INTERVAL)},
                               {"kp", std::to_string(keys_pressed)},   {"lc", std::to_string(left_clicks)},
                               {"rc", std::to_string(right_clicks)},   {"mm", std::to_string(distance_moved * 2.54)}};
        std::string data = json_serialize(fields, sizeof(fields) / sizeof(json_field));
        std::cout << data << "\n";

        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.data());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, data.size());
        curl_easy_perform(curl);
        curl_slist_free_all(headers);

        std::cout << "response:" << res << std::endl;

        distance_moved = left_clicks = right_clicks = keys_pressed = 0;
        last_post = std::time(0);
      } else {
        std::cerr << "curl=nullptr";
      }
    }
    std::this_thread::sleep_for(1s);
  }
}

int main() {
  curl = curl_easy_init();

  // Create an event tap for left mouse button events
  CGEventMask event_mask = CGEventMaskBit(kCGEventRightMouseUp) | CGEventMaskBit(kCGEventLeftMouseUp) |
                           CGEventMaskBit(kCGEventMouseMoved) | CGEventMaskBit(kCGEventKeyUp) |
                           CGEventMaskBit(kCGEventFlagsChanged);

  CFMachPortRef event_tap = CGEventTapCreate(kCGHIDEventTap, kCGHeadInsertEventTap, kCGEventTapOptionListenOnly,
                                             event_mask, event_tap_cb, nullptr);

  if (!event_tap) {
    std::cerr << "Error creating event tap" << std::endl;
    return 1;
  }

  update_displays();
  std::thread timer_thr(run_timer);

  // Add the event tap to the current run loop
  CFRunLoopSourceRef run_loop_source = CFMachPortCreateRunLoopSource(nullptr, event_tap, 0);
  CFRunLoopAddSource(CFRunLoopGetCurrent(), run_loop_source, kCFRunLoopCommonModes);
  CFRelease(run_loop_source);

  // Enable the event tap
  CGEventTapEnable(event_tap, true);

  // Run the run loop
  CFRunLoopRun();

  // Clean up
  CFRelease(event_tap);
  return 0;
}
