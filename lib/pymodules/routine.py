'''
routine.py

This is a module for setting up one-time or repeatable routines for mobs. This
can include walking a path, forging a sword, singing verses of a song, or
anything else. This was primarily meant to be for path-following, but I figured
it was worth the time to generalize it out for more complex actions
'''
import mud, mudsys, auxiliary, storage, event



################################################################################
# defines
################################################################################
__dflt_routine_step_time__ = 10 # seconds between routine steps
__global_routine_checks__  = []



################################################################################
# auxiliary data
################################################################################
class RoutineAuxData:
    '''Holds character data related to character routines.
    '''
    def __init__(self, set = None):
        self.routine = None   # the routine we follow
        self.repeat  = False  # after we finish it, do we repeat?
        self.step    = None   # what step of the routine are we on
        self.checks  = None   # what checks do we perform before each step

    def copyTo(self, to):
        if isinstance(self.routine, list):
            to.routine = [x for x in self.routine]
        else:
            to.routine = None
        if isinstance(self.checks, list):
            to.checks = [x for x in self.checks]
        else:
            to.checks = None
        to.repeat = self.repeat
        to.step   = self.step

    def copy(self):
        newdata = RoutineAuxData()
        self.copyTo(newdata)
        return newdata

    def store(self):
        return storage.StorageSet()

    def read(self, set):
        return



################################################################################
# functions
################################################################################
def register_routine_check(check):
    '''adds a routine check to the global list. Must be a function taking one
       argument, which is the character doing the routine. Return should be
       True if the check succeeded (i.e., we should not do a routine)'''
    __global_routine_checks__.append(check)

def start_routine(ch):
    '''starts a character routine event in motion'''
    time = __dflt_routine_step_time__
    aux  = ch.getAuxiliary("routine_data")
    item = aux.routine[aux.step]
    if isinstance(item, tuple):
        time = item[0]
    event.start_event(ch, time, routine_event)

def set_routine(ch, routine, repeat = False, checks = None):
    '''Sets a routine to a character. Routine steps can constain commands
       (character strings), functions (one argument, ch), or tuples
       (delay, string | function). If a tuple is not supplied, the default
       step time is used'''
    aux = ch.getAuxiliary("routine_data")
    aux.checks  = None
    if checks != None:
        aux.checks  = [x for x in checks]
    aux.repeat  = repeat
    aux.routine = None
    aux.step    = None
    if routine != None:
        aux.routine = [x for x in routine]
        aux.step    = 0
        start_routine(ch)

def do_step(ch):
    '''Performs the current step increments'''
    aux  = ch.getAuxiliary("routine_data")
    step = aux.routine[aux.step]
    time = __dflt_routine_step_time__

    # increment or decrement as necessary
    aux.step += 1

    # parse out the step
    if isinstance(step, tuple):
        time = step[0]
        step = step[1]

    # if it's a string, do it as an action. Otherwise, assume it is a function
    # and call it with ch as the only argument
    if isinstance(step, str):
        ch.act(step)
    else:
        step(ch)

    # figure out if we need to repeat it
    if aux.step == len(aux.routine) and aux.repeat == True:
        aux.step = 0

    # see if we need to queue up another event
    if aux.step < len(aux.routine):
        start_routine(ch)
    else:
        set_routine(ch, None)

def try_step(ch):
    '''Checks to see if we can perform a step in the routine. Returns true or
       false if it did or not'''
    aux = ch.getAuxiliary("routine_data")

    if aux.routine == None:
        return False

    # build a list of the checks we need to perform
    checks = __global_routine_checks__
    if aux.checks != None:
        checks = checks + aux.checks

    # If we have checks, run them
    if checks != None:
        for check in checks:
            if check(ch) == True:
                # queue us back up to try again later
                start_routine(ch)
                return False

    # If we had no checks or they were all passed, do the step
    do_step(ch)
    return True



################################################################################
# events
################################################################################
def routine_event(owner, data, arg):
    '''this is the event that perpetuates NPC routines. Each NPC that has a
       routine running has one of these events tied to him or her. When the
       routine time expires, a check is made to see if the routine can go on.
       If it can, the routine step is performed and the step number is
       incremented'''
    try_step(owner)


    
################################################################################
# commands
################################################################################
def cmd_routine(ch, cmd, arg):
    '''Appends a routine onto a character. The second argument needs to be an
       evaluable list statement. Put it in parentheses to avoid being cut off
       as spaces, since parse treats it as a single word. Example:

         > routine man "[\'say hi\', (3, \'say I am a little teapot\')]" True

       this will say hi after the default delay, and I am a little teapot after
       a delay of 3. It will then loop through this process indefinitely.
       Alternatively, these commands can be replaced with function calls.
       '''
    try:
        tgt, routine, repeat = mud.parse_args(ch, True, cmd, arg,
                                              "ch.room.noself word(py_list) | bool(repeat)")
    except:
        return

    set_routine(tgt, eval(routine), repeat)
    ch.send("Routine set.")



################################################################################
# initialization
################################################################################

# auxiliary data
auxiliary.install("routine_data", RoutineAuxData, "character")

# base check. Don't do a routine if we're currently doing an action
register_routine_check(lambda ch: ch.isActing())

# commands
mudsys.add_cmd("routine", None, cmd_routine, "admin", False)

# misc initialization
# mud.set_routine = set_routine
#
# now use:    ch.set_routine(routine)
# instead of: mud.set_routine(ch, routine)
#
mudsys.add_char_method("set_routine", set_routine)
