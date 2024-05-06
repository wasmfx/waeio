// Small utility to generate platform-specific parts of host bindings.

#include <errno.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct errno_item {
  const char *name;
  uint32_t code;
};

#define ERRNO_ITEMS 134
static const struct errno_item errno_items[ERRNO_ITEMS] = {
  { .name = "EPERM", .code = EPERM},
  { .name = "ENOENT", .code = ENOENT},
  { .name = "ESRCH", .code = ESRCH},
  { .name = "EINTR", .code = EINTR},
  { .name = "EIO", .code = EIO},
  { .name = "ENXIO", .code = ENXIO},
  { .name = "E2BIG", .code = E2BIG},
  { .name = "ENOEXEC", .code = ENOEXEC},
  { .name = "EBADF", .code = EBADF},
  { .name = "ECHILD", .code = ECHILD},
  { .name = "EAGAIN", .code = EAGAIN},
  { .name = "ENOMEM", .code = ENOMEM},
  { .name = "EACCES", .code = EACCES},
  { .name = "EFAULT", .code = EFAULT},
  { .name = "ENOTBLK", .code = ENOTBLK},
  { .name = "EBUSY", .code = EBUSY},
  { .name = "EEXIST", .code = EEXIST},
  { .name = "EXDEV", .code = EXDEV},
  { .name = "ENODEV", .code = ENODEV},
  { .name = "ENOTDIR", .code = ENOTDIR},
  { .name = "EISDIR", .code = EISDIR},
  { .name = "EINVAL", .code = EINVAL},
  { .name = "ENFILE", .code = ENFILE},
  { .name = "EMFILE", .code = EMFILE},
  { .name = "ENOTTY", .code = ENOTTY},
  { .name = "ETXTBSY", .code = ETXTBSY},
  { .name = "EFBIG", .code = EFBIG},
  { .name = "ENOSPC", .code = ENOSPC},
  { .name = "ESPIPE", .code = ESPIPE},
  { .name = "EROFS", .code = EROFS},
  { .name = "EMLINK", .code = EMLINK},
  { .name = "EPIPE", .code = EPIPE},
  { .name = "EDOM", .code = EDOM},
  { .name = "ERANGE", .code = ERANGE},
  { .name = "EDEADLK", .code = EDEADLK},
  { .name = "ENAMETOOLONG", .code = ENAMETOOLONG},
  { .name = "ENOLCK", .code = ENOLCK},
  { .name = "ENOSYS", .code = ENOSYS},
  { .name = "ENOTEMPTY", .code = ENOTEMPTY},
  { .name = "ELOOP", .code = ELOOP},
  { .name = "EWOULDBLOCK", .code = EWOULDBLOCK},
  { .name = "ENOMSG", .code = ENOMSG},
  { .name = "EIDRM", .code = EIDRM},
  { .name = "ECHRNG", .code = ECHRNG},
  { .name = "EL2NSYNC", .code = EL2NSYNC},
  { .name = "EL3HLT", .code = EL3HLT},
  { .name = "EL3RST", .code = EL3RST},
  { .name = "ELNRNG", .code = ELNRNG},
  { .name = "EUNATCH", .code = EUNATCH},
  { .name = "ENOCSI", .code = ENOCSI},
  { .name = "EL2HLT", .code = EL2HLT},
  { .name = "EBADE", .code = EBADE},
  { .name = "EBADR", .code = EBADR},
  { .name = "EXFULL", .code = EXFULL},
  { .name = "ENOANO", .code = ENOANO},
  { .name = "EBADRQC", .code = EBADRQC},
  { .name = "EBADSLT", .code = EBADSLT},
  { .name = "EDEADLOCK", .code = EDEADLOCK},
  { .name = "EBFONT", .code = EBFONT},
  { .name = "ENOSTR", .code = ENOSTR},
  { .name = "ENODATA", .code = ENODATA},
  { .name = "ETIME", .code = ETIME},
  { .name = "ENOSR", .code = ENOSR},
  { .name = "ENONET", .code = ENONET},
  { .name = "ENOPKG", .code = ENOPKG},
  { .name = "EREMOTE", .code = EREMOTE},
  { .name = "ENOLINK", .code = ENOLINK},
  { .name = "EADV", .code = EADV},
  { .name = "ESRMNT", .code = ESRMNT},
  { .name = "ECOMM", .code = ECOMM},
  { .name = "EPROTO", .code = EPROTO},
  { .name = "EMULTIHOP", .code = EMULTIHOP},
  { .name = "EDOTDOT", .code = EDOTDOT},
  { .name = "EBADMSG", .code = EBADMSG},
  { .name = "EOVERFLOW", .code = EOVERFLOW},
  { .name = "ENOTUNIQ", .code = ENOTUNIQ},
  { .name = "EBADFD", .code = EBADFD},
  { .name = "EREMCHG", .code = EREMCHG},
  { .name = "ELIBACC", .code = ELIBACC},
  { .name = "ELIBBAD", .code = ELIBBAD},
  { .name = "ELIBSCN", .code = ELIBSCN},
  { .name = "ELIBMAX", .code = ELIBMAX},
  { .name = "ELIBEXEC", .code = ELIBEXEC},
  { .name = "EILSEQ", .code = EILSEQ},
  { .name = "ERESTART", .code = ERESTART},
  { .name = "ESTRPIPE", .code = ESTRPIPE},
  { .name = "EUSERS", .code = EUSERS},
  { .name = "ENOTSOCK", .code = ENOTSOCK},
  { .name = "EDESTADDRREQ", .code = EDESTADDRREQ},
  { .name = "EMSGSIZE", .code = EMSGSIZE},
  { .name = "EPROTOTYPE", .code = EPROTOTYPE},
  { .name = "ENOPROTOOPT", .code = ENOPROTOOPT},
  { .name = "EPROTONOSUPPORT", .code = EPROTONOSUPPORT},
  { .name = "ESOCKTNOSUPPORT", .code = ESOCKTNOSUPPORT},
  { .name = "EOPNOTSUPP", .code = EOPNOTSUPP},
  { .name = "EPFNOSUPPORT", .code = EPFNOSUPPORT},
  { .name = "EAFNOSUPPORT", .code = EAFNOSUPPORT},
  { .name = "EADDRINUSE", .code = EADDRINUSE},
  { .name = "EADDRNOTAVAIL", .code = EADDRNOTAVAIL},
  { .name = "ENETDOWN", .code = ENETDOWN},
  { .name = "ENETUNREACH", .code = ENETUNREACH},
  { .name = "ENETRESET", .code = ENETRESET},
  { .name = "ECONNABORTED", .code = ECONNABORTED},
  { .name = "ECONNRESET", .code = ECONNRESET},
  { .name = "ENOBUFS", .code = ENOBUFS},
  { .name = "EISCONN", .code = EISCONN},
  { .name = "ENOTCONN", .code = ENOTCONN},
  { .name = "ESHUTDOWN", .code = ESHUTDOWN},
  { .name = "ETOOMANYREFS", .code = ETOOMANYREFS},
  { .name = "ETIMEDOUT", .code = ETIMEDOUT},
  { .name = "ECONNREFUSED", .code = ECONNREFUSED},
  { .name = "EHOSTDOWN", .code = EHOSTDOWN},
  { .name = "EHOSTUNREACH", .code = EHOSTUNREACH},
  { .name = "EALREADY", .code = EALREADY},
  { .name = "EINPROGRESS", .code = EINPROGRESS},
  { .name = "ESTALE", .code = ESTALE},
  { .name = "EUCLEAN", .code = EUCLEAN},
  { .name = "ENOTNAM", .code = ENOTNAM},
  { .name = "ENAVAIL", .code = ENAVAIL},
  { .name = "EISNAM", .code = EISNAM},
  { .name = "EREMOTEIO", .code = EREMOTEIO},
  { .name = "EDQUOT", .code = EDQUOT},
  { .name = "ENOMEDIUM", .code = ENOMEDIUM},
  { .name = "EMEDIUMTYPE", .code = EMEDIUMTYPE},
  { .name = "ECANCELED", .code = ECANCELED},
  { .name = "ENOKEY", .code = ENOKEY},
  { .name = "EKEYEXPIRED", .code = EKEYEXPIRED},
  { .name = "EKEYREVOKED", .code = EKEYREVOKED},
  { .name = "EKEYREJECTED", .code = EKEYREJECTED},
  { .name = "EOWNERDEAD", .code = EOWNERDEAD},
  { .name = "ENOTRECOVERABLE", .code = ENOTRECOVERABLE},
  { .name = "ERFKILL", .code = ERFKILL},
  { .name = "EHWPOISON", .code = EHWPOISON},
  { .name = "ENOTSUP", .code = ENOTSUP}
};

