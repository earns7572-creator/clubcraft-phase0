#pragma once

#include <cstdlib>
#include <iostream>

namespace clubcraft::test
{

[[noreturn]] inline void failCheck(const char* expression,
                                   const char* file,
                                   int line)
{
    std::cerr << "CHECK failed: " << expression << " at " << file << ':' << line << '\n';
    std::abort();
}

} // namespace clubcraft::test

#define CHECK(expression) \
    do \
    { \
        if (!(expression)) \
            ::clubcraft::test::failCheck(#expression, __FILE__, __LINE__); \
    } while (false)
