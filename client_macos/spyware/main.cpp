#include <iostream>
#include <math.h>
#include <string>
#include <ApplicationServices/ApplicationServices.h>
#include <cpr/cpr.h>
#include <thread>
#include <ctime>

constexpr int post_interval = 60;

CGEventFlags oldflags = 0;

CGPoint previousMousePosition = {0, 0};
float distance_moved = 0;
int keys_pressed = 0;
int left_clicks = 0;
int right_clicks = 0;

time_t last_post = std::time(0);

CGEventRef event_tap_cb(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *refcon) {
    if (type == kCGEventLeftMouseUp)
        left_clicks++;
    else if (type == kCGEventRightMouseUp)
        right_clicks++;
    else if (type == kCGEventMouseMoved) {
        CGPoint currentMousePosition = CGEventGetLocation(event);
        if (previousMousePosition.x == 0 && previousMousePosition.y == 0) {
            previousMousePosition = currentMousePosition;
        } else {
            float delta_x = currentMousePosition.x - previousMousePosition.x;
            float delta_y = currentMousePosition.y - previousMousePosition.y;
            int dpi = 96;
            if (currentMousePosition.y >= 0) {
                dpi = 109;
            } else {
                dpi = 86;
            }
            distance_moved += (float(abs(delta_x) + abs(delta_y)) / float(dpi));
            previousMousePosition = currentMousePosition;
        }
    }
    else if (type == kCGEventKeyUp) {
        keys_pressed++;
    } else if (type == kCGEventFlagsChanged) {
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
    
    // Return the event
    return event;
}

void run_timer() {
    using namespace std::chrono_literals;

    while(true) {
        time_t t = std::time(0);
        if (t % post_interval == 0) {
            cpr::Response r = cpr::Post(cpr::Url("http://localhost:3030/add_activity?test=asd"),
                                        cpr::Header{{ "token", "asd" }},
                                        cpr::Multipart{
                { "time", std::to_string(std::time(0)) },
                { "int", std::to_string(post_interval) },
                { "kp", std::to_string(keys_pressed) },
                { "lc", std::to_string(left_clicks) },
                { "rc", std::to_string(right_clicks) },
                { "mm", std::to_string(distance_moved * 2.54) },
                { "pl", "0" }});
            std::cout << "response:" <<r.text << std::endl;
            
            distance_moved = left_clicks = right_clicks = keys_pressed = 0;
            last_post = std::time(0);
        }
        std::this_thread::sleep_for(1s);
    }
}

int main() {
    // Create an event tap for left mouse button events
    CGEventMask eventMask = CGEventMaskBit(kCGEventRightMouseUp) |
                            CGEventMaskBit(kCGEventLeftMouseUp) |
                            CGEventMaskBit(kCGEventMouseMoved) |
                            CGEventMaskBit(kCGEventKeyUp) |
                            CGEventMaskBit(kCGEventFlagsChanged);

    CFMachPortRef eventTap = CGEventTapCreate(kCGHIDEventTap,
                                              kCGHeadInsertEventTap,
                                              kCGEventTapOptionListenOnly,
                                              eventMask,
                                              event_tap_cb,
                                              nullptr);

    if (!eventTap) {
        std::cerr << "Error creating event tap" << std::endl;
        return 1;
    }

    // Add the event tap to the current run loop
    CFRunLoopSourceRef runLoopSource = CFMachPortCreateRunLoopSource(nullptr, eventTap, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), runLoopSource, kCFRunLoopCommonModes);
    CFRelease(runLoopSource);

    // Enable the event tap
    CGEventTapEnable(eventTap, true);

    std::thread timer_thr(run_timer);
    
    // Run the run loop
    CFRunLoopRun();

    // Clean up
    CFRelease(eventTap);
    return 0;
}
