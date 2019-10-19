"""
inform.py

Python's mirror of C's inform.c -- contains various functions that perform
informative duties. Examining places/things, displaying proper names, etc...
"""
import utils, char, hooks, mudsock, mud, string, display



################################################################################
# utility functions
################################################################################
def build_who(ch = None):
    '''returns a formatted list of all the people currently online'''
    buf = [ ]
    buf.append(display.seperator)

    # build character info
    count   = len(mudsock.socket_list())
    playing = 0
    for sock in mudsock.socket_list():
        if not (sock.ch == None or sock.ch.room == None):
            desc = sock.ch.name
            if ch != None:
                desc = ch.see_as(sock.ch)
            buf.append(" %-12s %-10s %53s" %
                       (desc,sock.ch.race,sock.ch.user_groups))
            playing = playing + 1

    conn_end = "s"
    if count == 1: conn_end = ""
    play_end = "s"
    if playing == 1: play_end = ""

    # build our footer
    buf.append(display.seperator)
    buf.append((" %d socket" % count)  + conn_end + " logged in." + (" %d player" % playing) + play_end + " currently playing.")
    buf.append(display.seperator)
    buf.append("")
    return "\r\n".join(buf)



################################################################################
# look append functions
################################################################################
def show_equipment(ch, tgt):
    '''shows ch tgt\'s equipment'''
    for obj in tgt.eq:
        if ch.cansee(obj) or ch == tgt:
            ch.send("  %-30s %s" % (obj.name, tgt.get_slots(obj)))

cardinal_dirs = ["north", "south", "east", "west"]
compass_dirs  = ["north", "south", "east", "west",
                 "northwest", "northeast", "southwest", "southeast"]

def list_one_exit(ch, ex, dir):
    builder_info = ""
    if ch.isInGroup("builder"):
        builder_info = " [" + ex.dest.proto + "]"

    # display the direction we can go
    ch.send("  {n- %-10s :: %s%s" % (dir, ch.see_as(ex), builder_info))
                  
def list_room_exits(ch, room, filter_compass = False):
    # first, go through our standard exits
    if not filter_compass:
        for dir in compass_dirs:
            ex = room.exit(dir)
            if ex == None:
                continue
            if ex.dest == None:
                mud.log_string("ERROR: room %s headed %s to %s, which does not exist."%\
                               room.proto, dir, ex.destproto)
            elif ch.cansee(ex):
                list_one_exit(ch, ex, dir)

    # now do special exits
    for dir in room.exnames:
        if not dir in compass_dirs:
            ex = room.exit(dir)
            if ex.dest == None:
                mud.log_string("ERROR: room %s headed %s to %s, which does not exist." % \
                               room.proto, dir, ex.destproto)
            elif ch.cansee(ex):
                list_one_exit(ch, ex, dir)

def list_one_furniture(ch, obj):
    '''list the contents of a piece of furniture to the character.'''
    sitters = obj.chars
    am_on   = ch in sitters
    if am_on:
        sitters.remove(ch)

    # is it just us now?
    if len(sitters) == 0:
        ch.send(obj.rdesc)
    else:
        # build display info
        onat   = obj.furniture_type
        isare  = "is"
        if len(sitters) > 1:
            isare = "are"
        wth = ""
        if am_on:
            wth = " with you"
        sitter_descs = utils.build_show_list(ch, sitters,
                                             lambda x: x.name,
                                             lambda x: x.mname, ", ", True)
        ch.send("%s %s %s %s%s." % (sitter_descs, isare, onat, obj.name, wth))

def list_room_contents(ch, room):
    # find our list of visible objects and characters
    vis_objs  = utils.find_all_objs(ch, room.objs, "", None, True)
    vis_chars = utils.find_all_chars(ch, room.chars, "", None, True)

    # lists of used furnture, and characters using furniture
    furniture  = [ ]

    # find our list of used furniture
    for obj in vis_objs:
        if len(obj.chars) > 0:
            furniture.append(obj)

    # now remove our used furniture and people using it from the visible lists
    for furn in furniture:
        vis_objs.remove(furn)
        for pers in furn.chars:
            vis_chars.remove(pers)

    # show our list of visible characters
    if ch in vis_chars:
        vis_chars.remove(ch)
    if len(vis_chars) > 0:
        utils.show_list(ch, vis_chars, lambda(x): x.rdesc, lambda(x): x.mdesc)

    # show our list of used furniture
    for furn in furniture:
        list_one_furniture(ch, furn)

    # show our list of visible objects
    if len(vis_objs) > 0:
        utils.show_list(ch, vis_objs, lambda(x): x.rdesc, lambda(x): x.mdesc)



################################################################################
# hooks
################################################################################
def equipment_look_hook(info):
    '''displays a character\'s equipment when looked at'''
    tgt, ch = hooks.parse_info(info)

    if ch != tgt:
        gndr = tgt.heshe
        act  = "is"
    else:
        gndr = "You"
        act  = "are"

    if len(tgt.eq) > 0:
        ch.send("\n" + gndr + " " + act + " wearing:")
        show_equipment(ch, tgt)

def room_look_hook(info):
    '''diplays info about the room contents'''
    room, ch = hooks.parse_info(info)
    list_room_exits(ch, room)
    list_room_contents(ch, room)

def exit_look_hook(info):
    ex, ch = hooks.parse_info(info)
    if not ex.is_closed and ch.cansee(ex) and ex.dest != None:
        list_room_contents(ch, ex.dest)



################################################################################
# add our hooks
################################################################################
hooks.add("look_at_char", equipment_look_hook)
hooks.add("look_at_room", room_look_hook)
hooks.add("look_at_exit", exit_look_hook)



################################################################################
# unload procedure
################################################################################
def __unload__():
    '''things that need to be detached when the module is un/reloaded'''
    hooks.remove("look_at_char", equipment_look_hook)
    hooks.remove("look_at_room", room_look_hook)
    hooks.remove("look_at_exit", exit_look_hook)
