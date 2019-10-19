'''
cmd_inform.py

Contains various commands that are informative in nature. For instance, look,
equipment, inventory, etc...
'''
import mud, mudsys, inform, utils



################################################################################
# utility functions
################################################################################
def cmd_inventory(ch, cmd, arg):
    '''Lists all of the items currently carried in your inventory.'''
    if len(ch.inv) == 0:
        ch.send("You are not carrying anything.")
    else:
        ch.send("You are carrying:")
        visible = utils.find_all_objs(ch, ch.inv, "", None, True)
        utils.show_list(ch, visible, lambda(x): x.name, lambda(x): x.mname)

def cmd_equipment(ch, cmd, arg):
    '''Displays all of the equipment you are currently wearing.'''
    ch.send("You are wearing:")
    inform.show_equipment(ch, ch)

def cmd_who(ch, cmd, arg):
    '''List all of the players currently online.'''
    ch.page(inform.build_who(ch))
    
def cmd_look(ch, cmd, arg):
    '''allows players to examine just about anything in the game'''
    if arg == '':
        inform.look_at_room(ch, ch.room)
    else:
        found, type = mud.generic_find(ch, arg, "all", "immediate", False)

        # what did we find?
        if found == None:
            ch.send("What did you want to look at?")
        elif type == "obj" or type == "in":
            inform.look_at_obj(ch, found)
        elif type == "char":
            inform.look_at_char(ch, found)
        elif type == "exit":
            inform.look_at_exit(ch, found)

        # extra descriptions as well
        ############
        # FINISH ME
        ############



################################################################################
# add our commands
################################################################################
mudsys.add_cmd("inventory", "i",   cmd_inventory, "player", False)
mudsys.add_cmd("equipment", "eq",  cmd_equipment, "player", False)
mudsys.add_cmd("worn",      None,  cmd_equipment, "player", False)
mudsys.add_cmd("who",       None,  cmd_who,       "player", False)

'''
mudsys.add_cmd("look",      "l",   cmd_look,      "player", False)
'''
