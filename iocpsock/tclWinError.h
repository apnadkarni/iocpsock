CONST char *Tcl_Win32ErrId (unsigned long errorCode);
CONST char *Tcl_Win32ErrMsg TCL_VARARGS(unsigned long,errorCode);
CONST char *Tcl_Win32ErrMsgVA (unsigned long errorCode, va_list argList);
CONST char *Tcl_Win32Error TCL_VARARGS(Tcl_Interp *,interp);