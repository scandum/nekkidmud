################################################################################
#
# __restricted_builtin__.py
#
# This module is designed to replace the __builtin__, but overwrite many of the
# functions that would allow an unscrupulous scripter to take malicious actions
#
################################################################################
from __builtin__ import *
from __restricted_builtin_funcs__ import r_import, r_open, r_execfile, r_eval, \
     r_reload, r_exec, r_unload

# override some dangerous functions with their safer versions
__import__   = r_import
execfile     = r_execfile
open         = r_open
eval         = r_eval
reload       = r_reload
