'''
movement.py

all of the functions concerned with movement and position change
'''
import inform, hooks, mudsys, mud



# a ranking of positions
positions     = ["unconscious", "sleeping", "sitting", "standing", "flying"]
pos_act       = ["collapse",   "sleep",    "sit",     "stand",    "fly"]

# stuff for handling movement
dir_name = ["north", "east", "south", "west", "northeast", "northwest",
                 "southwest", "southeast", "up", "down"]
dir_abbr = ["n", "e", "s", "w", "ne", "nw", "sw", "se", "u", "d"]
dir_opp  = [  2,   3,   0,   1,   6,     7,    4,    5,   9,   8]



def try_use_furniture(ch, obj, pos):
    '''attempts to resituate a person on the piece of furniture'''
    # are we already on it?
    if ch.on == obj:
        ch.send("You are already " +ch.on.furniture_type+ " " +ch.on.name+ ".")
    # make sure we have room
    elif obj.furniture_capacity <= len(obj.chars):
        ch.send("There isn't any room left.")
    else:
        # are we already on something? get up first
        if ch.on:
            mud.message(ch,None,ch.on,None,True,"to_char",
                        "You stand up from $o.")
            mud.message(ch,None,ch.on,None,True,"to_room",
                        "$n stands up from $o.")
            ch.on = None

        # send our mud.messages for sitting down
        act = pos_act[positions.index(pos)]
        mud.message(ch, None, obj, None, True,
                    "to_char", "You " + act + " " + obj.furniture_type + " $o.")
        mud.message(ch, None, obj, None, True,
                    "to_room", "$n " + act + " " + obj.furniture_type + " $o.")

        # place ourselves down on our new furniture
        ch.on  = obj
        ch.pos = pos
        return True

    # we didn't manage to get on the furniture
    return False

def try_change_pos(ch, pos):
    '''this function attempts to change the position of the person'''
    if ch.pos == pos:
        ch.send("You are already " + pos + ".")
        return False
    else:
        if ch.pos == "flying":
            mud.message(ch,None,None,None,True, "to_char", "You stop flying.")
            mud.message(ch,None,None,None,True, "to_room", "$n stops flying.")

        act = pos_act[positions.index(pos)]
        mud.message(ch, None, None, None, True, "to_char", "You " + act + ".")
        mud.message(ch, None, None, None, True, "to_room", "$n "  + act + "s.")
        ch.pos = pos
        return True

def cmd_sit(ch, cmd, arg):
    '''If standing, attempts to sit on the ground.'''
    try:
        obj, = mud.parse_args(ch, True, cmd, arg, "| [on] obj.room")
    except: return

    if obj == None:
        try_change_pos(ch, "sitting")
    elif obj.istype("furniture"):
        try_use_furniture(ch, obj, "sitting")
    else:
        ch.send("You cannot sit on " + ch.see_as(obj) + ".")

def cmd_sleep(ch, cmd, arg):
    '''If awake, attempts to lay down and sleep.'''
    try:
        obj, = mud.parse_args(ch, True, cmd, arg, "| [on] obj.room")
    except: return

    if obj == None:
        try_change_pos(ch, "sleeping")
    elif obj.istype("furniture"):
        try_use_furniture(ch, obj, "sleeping")
    else:
        ch.send("You cannot sleep on " + ch.see_as(obj) + ".")

def cmd_stand(ch, cmd, arg):
    '''If sitting, attempts to stand. If flying, attempts to land.'''
    try_change_pos(ch, "standing")

def cmd_wake(ch, cmd, arg):
    '''If sleep, attempts to wake up and sit.'''
    mud.message(ch,None,None,None,True, "to_char",
                "You stop sleeping and sit up.")
    mud.message(ch,None,None,None,True, "to_room",
                "$n stops sleeping and sits up.")
    ch.pos = "sitting"

def dir_opposite(dir):
    '''returns the opposite direction of the specified one, or None if none.'''
    try:
        return dir_name[dir_opp[dir_index(dir)]]
    except: return None

def dir_index(dir):
    '''returns the index of the direction name'''
    try:
        return dir_name.index(dir)
    except: pass
    try:
        return dir_abbr.index(dir)
    except: pass
    return -1

def try_move_mssg(ch, dir):
    '''Handles all moving of characters from one room to another, through
       commands. Attempts a move. If successful, returns the exit left through.
       Informs people of our moving'''
    ex, success = try_move(ch, dir, True)
    return ex

