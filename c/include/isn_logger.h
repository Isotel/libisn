#ifndef ISN_LOGGER_H
#define ISN_LOGGER_H

typedef int isn_logger_level_t;

/*--------------------------------------------------------------------*/
/* DEFINITIONS                                                        */
/*--------------------------------------------------------------------*/

#define ISN_LOGGER_LOG_LEVEL_FATAL  (-1)
#define ISN_LOGGER_LOG_LEVEL_ERROR  (-2)
#define ISN_LOGGER_LOG_LEVEL_WARN   (-3)
#define ISN_LOGGER_LOG_LEVEL_INFO   (-4)
#define ISN_LOGGER_LOG_LEVEL_DEBUG  (-5)
#define ISN_LOGGER_LOG_LEVEL_TRACE  (-6)

/*--------------------------------------------------------------------*/
/* Macros                                                             */
/*--------------------------------------------------------------------*/

#define LOG_FATAL(P_LOGGER, P_FMT, ...) \
if (P_LOGGER <= ISN_LOGGER_LOG_LEVEL_FATAL) { \
    fprintf(stdout, "[fatal] %s:%d: %s, " P_FMT "\n", __FILE__, __LINE__, __func__, ## __VA_ARGS__); \
}

#define LOG_ERROR(P_LOGGER, P_FMT, ...) \
if (P_LOGGER <= ISN_LOGGER_LOG_LEVEL_ERROR) { \
    fprintf(stdout, "[error] %s:%d: %s, " P_FMT "\n", __FILE__, __LINE__, __func__, ## __VA_ARGS__); \
}

#define LOG_WARN(P_LOGGER, P_FMT, ...) \
if (P_LOGGER <= ISN_LOGGER_LOG_LEVEL_WARN) { \
    fprintf(stdout, "[warn] %s:%d: %s, " P_FMT "\n", __FILE__, __LINE__, __func__, ## __VA_ARGS__); \
}

#define LOG_INFO(P_LOGGER, P_FMT, ...) \
if (P_LOGGER <= ISN_LOGGER_LOG_LEVEL_INFO) { \
    fprintf(stdout, "[info] %s:%d: %s, " P_FMT "\n", __FILE__, __LINE__, __func__, ## __VA_ARGS__); \
}

#define LOG_DEBUG(P_LOGGER, P_FMT, ...) \
if (P_LOGGER <= ISN_LOGGER_LOG_LEVEL_DEBUG) { \
    fprintf(stdout, "[debug] %s:%d: %s, " P_FMT "\n", __FILE__, __LINE__, __func__, ## __VA_ARGS__); \
}

#define LOG_TRACE(P_LOGGER, P_FMT, ...) \
if (P_LOGGER <= ISN_LOGGER_LOG_LEVEL_TRACE) { \
    fprintf(stdout, "[trace] %s:%d: %s, " P_FMT "\n", __FILE__, __LINE__, __func__, ## __VA_ARGS__); \
}

#endif