void emit_hguard(FILE* fp, const char *h) {
  fprintf(fp, "#ifndef %s\n", h);
  fprintf(fp, "#define %s\n", h);
}

void emit_hend(FILE* fp) {
  fprintf(fp, "#endif\n");
}

void emit_string(FILE* fp, const char *s) {
  fprintf(fp, "%s", s);
}

void emit_stringln(FILE* fp, const char *s) {
  fprintf(fp, "%s\n", s);
}

void emit_header(FILE* fp) {
  emit_stringln(fp, "// DO NOT EDIT. This file is automatically generated by the build process!");
}

void emit_errno_h(FILE* fp, const struct errno_item items[ERRNO_ITEMS]) {
  emit_header(fp);
  emit_hguard(fp, "WAEIO_HOST_ERRNO_H");
  emit_stringln(fp, "\n#include <stdint.h>\n");
  emit_stringln(fp, "extern int32_t host_errno;\n");
  emit_stringln(fp, "const char* host_strerror(int32_t);\n");
  for (int i = 0; i < ERRNO_ITEMS; i++) {
    fprintf(fp, "#define HOST_%s %d\n", items[i].name, items[i].code);
  }
  emit_stringln(fp, "");
  emit_hend(fp);
}

void emit_errno_c(FILE *fp, const struct errno_item items[ERRNO_ITEMS]) {
  emit_header(fp);
  bool seen[ERRNO_ITEMS];
  memset(seen, false, sizeof(bool)*ERRNO_ITEMS);

  emit_stringln(fp, "#include <host/errno.h>");
  emit_stringln(fp, "#include <stdint.h>\n");
  emit_stringln(fp, "int32_t host_errno = 0;\n");
  emit_stringln(fp, "const char* host_strerror(int32_t e) {");
  for (int i = 0; i < ERRNO_ITEMS; i++) {
    if (!seen[items[i].code]) {
      seen[items[i].code] = true;
      emit_string(fp, "  if (");
      fprintf(fp, "e == HOST_%s", items[i].name);
      for (int j = 0; j < ERRNO_ITEMS; j++) {
        if (i != j && items[i].code == items[j].code) {
          fprintf(fp, " || e == HOST_%s", items[j].name);
        }
      }
      fprintf(fp, ") return \"%s\";\n", strerror(items[i].code));
    }
  }
  emit_stringln(fp, "  return \"UNKNOWN ERROR\";");
  emit_stringln(fp, "}");
}

