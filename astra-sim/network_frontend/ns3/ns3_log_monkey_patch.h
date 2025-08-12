// not good solution, but works
#ifdef NS3_LOG_COMPAT_UNDEF_SYSLOG

#undef LOG_EMERG
#undef LOG_ALERT
#undef LOG_CRIT
#undef LOG_ERR
#undef LOG_WARNING
#undef LOG_NOTICE
#undef LOG_INFO
#undef LOG_DEBUG

#else

// I guess redefine them back is good, but just not figure out how
// #define LOG_EMERG 0
// #define LOG_ALERT 1
// #define LOG_CRIT 2
// #define LOG_ERR 3
// #define LOG_WARNING 4
// #define LOG_NOTICE 5
// #define LOG_INFO 6
// #define LOG_DEBUG 7

#endif
