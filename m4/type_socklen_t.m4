AC_DEFUN([TYPE_SOCKLEN_T],[
  AC_CACHE_CHECK([for socklen_t],
    [neo4j_cv_type_socklen_t],[
    AC_TRY_COMPILE([
#include <sys/types.h>
#include <sys/socket.h>
    ],[
socklen_t len = 99;
return 0;
    ],[
      neo4j_cv_type_socklen_t=yes
    ],[
      neo4j_cv_type_socklen_t=no
    ])
  ])

  if test "X$neo4j_cv_type_socklen_t" = "Xno"; then
    AC_DEFINE(socklen_t, int, [Substitute for socklen_t])
  fi
])