void emit_import(FILE *fp, const char *module, const char *fn, const char *sig) {
  emit_stringln(fp, "extern");
  fprintf(fp, "__wasm_import__(\"%s\", \"%s\")\n", module, fn);
  fprintf(fp, "%s;\n", sig);
}

void emit_poll_h(FILE *fp) {
  emit_header(fp);
  emit_hguard(fp, "WAEIO_HOST_POLL_H");

  emit_stringln(fp, "\n#include <assert.h>");
  emit_stringln(fp, "#include <stddef.h>");
  emit_stringln(fp, "#include <stdint.h>");
  emit_stringln(fp, "#include <wasm_utils.h>\n");

  fprintf(fp, "#define HOST_POLLIN %d\n", POLLIN);
  fprintf(fp, "#define HOST_POLLPRI %d\n", POLLPRI);
  fprintf(fp, "#define HOST_POLLOUT %d\n", POLLOUT);
  fprintf(fp, "#define HOST_POLLERR %d\n", POLLERR);
  fprintf(fp, "#define HOST_POLLHUP %d\n", POLLHUP);
  fprintf(fp, "#define HOST_POLLNVAL %d\n\n", POLLNVAL);

  fprintf(fp, "struct pollfd {\n"
          "  int32_t fd;\n"
          "  short int events;\n"
          "  short int revents;\n"
          "} __attribute__((packed));\n"
          "static_assert(sizeof(struct pollfd) == 8, \"size of struct pollfd\");\n"
          "static_assert(offsetof(struct pollfd, fd) == 0, \"offset of fd\");\n"
          "static_assert(offsetof(struct pollfd, events) == 4, \"offset of events\");\n"
          "static_assert(offsetof(struct pollfd, revents) == 6, \"offset of revents\");\n\n");

  emit_import(fp, "host_poll", "poll", "int32_t host_poll(struct pollfd*, uint32_t, uint32_t, int32_t*)");

  emit_hend(fp);
}

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: %s < errno.h | errno.c | poll.h >\n", argv[0]);
    exit(-1);
  }

  if (strcmp(argv[1], "errno.h") == 0) emit_errno_h(stdout, errno_items);
  if (strcmp(argv[1], "errno.c") == 0) emit_errno_c(stdout, errno_items);
  if (strcmp(argv[1], "poll.h") == 0) emit_poll_h(stdout);

  return 0;
}
