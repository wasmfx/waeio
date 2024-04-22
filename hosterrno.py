#/usr/bin/env python3

import subprocess
import sys

def generate_strerror_sig():
    return "const char* host_strerror(int32_t);\n"

def generate_strerror(table):
    body = []
    for (code, _, desc) in table:
        body.append("  if (e == HOST_" + str(code) + ") return \"" + str(desc) + "\";\n")
    body.append("  return \"UNKNOWN ERROR\";\n")

    fn = '''
const char* host_strerror(int32_t e) {
''' + "".join(body) + "}"
    return fn

def generate_ecodes(table):
    defs = []
    for (code, num, _) in table:
        defs.append("#define HOST_" + str(code) + " " + str(num))
    return "\n".join(defs)

def generate_imports_h():
    return "#include <stdint.h>\n"

def generate_imports_c():
    return "#include <host/errno.h>\n"

def generate_errno_h():
    return "extern int32_t host_errno;\n"

def generate_errno_c():
    return "int32_t host_errno = 0;\n"

def main(argv):

    if len(argv) < 2:
        print("usage: " + argv[0] + " <h | c>")
        exit(1)

    filekind = '\0';
    if argv[1] == 'h' or argv[1] == 'c':
        filekind = argv[1]
    else:
        print("usage: " + argv[0] + "<h | c>")
        exit(1)

    # Run errno -l to get list of host error codes, numbers, and
    # descriptions.
    proc = subprocess.Popen(['errno', '-l'], stdout=subprocess.PIPE)
    output = proc.stdout.readlines()
    table = []
    for line in output:
        [code, num, *desc] = line.decode('utf-8').split()
        table.append((code, num, " ".join(desc)))

    # Generate header file.
    if filekind == 'h':
        with open("inc/host/errno.h", "w") as f:
            print('''
#ifndef WAEIO_HOST_ERRNO_H
#define WAEIO_HOST_ERRNO_H

''', file=f)
            print(generate_imports_h(), file=f)
            print(generate_errno_h(), file=f)
            print(generate_ecodes(table), file=f)
            print(generate_strerror_sig(), file=f)
            print("#endif", file=f)

    # Generate implementation file.
    if filekind == 'c':
        with open("src/host/errno.c", "w") as f:
            print(generate_imports_c(), file=f)
            print(generate_errno_c(), file=f)
            print(generate_strerror(table), file=f)

main(sys.argv)
