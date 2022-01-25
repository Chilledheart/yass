/* a short cut to compatible with Windows SDK 7.1A */

#ifndef NTDDI_WIN8

/*
 * When compiling C and C++ code using SDK header files, the development
 * environment can specify a target platform by #define-ing the
 * pre-processor symbol WINAPI_FAMILY to one of the following values.
 * Each FAMILY value denotes an application family for which a different
 * subset of the total set of header-file-defined APIs are available.
 * Setting the WINAPI_FAMILY value will effectively hide from the
 * editing and compilation environments the existence of APIs that
 * are not applicable to the family of applications targeting a
 * specific platform.
 */

#define WINAPI_FAMILY_PC_APP               2   /* Windows Store Applications */
#define WINAPI_FAMILY_PHONE_APP            3   /* Windows Phone Applications */
#define WINAPI_FAMILY_SYSTEM               4   /* Windows Drivers and Tools */
#define WINAPI_FAMILY_SERVER               5   /* Windows Server Applications */
#define WINAPI_FAMILY_GAMES                6   /* Windows Games and Applications */
#define WINAPI_FAMILY_DESKTOP_APP          100   /* Windows Desktop Applications */
#if WINAPI_FAMILY != WINAPI_FAMILY_DESKTOP_APP &&                              \
    WINAPI_FAMILY != WINAPI_FAMILY_PC_APP &&                                   \
    WINAPI_FAMILY != WINAPI_FAMILY_PHONE_APP &&                                \
    WINAPI_FAMILY != WINAPI_FAMILY_SYSTEM &&                                   \
    WINAPI_FAMILY != WINAPI_FAMILY_GAMES &&                                    \
    WINAPI_FAMILY != WINAPI_FAMILY_SERVER
#error Unknown WINAPI_FAMILY value. Was it defined in terms of a WINAPI_PARTITION_* value?
#endif

#ifndef WINAPI_PARTITION_DESKTOP
#define WINAPI_PARTITION_DESKTOP (WINAPI_FAMILY == WINAPI_FAMILY_DESKTOP_APP)
#endif

#ifndef WINAPI_PARTITION_APP
#define WINAPI_PARTITION_APP                                                   \
  (WINAPI_FAMILY == WINAPI_FAMILY_DESKTOP_APP ||                               \
   WINAPI_FAMILY == WINAPI_FAMILY_PC_APP ||                                    \
   WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP)
#endif

#ifndef WINAPI_PARTITION_PC_APP
#define WINAPI_PARTITION_PC_APP                                                \
  (WINAPI_FAMILY == WINAPI_FAMILY_DESKTOP_APP ||                               \
   WINAPI_FAMILY == WINAPI_FAMILY_PC_APP)
#endif

#ifndef WINAPI_PARTITION_PHONE_APP
#define WINAPI_PARTITION_PHONE_APP (WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP)
#endif

#ifndef WINAPI_PARTITION_GAMES
#define WINAPI_PARTITION_GAMES                                                 \
  (WINAPI_FAMILY == WINAPI_FAMILY_GAMES ||                                     \
   WINAPI_FAMILY == WINAPI_FAMILY_DESKTOP_APP)
#endif

#ifndef WINAPI_PARTITION_SYSTEM
#define WINAPI_PARTITION_SYSTEM                                                \
  (WINAPI_FAMILY == WINAPI_FAMILY_SYSTEM ||                                    \
   WINAPI_FAMILY == WINAPI_FAMILY_SERVER)
#endif

#define WINAPI_PARTITION_PHONE  WINAPI_PARTITION_PHONE_APP

/*
 * Header files use the WINAPI_FAMILY_PARTITION macro to assign one or
 * more declarations to some group of partitions.  The macro chooses
 * whether the preprocessor will emit or omit a sequence of declarations
 * bracketed by an #if/#endif pair.  All header file references to the
 * WINAPI_PARTITION_* values should be in the form of occurrences of
 * WINAPI_FAMILY_PARTITION(...).
 *
 * For example, the following usage of WINAPI_FAMILY_PARTITION identifies
 * a sequence of declarations that are part of both the Windows Desktop
 * Partition and the Windows-Phone-Specific Store Partition:
 *
 *     #if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP | WINAPI_PARTITION_PHONE_APP)
 *     ...
 *     #endif // WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP | WINAPI_PARTITION_PHONE_APP)
 *
 * The comment on the closing #endif allow tools as well as people to find the
 * matching #ifdef properly.
 *
 * Usages of WINAPI_FAMILY_PARTITION may be combined, when the partitition definitions are
 * related.  In particular one might use declarations like
 *
 *     #if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP) && !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
 *
 * or
 *
 *     #if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP) && !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_PHONE_APP)
 *
 * Direct references to WINAPI_PARTITION_ values (eg #if !WINAPI_FAMILY_PARTITION_...)
 * should not be used.
 */
#define WINAPI_FAMILY_PARTITION(Partitions)     (Partitions)
#endif
