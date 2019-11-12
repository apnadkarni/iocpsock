const char *Tcl_WinErrId (unsigned int errorCode);
const char *Tcl_WinErrMsg (unsigned int errorCode, va_list *extra);
const char *Tcl_WinError (Tcl_Interp *interp, unsigned int errorCode, va_list *extra);
