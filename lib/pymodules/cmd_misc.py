'''
cmd_misc.c

a collection of miscellaneous commands that come with NakedMud(tm)
'''
import mudsys, mud, hooks, event, mudsys



def cmd_stop(ch, cmd, arg):
    '''If you are currently performing an action (for example, a delayed
       command), make an attempt to stop performing that action.'''
    if not ch.isActing():
        ch.send("But you're not currently performing an action!\r\n")
    else:
        ch.interrupt()

def cmd_clear(ch, cmd, arg):
    '''This command will clear your display screen.'''
    ch.send_raw("\033[H\033[J")

def event_delayed_cmd(ch, filler, cmd):
    '''used to perform delayed commands'''
    ch.act(cmd, True)

def cmd_delay(ch, cmd, arg):
    '''Usage: delay <seconds> <command>

       Allows the user to prepare a command to be executed in the future. For
       example:

       > delay 2 say hello, world!

       Will make you say \'hello, world!\' two seconds after entering the
       delayed command.'''
    try:
        secs, to_delay = mud.parse_args(ch, True, cmd, arg, "double string")
    except: return

    if secs < 1:
        ch.send("You can only delay commands for positive amounts of time.")
    else:
        ch.send("You delay '%s' for %.2f seconds" % (to_delay, secs))
        event.start_event(ch, secs, event_delayed_cmd, None, to_delay)

def cmd_motd(ch, cmd, arg):
    '''This command will display to you the mud\'s message of the day.'''
    ch.page(get_motd())

def cmd_save(ch, cmd, arg):
    '''Attempt to save your character and all recent changes made to it, to
       disk. This automatically happens when logging out.'''
    if mudsys.do_save(ch):
        ch.send("Saved.")
    else:
        ch.send("Your character was not saved.")
    

def cmd_quit(ch, cmd, arg):
    '''Attempts to save and log out of the game.'''
    mud.log_string(ch.name + " has left the game.")
    mudsys.do_save(ch)
    mudsys.do_quit(ch)



################################################################################
# add our commands
################################################################################
mudsys.add_cmd("stop",  None, cmd_stop,  "player", False)
mudsys.add_cmd("clear", None, cmd_clear, "player", False)
mudsys.add_cmd("delay", None, cmd_delay, "player", False)
mudsys.add_cmd("motd",  None, cmd_motd,  "player", False)
mudsys.add_cmd("save",  None, cmd_save,  "player", False)
mudsys.add_cmd("quit",  None, cmd_quit,  "player", True)

chk_can_save = lambda ch, cmd: not ch.is_npc
mudsys.add_cmd_check("save", chk_can_save)
mudsys.add_cmd_check("quit", chk_can_save)
