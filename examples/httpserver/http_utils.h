#ifndef WAEIO_EXAMPLES_HTTPSERVER_HTTP_UTILS_H
#define WAEIO_EXAMPLES_HTTPSERVER_HTTP_UTILS_H

#include <stdint.h>
#include <stdio.h>
#include <time.h>

#if defined DEBUG
#define conn_log(...) printf(__VA_ARGS__)
#else
#define conn_log(...) {}
#endif

static int make_response(uint8_t *buffer, uint32_t buflen, const char *httpcode, const uint8_t *body, uint32_t content_length) {
  static const char *daysOfWeek[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  static const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

  uint32_t response_length = 0;
  time_t t = time(NULL);
  struct tm *tm = gmtime(&t);

  response_length += snprintf((char*)buffer + response_length, buflen - response_length, "HTTP/1.1 %s", httpcode);
  response_length += snprintf((char*)buffer + response_length, buflen - response_length, "\r\n");
  // Date: <day-name>, <day> <month> <year> <hour>:<minute>:<second> GMT
  response_length += snprintf((char*)buffer + response_length, buflen - response_length,
                              "Date: %s, %02d %s %04d %02d:%02d:%02d GMT\r\n", daysOfWeek[tm->tm_wday], tm->tm_mday,
                              months[tm->tm_mon], tm->tm_year + 1900, tm->tm_hour, tm->tm_min, tm->tm_sec);
  /* response_length += snprintf((char*)buffer + response_length, buflen - response_length, */
  /*                             "Connection: close\r\n"); */
  response_length += snprintf((char*)buffer + response_length, buflen - response_length,
                              "Content-Length: %d\r\n", content_length);
  response_length += snprintf((char*)buffer + response_length, buflen - response_length,
                              "Content-Type: text/plain\r\n");
  response_length += snprintf((char*)buffer + response_length, buflen - response_length, "\r\n");
  if (content_length > 0) {
    response_length += snprintf((char*)buffer + response_length, buflen - response_length, "%s", body);
  }

  if (response_length < 0 || response_length >= buflen) {
    return -1;
  }

  return response_length;
}

static inline int response_ok(uint8_t *buffer, uint32_t buflen, const uint8_t *body, int content_length) {
  return make_response(buffer, buflen, "200 OK", body, content_length);
}

static inline int response_notfound(uint8_t *buffer, uint32_t buflen, const uint8_t *body, int content_length) {
  return make_response(buffer, buflen, "404 Not found", body, content_length);
}

static inline int response_err(uint8_t *buffer, uint32_t buflen, const uint8_t *body, int content_length) {
  return make_response(buffer, buflen, "500 Internal server error", body, content_length);
}

static inline int response_toolarge(uint8_t *buffer, uint32_t buflen, const uint8_t *body, int content_length) {
  return make_response(buffer, buflen, "413 Content too large", body, content_length);
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
    "the well, and noticed that they were filled with cupboards......\n";

#endif
