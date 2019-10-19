'''
history.py

a little database module for storing communication histories. Can store by
arbitrary groupings e.g., for guild, global, zone, or personal communications.
'''
import mudsys



################################################################################
# local variables
################################################################################

# our table that maps communication type to a second table of groupings and
# grouping functions
comm_table = { }

# what is the maximum length of our history logs
MAX_HISTORY_LEN = 20



################################################################################
# history functions
################################################################################
def register_comm_history(type, group_func):
    '''register a new type of history, and add a grouping function as well.'''
    if not type in comm_table:
        comm_table[type] = (group_func, { })

def get_history(ch, type):
    '''return the communication history for a character.'''
    group_func, table = comm_table[type]
    key = group_func(ch)
    if not key in table:
        return [ ]
    return table[key]

def add_history(ch, type, mssg):
    group_func, table = comm_table[type]
    key = group_func(ch)
    if key != None:
        if not key in table:
            table[key] = [ ]
        table[key].append(mssg)

        # make sure we don't get too big
        while len(table[key]) > MAX_HISTORY_LEN:
            table[key].pop(0)



################################################################################
# commands
################################################################################
def cmd_history(ch, cmd, arg):
    '''Communication logs are stored as you receive communication. To review
       communication you have used, you can use the history command.'''
    arg = arg.lower()
    if arg == "":
        opts = comm_table.keys()
        opts.sort()
        ch.send("History logs available to you are:")
        ch.send("  " + ", ".join(opts))
    elif not arg in comm_table:
        ch.send("There is no history log for that type of communication.")
    else:
        group_func, table = comm_table[arg]
        key = group_func(ch)

        if not key in table:
            ch.send("Your history is empty.")
        else:
            ch.page("\r\n".join(table[key]) + "\r\n")



################################################################################
# initialization
################################################################################
mudsys.add_cmd("history", None, cmd_history, "player", False)
