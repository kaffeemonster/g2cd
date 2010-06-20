dnl
dnl salvaged out of gcc's acinclude.m4,
dnl no copyright or license in there, but would guess GPL
dnl Jan
dnl
dnl AS_CHECK_FEATURE(description, cv, [extra switches to as], [assembler input],
dnl [command if feature available])
dnl
dnl Checks for an assembler feature.
dnl ASSEMBLER INPUT is fed to the assembler and the feature is available
dnl if assembly succeeds.  If EXTRA TESTING LOGIC is not the empty string,
dnl then it is run instead of simply setting CV to "yes" - it is responsible
dnl for doing so, if appropriate.

AC_DEFUN([AS_CHECK_FEATURE],
[AC_CACHE_CHECK([assembler for $1], [$2],
 [[$2]=no
  if test x"$my_cv_as" != x; then
    echo ifelse(m4_substr([$4],0,1),[$], "[$4]", '[$4]') > conftest.s
    if AC_TRY_COMMAND([$my_cv_as $3 -o conftest.o conftest.s >&AS_MESSAGE_LOG_FD])
    then
      [$2]=yes
    else
      echo "configure: failed program was" >&AS_MESSAGE_LOG_FD
      cat conftest.s >&AS_MESSAGE_LOG_FD
    fi
    rm -f conftest.o conftest.s
  fi])
ifelse([$5],,,[dnl
if test $[$2] = yes; then
  $5
fi])
])
