#include <curl/curl.h>
#include <errno.h>
#include <fcntl.h>
#include <jansson.h>
#include <libevdev/libevdev.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#define SWAY_BUFFER_SIZE 2048
#define PLATFORM_NUMBER 2
#define POST_INTERVAL 5
#define DISPLAY_DPI 102  // HARDCODED DPI BECAUSE IM TOO LAZY TO IMPLEMENT MONITOR DETECTION
const char* SERVER_URL = "http://localhost:3030/add_activity";
const char* TOKEN = "asd";
unsigned char enable_sway_ipc = 0;

CURL* curl;

#pragma pack(push, 1)
struct ipc_header {
  char magic[6];  // i3-ipc
  uint32_t size;
  uint32_t type;
};
#pragma pack(pop)

ssize_t send_sw_msg(char* buf, int fd, uint32_t type, const char* data) {
  const size_t size = strlen(data);
  memcpy(buf, "i3-ipc", 6);
  memcpy(buf + 6, &size, sizeof(uint32_t));
  memcpy(buf + 10, &type, sizeof(uint32_t));
  memcpy(buf + 14, data, size);
  return write(fd, buf, size + 14);
}
ssize_t read_sw_msg(int fd, char* buf) {
  ssize_t rd = read(fd, buf, SWAY_BUFFER_SIZE);
  uint32_t size = ((struct ipc_header*)(buf))->size + 14;
  if (size < SWAY_BUFFER_SIZE)
    buf[size] = '\0';
  return rd;
}
void parse_sw_json(const char* buf, char* foreground_wndw_id, char* foreground_wndw_title) {
    // printf("%ld %u\n", rd, size);
    json_t* root = NULL;
    json_error_t error;
    root = json_loads(buf + 14, 0, &error);
    if (root != NULL && json_is_object(root)) {
      json_t* container = json_object_get(root, "container");
      if (container != NULL && json_is_object(container)) {
        json_t* focused = json_object_get(container, "focused");
        if (json_is_true(focused)) {
          json_t* app_id = json_object_get(container, "app_id");
          if (json_is_string(app_id)) {
            const char* app_id_str = json_string_value(app_id);
            const size_t len = strlen(app_id_str);
            memcpy(foreground_wndw_id, app_id_str, len > 128 ? 128 : len);
            foreground_wndw_id[len] = '\0';
          } else {
            json_t* window_properties = json_object_get(container, "window_properties");
            if (json_is_object(window_properties)) {
              json_t* class = json_object_get(window_properties, "class");
              if (json_is_string(class)) {
                const char* class_str = json_string_value(class);
                const size_t len = strlen(class_str);
                memcpy(foreground_wndw_id, class_str, len > 128 ? 128 : len);
                foreground_wndw_id[len] = '\0';
              }
            }
          }
          json_t* name = json_object_get(container, "name");
          if (json_is_string(name)) {
            const char* title_str = json_string_value(name);
            const size_t len = strlen(title_str);
            memcpy(foreground_wndw_title, title_str, len > 128 ? 128 : len);
            foreground_wndw_title[len] = '\0';
          }
        }
      }
      json_decref(root);
    } else {
      // printf("error parsing json from swayipc\n");
    }
}
int main(int argc, char** argv) {
  curl = curl_easy_init();

  struct pollfd* fds = calloc(2, sizeof(struct pollfd));
  int fds_capacity = 2;
  int fds_len = 0;
  int opt = 0;

  while ((opt = getopt(argc, argv, ":d:s")) != -1) {
    switch (opt) {
      case 'd':
        int fd = open(optarg, O_RDONLY | O_NONBLOCK);
        if (fd == -1) {
          printf("error opening file (%d)\n", errno);
          free(fds);
          return 1;
        }
        if (fds_len >= fds_capacity) {
          fds = realloc(fds, (fds_capacity + 1) * sizeof(struct pollfd));
          if (fds == NULL) return 1;
          fds_capacity += 1;
        }
        fds[fds_len].fd = fd;
        fds[fds_len].events = POLLIN;
        fds[fds_len++].revents = 0;

        printf("device path: %s\n", optarg);
        break;
      case 's':
        enable_sway_ipc = 1;
        break;

      case ':':
        printf("option -%c requires an argument\n", optopt);
        break;
      case '?':
        printf("unknown option: -%c\n", optopt);
      default:
        break;
    }
  }

  setuid(getuid());
  setgid(getgid());

  int sw_fd = -1;
  char* sw_buf = NULL;
  if (enable_sway_ipc) {
    const char* sock_path = getenv("SWAYSOCK");
    if (sock_path == NULL) {
      printf("SWAYSOCK not set, disabling sway IPC\n");
      enable_sway_ipc = 0;
    }

    sw_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sw_fd == -1) {
      printf("error opening socket (%s)\n", sock_path);
      enable_sway_ipc = 0;
    }
    fcntl(sw_fd, F_SETFL, O_NONBLOCK);

    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, sock_path);
    if (connect(sw_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
      printf("error connecting to sway socket\n");
      enable_sway_ipc = 0;
    }

    sw_buf = malloc(SWAY_BUFFER_SIZE);
    long snt = send_sw_msg(sw_buf, sw_fd, 2, "[\"window\"]");
    long rd = read_sw_msg(sw_fd, sw_buf);
    printf("%ld %ld %u %s\n", snt, rd, ((struct ipc_header*)(sw_buf))->size, sw_buf + 14);

    // kinda dumb but eh
    // if (fds_len >= fds_capacity) {
    //   fds = realloc(fds, fds_capacity + 1 * sizeof(struct pollfd));
    //   fds_capacity += 1;
    // }
    // fds[fds_len].fd = sw_fd;
    // fds[fds_len].events = POLLIN;
    // fds[fds_len++].revents = 0;
  }

  struct libevdev** devs = malloc(fds_len * sizeof(struct libevdev*));
  for (int i = 0; i < fds_len; i++) {
    if (fds[i].fd <= 0 || fds[i].fd == sw_fd) continue;

    if (libevdev_new_from_fd(fds[i].fd, &devs[i]) != 0) {
      printf("error creating libevdev\n");
      return 1;
    };
  }

  int kb_presses = 0;
  int px_moved = 0;
  int left_clicks = 0;
  int right_clicks = 0;
  time_t last_post = time(0);

  char* foreground_wndw_id = NULL;
  char* foreground_wndw_title = NULL;
  if (enable_sway_ipc) {
    foreground_wndw_id = malloc(128);
    foreground_wndw_title = malloc(128);
  }
  while (1) {
    poll(fds, fds_len, 1000);

    for (int i = 0; i < fds_len; i++) {
      if (fds[i].fd <= 0 || fds[i].fd == sw_fd) continue;

      struct input_event ev;
      while (libevdev_next_event(devs[i], LIBEVDEV_READ_FLAG_NORMAL, &ev) == LIBEVDEV_READ_STATUS_SUCCESS) {
          // printf("%d %d %d ret:%d\n", ev.type, ev.code, ev.value, ret);
          switch (ev.type) {
            case EV_KEY:
              if (ev.code == BTN_LEFT)
                left_clicks++;
              else if (ev.code == BTN_RIGHT)
                right_clicks++;
              else if (ev.value == 0)
                kb_presses++;
              break;
            case EV_REL:
              if (ev.code == REL_X || ev.code == REL_Y) px_moved++;
              break;
          }
      }
    }

    if (enable_sway_ipc) {
      long rd = read_sw_msg(sw_fd, sw_buf);
      uint32_t size = ((struct ipc_header*)(sw_buf))->size;
      // discard message if its too big
      if (size > SWAY_BUFFER_SIZE)
        while ((rd = read_sw_msg(sw_fd, sw_buf)) != -1);
      else if (rd != -1)
        parse_sw_json(sw_buf, foreground_wndw_id, foreground_wndw_title);
    }

    if (time(0) - last_post > POST_INTERVAL) {
      printf("kb_presses:%d cm_moved:%f\n", kb_presses, px_moved / (DISPLAY_DPI / 2.54));
      last_post = time(0);
      if (curl) {
        CURLcode res = CURL_LAST;
        struct curl_slist* headers = NULL;
        char* buf = malloc(512);

        snprintf(buf, 512, "token: %s", TOKEN);
        headers = curl_slist_append(headers, buf);
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_URL, SERVER_URL);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);

        int size = snprintf(buf, 512,
                           "{\"time\":\"%ld\",\"int\":\"%d\",\"kp\":\"%d\",\"lc\":\"%d\",\"rc\":\"%d\","
                           "\"mm\": \"%f\", \"pl\": \"%d\"}",
                           time(0), POST_INTERVAL, kb_presses, left_clicks, right_clicks,
                           px_moved / (DISPLAY_DPI / 2.54), PLATFORM_NUMBER);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, buf);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)size);
        res = curl_easy_perform(curl);
        curl_slist_free_all(headers);
        free(buf);

        if (enable_sway_ipc)

        printf("focused window, id:%s title:%s\n", foreground_wndw_id, foreground_wndw_title);
        printf("response: %d\n", res);
      } else {
        printf("curl=nullptr\n");
      }
      px_moved = kb_presses = left_clicks = right_clicks = 0;
    }
  }

  for (int i = 0; i < fds_len; i++)
    if (fds[i].fd > 0) {
      close(fds[i].fd);
      libevdev_free(devs[i]);
    }
  free(fds);
  free(devs);
  free(sw_buf);
  free(foreground_wndw_id);
  free(foreground_wndw_title);
  curl_easy_cleanup(curl);
  return 0;
}
