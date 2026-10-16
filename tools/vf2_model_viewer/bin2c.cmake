# Convert a binary file into a C header with a byte array.
# Usage: cmake -DIN=<file> -DOUT=<file.h> -DNAME=<symbol> -P bin2c.cmake
file(READ "${IN}" hex HEX)
string(LENGTH "${hex}" len)
math(EXPR count "${len} / 2")
string(REGEX MATCHALL ".." bytes "${hex}")
set(arr "")
foreach(b ${bytes})
    string(APPEND arr "0x${b},")
endforeach()
file(WRITE "${OUT}"
     "/* generated from ${IN} -- do not edit */\n"
     "static const unsigned char ${NAME}[] = {${arr}};\n"
     "static const unsigned int ${NAME}_len = ${count}u;\n")
