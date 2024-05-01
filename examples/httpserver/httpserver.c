#include <assert.h>
#include <host/errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <picohttpparser.h>

#include <waeio.h>
#include <wasio.h>


static const char *daysOfWeek[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
static const char *months[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

static int prepare_response(char *buffer, size_t buff_size, const char *body,
    int content_length) {
    size_t response_length = 0;
    time_t t = time(NULL);
    // DO NOT use localtime
    struct tm *tm = gmtime(&t);

    response_length +=
        snprintf(buffer + response_length, buff_size - response_length, "HTTP/1.1 200 OK");
    response_length += snprintf(buffer + response_length, buff_size - response_length, "\r\n");
    // Date: <day-name>, <day> <month> <year> <hour>:<minute>:<second> GMT
    // This is considerable faster than strftime
    response_length += snprintf(buffer + response_length, buff_size - response_length,
        "Date: %s, %02d %s %04d %02d:%02d:%02d GMT\r\n", daysOfWeek[tm->tm_wday], tm->tm_mday,
        months[tm->tm_mon], tm->tm_year + 1900, tm->tm_hour, tm->tm_min, tm->tm_sec);
    // TODO, maybe allow closing?
    // response_length += snprintf(buffer + response_length, buff_size - response_length,
    // "Connection: close\r\n");
    response_length += snprintf(buffer + response_length, buff_size - response_length,
        "Content-Length: %d\r\n", content_length);
    response_length += snprintf(buffer + response_length, buff_size - response_length,
        "Content-Type: text/plain\r\n");
    response_length += snprintf(buffer + response_length, buff_size - response_length, "\r\n");
    if (content_length > 0) {
        response_length +=
            snprintf(buffer + response_length, buff_size - response_length, "%s", body);
    }

    if (response_length < 0 || response_length >= buff_size) {
        return -1;
    }

    return response_length;
}

const char *response_body =
    "CHAPTER I. Down the Rabbit-Hole  Alice was beginning to get very tired of sitting by her "
    "sister on the bank, and of having nothing to do: once or twice she had peeped into the book "
    "her sister was reading, but it had no pictures or conversations in it, <and what is the use "
    "of a book,> thought Alice <without pictures or conversations?> So she was considering in her "
    "own mind (as well as she could, for the hot day made her feel very sleepy and stupid), "
    "whether the pleasure of making a daisy-chain would be worth the trouble of getting up and "
    "picking the daisies, when suddenly a White Rabbit with pink eyes ran close by her. There was "
    "nothing so very remarkable in that; nor did Alice think it so very much out of the way to "
    "hear the Rabbit say to itself, <Oh dear! Oh dear! I shall be late!> (when she thought it over "
    "afterwards, it occurred to her that she ought to have wondered at this, but at the time it "
    "all seemed quite natural); but when the Rabbit actually took a watch out of its "
    "waistcoat-pocket, and looked at it, and then hurried on, Alice started to her feet, for it "
    "flashed across her mind that she had never before seen a rabbit with either a "
    "waistcoat-pocket, or a watch to take out of it, and burning with curiosity, she ran across "
    "the field after it, and fortunately was just in time to see it pop down a large rabbit-hole "
    "under the hedge. In another moment down went Alice after it, never once considering how in "
    "the world she was to get out again. The rabbit-hole went straight on like a tunnel for some "
    "way, and then dipped suddenly down, so suddenly that Alice had not a moment to think about "
    "stopping herself before she found herself falling down a very deep well. Either the well was "
    "very deep, or she fell very slowly, for she had plenty of time as she went down to look about "
    "her and to wonder what was going to happen next. First, she tried to look down and make out "
    "what she was coming to, but it was too dark to see anything; then she looked at the sides of "
    "the well, and noticed that they were filled with cupboards......";

static const uint32_t buffer_size = 1 << 16;
static const uint32_t max_headers = 100;

static void* handle_request(wasio_fd_t *fd) {
  wasio_fd_t clientfd = (wasio_fd_t)(uintptr_t)(void*)fd;
  char *reqbuf = malloc(sizeof(char)*buffer_size);
  const char *method;
  size_t method_len;
  const char *path;
  size_t path_len;
  int minor_version;
  struct phr_header headers[max_headers];
  size_t num_headers;
  size_t prevbuflen = 0;

  int ans = waeio_recv(clientfd, (uint8_t*)reqbuf, buffer_size);
  if (ans < 0) {
    free(reqbuf);
    abort();
  }

  ans = phr_parse_request(reqbuf, ans, &method, &method_len, &path, &path_len,
                          &minor_version, headers, &num_headers, prevbuflen);
  if (ans > 0) {
    ans = prepare_response(reqbuf, buffer_size, response_body, strlen(response_body));
    if (ans < 0) {
      free(reqbuf);
      abort();
    }
    ans = waeio_send(clientfd, (uint8_t*)reqbuf, ans);
    if (ans < 0) {
      free(reqbuf);
      abort();
    }
  } else { // FAILED
    free(reqbuf);
    abort();
  }
  free(reqbuf);
  return NULL;
}

void* listener(wasio_fd_t *sockfd) {
  wasio_fd_t fd = *sockfd;
  while (true) {
    wasio_fd_t conn;
    int ans = waeio_accept(fd, &conn);
    if (ans < 0) break;
    ans = waeio_async(handle_request, conn);
    if (ans < 0) break;
  }
  return NULL;
}

int main(void) {
  return waeio_main(listener);
}
