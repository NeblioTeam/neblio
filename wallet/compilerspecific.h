#ifndef COMPILERSPECIFIC_H
#define COMPILERSPECIFIC_H

#if defined(__clang__)
#define NEBLIO_DIAGNOSTIC_PUSH "clang diagnostic push"
#define NEBLIO_DIAGNOSTIC_POP "clang diagnostic pop"
#define NEBLIO_HIDE_SHADOW_WARNING "clang diagnostic ignored \"-Wshadow\""
#elif defined(__GNUC__) || defined(__GNUG__)
#define NEBLIO_DIAGNOSTIC_PUSH "GCC diagnostic push"
#define NEBLIO_DIAGNOSTIC_POP "GCC diagnostic pop"
#define NEBLIO_HIDE_SHADOW_WARNING "GCC diagnostic ignored \"-Wshadow\""
#elif defined(_MSC_VER)
#error "Unsupported compiler"
#else
#error "Unsupported compiler"
#endif

#endif // COMPILERSPECIFIC_H
