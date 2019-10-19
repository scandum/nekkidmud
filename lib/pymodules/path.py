'''
path.py

Plugs into the routine module to allow for the easy construction of paths and
path following.
'''
import mud, mudsys, room



################################################################################
# functions
################################################################################
def leads_to(frm, to):
    '''returns whether from leads directly to to'''
    for ex in frm.exnames:
        if frm.exit(ex).dest == to:
            return True
    return False

def shortest_path_bfs(frm, to, ignore_doors = False, stay_zone = True,
                      ignore = None):
    '''calculates the shortest path, but uses a breadth first search. More
       efficient than depth-first seach for very short paths with lots of
       branches or very large muds.
    '''
    if frm == to:
        return [ frm ]

    depth = [ [ frm ] ]

    if ignore == None:
        ignore = set()
    ignore.add(frm)

    # figure out what zone we're doing this from
    zone = None
    if stay_zone:
        zone = frm.locale

    # keep going until we find to, or we can't go any deeper
    found = False
    while not found:
        layer = [ ]
        for room in depth[-1]:
            for dir in room.exnames:
                dest = room.exit(dir).dest
                if dest == None or dest in ignore:
                    pass
                elif zone != None and dest.locale != zone:
                    pass
                elif dest == to:
                    found = True
                    layer = [ ]
                    break
                else:
                    layer.append(dest)
                ignore.add(dest)
            if found == True:
                break
        if len(layer) > 0:
            depth.append(layer)
        if found == True or len(layer) == 0:
            break

    # no path found
    if found == False:
        return None

    # find the rooms that link each other, in reverse order
    path  = [ to ]
    order = range(len(depth))
    order.reverse()
    for i in order:
        for room in depth[i]:
            if leads_to(room, path[-1]):
                path.append(room)
                break

    path.reverse()
    return path

def shortest_path_dfs(frm, to, ignore_doors = False, stay_zone = True,
                      ignore = None):
    '''returns the steps needed to take to go from one room to another. More
       efficient than breadth-first search for very long paths with only a few
       branches, or very small muds.'''
    path = []

    if ignore == None:
        ignore = set()

    # a list of rooms we ignore for tracking. Add ourself so we don't loop back
    ignore.add(frm)

    # if we're the target room, return an empty list
    if frm == to:
        return [frm]

    zone = None
    if stay_zone:
        zone = frm.locale

    # build the shortest path 
    for ex in frm.exnames:
        # check if it has a door, and if we ignore it
        if frm.exit(ex).is_closed:
            continue

        # get the dest room. if there is none, skip this exit
        next_room = frm.exit(ex).dest
        
        if next_room == None:
            continue

        # if we already know this is a dead end or a loopback, skip it
        if (next_room in ignore or
            (stay_zone and not next_room.locale == zone)):
            continue

        next_path = shortest_path(next_room, to, ignore_doors, stay_zone,ignore)

        # dead end
        if len(next_path) == 0:
            continue

        # we found a path, append ourself
        next_path.insert(0, frm)

        # are we shorter than the previous one?
        if len(path) == 0 or len(path) > len(next_path):
            path = next_path

    return path

# set whether we are using bfs or dfs as our main pathing method
shortest_path = shortest_path_bfs

def path_to_dirs(path):
    '''takes a path of rooms and converts it to directions'''

    dirs = []
    i    = 0
    while i < len(path) - 1:
        for ex in path[i].exnames:
            if path[i].exit(ex).dest == path[i+1]:
                dirs.append(ex)
                break
        i = i + 1

    # return the directions we generated, if any
    return dirs

def build_patrol(rms, reverse = True, ignore_doors = False, stay_zone = True):
    '''builds a set of directions that need to be followed to do a patrol
       between the rooms. If reverse is true, also supplies the directions
       to loop back on itself'''
    if reverse == True:
        loopback = [x for x in rms]
        loopback.reverse()
        rms      = rms + loopback[1:]

    path = []
    i    = 0
    while i < len(rms) - 1:
        path.extend(shortest_path(rms[i], rms[i+1], ignore_doors, stay_zone))
        i += 1
    return path_to_dirs(path)

def step(frm, to, ignore_doors = False, stay_zone = True):
    '''returns the first step needed to take to go from one room to another'''
    steps = shortest_path(frm, to, ignore_doors, stay_zone)
    if steps == None or len(steps) <= 1:
        return None
    return path_to_dirs(steps)[0]



################################################################################
# commands
################################################################################
def cmd_path(ch, cmd, arg):
    '''Usage: path <room>

       Prints out a Python list of the directions needed to move from your
       current location to a specified destination.'''
    try:
        dest, = mud.parse_args(ch, True, cmd, arg, "room")
    except: return

    path = build_patrol([ch.room, dest])

    if len(path) == 0:
        ch.send("Path doesn't exist")
    else:
        ch.send(str(path))



################################################################################
# initialization
################################################################################

# add our commands
mudsys.add_cmd("path", None, cmd_path, "admin", False)

# mud initialization
mud.build_patrol = build_patrol
