################################################################################
#
# __restricted_builtin_funcs__.py
#
# This contains functions used by __restricted_builtin__ to do certain
# potentially dangerous actions in a safe mode
#
################################################################################
import __builtin__

def r_import(name, globals = {}, locals = {}, fromlist = []):
    '''Restricted __import__ only allows importing of specific modules'''

    ok_modules = ("mud", "obj", "char", "room", "exit", "account", "mudsock",
                  "event", "action", "random", "traceback", "utils",
                  "__restricted_builtin__")
    if name not in ok_modules:
        raise ImportError, "Untrusted module, %s" % name
    return __builtin__.__import__(name, globals, locals, fromlist)

def r_open(file, mode = "r", buf = -1):
    if mode not in ('r', 'rb'):
        raise IOError, "can't open files for writing in restricted mode"
    return open(file, mode, buf)

def r_exec(code):
    """exec is disabled in restricted mode"""
    raise NotImplementedError,"execution of code is disabled"

def r_eval(code):
    """eval is disabled in restricted mode"""
    raise NotImplementedError,"evaluating code is disabled"

def r_execfile(file):
    """executing files is disabled in restricted mode"""
    raise NotImplementedError,"executing files is disabled"

def r_reload(module):
    """reloading modules is disabled in restricted mode"""
    raise NotImplementedError, "reloading modules is disabled"

def r_unload(module):
    """unloading modules is disabled in restricted mode"""
    raise NotImplementedError, "unloading modules is disabled"