def try_move(ch, dir, mssg = False):
    '''Handles all moving of characters from one room to another, through
       commands. Attempts a move. If successful, returns the exit left
       through.'''
    ex      = ch.room.exit(dir)
    success = False

    exname = "it"
    if ex != None and ex.name != "":
        exname = ex.name
    
    # did we find an exit?
    if ex == None or not ch.cansee(ex):
        ch.send("Alas, there is no exit in that direction.")
    elif ex.is_closed:
        ch.send("You will have to open " + exname + " first.")
    elif ex.dest == None:
        ch.send("It doesn't look like " + exname + " leads anywhere!")
    else:
        old_room = ch.room
        dirnum   = dir_index(dir)

        # send out our leave mud.messages as needed. Is anyone in the old room?
        if mssg == True:
            if ex.leave_mssg != '':
                mud.message(ch, None, None, None, True, "to_room",ex.leave_mssg)
            elif dirnum == -1:
                mud.message(ch, None, None, None, True, "to_room", "$n leaves.")
            else:
                mud.message(ch, None, None, None, True, "to_room",
                            "$n leaves " + dir_name[dirnum] + ".")

        # run our leave hooks
        hooks.run("exit", hooks.build_info("ch rm ex", (ch, ch.room, ex)))

        # if a hook hasn't moved us, go through with going through the exit
        if ch.room == old_room:
            ch.room = ex.dest

        # stuff that happens before we 'look'
        hooks.run("pre_enter", hooks.build_info("ch rm", (ch, ch.room)))
            
        ch.act("look")

        # send out our enter mud.messages as needed
        if mssg == True:
            if ex.enter_mssg != '':
                mud.message(ch,None,None,None,True,"to_room",ex.enter_mssg)
            elif dirnum == -1:
                mud.message(ch,None,None,None,True,"to_room","$n has arrived.")
            else:
                mud.message(ch, None, None, None, True, "to_room",
                            "$n arrives from the "+dir_name[dir_opp[dirnum]]+".")

        # run our enter hooks
        hooks.run("enter", hooks.build_info("ch rm", (ch, ch.room)))
        success = True

    # return the exit we found (if we found any)
    return ex, success

def cmd_move(ch, cmd, arg):
    '''A basic movement command, relocating you to another room in the
       specified direction.'''
    # cmd_move is the basic entry to all of the movement utilities.
    # See try_move() in movement.py
    try_move_mssg(ch, cmd)



################################################################################
# mud commands
################################################################################
mudsys.add_cmd("north",     "n",  cmd_move, "player", True)
mudsys.add_cmd("west",      "w",  cmd_move, "player", True)
mudsys.add_cmd("east",      "e",  cmd_move, "player", True)
mudsys.add_cmd("south",     "s",  cmd_move, "player", True)
mudsys.add_cmd("up",        "u",  cmd_move, "player", True)
mudsys.add_cmd("down",      "d",  cmd_move, "player", True)
mudsys.add_cmd("northwest", None, cmd_move, "player", True)
mudsys.add_cmd("northeast", None, cmd_move, "player", True)
mudsys.add_cmd("southwest", None, cmd_move, "player", True)
mudsys.add_cmd("southeast", None, cmd_move, "player", True)
mudsys.add_cmd("nw",        None, cmd_move, "player", True)
mudsys.add_cmd("ne",        None, cmd_move, "player", True)
mudsys.add_cmd("sw",        None, cmd_move, "player", True)
mudsys.add_cmd("se",        None, cmd_move, "player", True)

mudsys.add_cmd("wake",      None, cmd_wake, "player", True)
mudsys.add_cmd("sleep",     None, cmd_sleep,"player", True)
mudsys.add_cmd("stand",     None, cmd_stand,"player", True)
mudsys.add_cmd("land",      None, cmd_stand,"player", True)
mudsys.add_cmd("sit",       None, cmd_sit,  "player", True)

# The mud needs to know our command for movement as well
mudsys.set_cmd_move(cmd_move)

# useful mud methods
mud.dir_opposite = dir_opposite

def chk_can_move(ch, cmd):
    if not ch.pos in ["standing", "flying"]:
        ch.send("You cannot do that while " + ch.pos + "!")
        return False

for cmd in ["north", "west", "east", "south", "up", "down", "northwest",
            "northeast", "southwest", "southeast", "nw", "ne", "sw", "se"]:
    mudsys.register_dflt_move_cmd(cmd)
mudsys.register_move_check(chk_can_move)

def chk_wake(ch, cmd):
    if not ch.pos == "sleeping":
        ch.send("You must be asleep to wake up.")
        return False
mudsys.add_cmd_check("wake", chk_wake)

def chk_sleep(ch, cmd):
    if ch.pos == "sleeping":
        ch.send("You are already sleeping!")
        return False
    elif ch.pos == "unconscious":
        ch.send("You cannot sleep while you are unconscious.")
        return False
mudsys.add_cmd_check("sleep", chk_sleep)

def chk_stand(ch, cmd):
    if ch.pos == "standing":
        ch.send("You are already standing!")
        return False
    elif ch.pos != "sitting":
        ch.send("You cannot stand while " + ch.pos + ".")
        return False
mudsys.add_cmd_check("stand", chk_stand)

def chk_land(ch, cmd):
    if ch.pos == "standing":
        ch.send("You are already on the ground!")
        return False
    elif ch.pos != "flying":
        ch.send("You cannot land if you are not flying.")
        return False
mudsys.add_cmd_check("land", chk_land)

def chk_sit(ch, cmd):
    if ch.pos == "sitting":
        ch.send("You are already sitting!")
        return False
    elif ch.pos != "standing":
        ch.send("You must be standing to sit.")
        return False
mudsys.add_cmd_check("sit", chk_sit)
