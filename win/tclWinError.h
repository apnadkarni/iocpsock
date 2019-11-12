const char *Tcl_WinErrId (unsigned int errorCode);
const WCHAR *Tcl_WinErrMsg (unsigned int errorCode, va_list *extra);
const WCHAR *Tcl_WinError (Tcl_Interp *interp, unsigned int errorCode, va_list *extra);

