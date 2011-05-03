# AC_LANG_VAR_LINK_TRY(C)(VARIABLE)
# ---------------------------------
m4_define([AC_LANG_VAR_LINK_TRY(C)],
[AC_LANG_PROGRAM(
[
extern int $1;
int i;
],
[i = $1;])])


# AC_LANG_VAR_LINK_TRY(VARIABLE)
# ------------------------------
# Produce a source which links correctly iff the VARIABLE exists.
AC_DEFUN([AC_LANG_VAR_LINK_TRY],
[_AC_LANG_DISPATCH([$0], _AC_LANG, $@)])


# AC_LANG_VAR_LINK_TRY(C++)(VARIABLE)
# -----------------------------------
m4_copy([AC_LANG_VAR_LINK_TRY(C)], [AC_LANG_VAR_LINK_TRY(C++)])


# AC_CHECK_VAR(VARIABLE, [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND])
# ----------------------------------------------------------------
AC_DEFUN([AC_CHECK_VAR],
[AS_VAR_PUSHDEF([ac_var], [ac_cv_var_$1])dnl
AC_CACHE_CHECK([for $1], ac_var,
[AC_LINK_IFELSE([AC_LANG_VAR_LINK_TRY([$1])],
                [AS_VAR_SET(ac_var, yes)],
                [AS_VAR_SET(ac_var, no)])])
AS_IF([test AS_VAR_GET(ac_var) = yes], [$2], [$3])dnl
AS_VAR_POPDEF([ac_var])dnl
])# AC_CHECK_VAR


# AC_CHECK_VARS(VARIABLE..., [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND])
# --------------------------------------------------------------------
AC_DEFUN([AC_CHECK_VARS],
[AC_FOREACH([AC_Var], [$1],
  [AH_TEMPLATE(AS_TR_CPP(HAVE_[]AC_Func),
               [Define to 1 if you have the `]AC_Func[' global variable.])])dnl
for ac_check_var in $1
do
AC_CHECK_VAR($ac_check_var,
             [AC_DEFINE_UNQUOTED([AS_TR_CPP([HAVE_$ac_check_var])]) $2],
             [$3])dnl
done
])
