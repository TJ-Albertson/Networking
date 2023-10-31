#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>

inline bool sequence_greater_than(uint16_t s1, uint16_t s2)
{
    return ((s1 > s2) && (s1 - s2 <= 32768)) || ((s1 < s2) && (s2 - s1 > 32768));
}

inline bool sequence_less_than(uint16_t s1, uint16_t s2)
{
    return sequence_greater_than(s2, s1);
}

inline int sequence_difference(uint16_t _s1, uint16_t _s2)
{
    int s1 = _s1;
    int s2 = _s2;
    if (abs(s1 - s2) >= 32786) {
        if (s1 > s2)
            s2 += 65536;
        else
            s1 += 65536;
    }
    return s1 - s2;
}

#endif