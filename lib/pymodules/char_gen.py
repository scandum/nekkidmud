'''
char_gen.py

The basic character generation module. Allows accounts to create new characters
with basic selection for name, sex, and race.
'''
import mudsys, mud, socket, char, hooks



def check_char_name(arg):
    '''checks to make sure the character name is valid. Names are valid if they
       are greater than 2 characters, less than 13, and comprise only alpha
       characters.'''
    return len(arg) >= 3 and len(arg) <= 12 and arg.isalpha()

def cg_name_handler(sock, arg):
    if not check_char_name(arg):
        sock.send("{cIllegal name, please pick another.{n\r\n")
    elif mudsys.player_exists(arg):
        sock.send("{cA player with that name already exists.{n\r\n")
    elif mudsys.player_creating(arg):
        sock.send("{cA player is already being created with that name.{n\r\n")
    elif arg.lower().startswith("guest"):
        sock.send("{cCharacter names cannot begin with 'guest'.{n\r\n")
    else:
        name = arg[0].upper() + arg[1:]
        ch = mudsys.create_player(name)

        if ch == None:
            sock.send("{cIllegal name, please pick another.{n\r\n")
        else:
            mudsys.attach_char_socket(ch, sock)
            ch.rdesc = ch.name + " is here."
            sock.pop_ih()

def cg_sex_handler(sock, arg):
    try:
        result = {
            'M' : 'male',
            'F' : 'female',
            }[arg[0].upper()]
        sock.ch.sex = result
        sock.pop_ih()
    except:
        sock.send("{cInvalid sex, try again.{n\r\n")

def cg_race_handler(sock, arg):
    if not mud.is_race(arg, True):
        sock.send("{cInvalid race selection, try again.{n\r\n")
    else:
        sock.ch.race = arg.lower()
        sock.pop_ih()

def cg_finish_handler(sock, arg):
    # pop our input handler for finishing character generation
    sock.pop_ih()

    # log that the character created
    mud.log_string("New player: " + sock.ch.name + " has entered the game.")
    
    # register and save him to disk and to an account
    mudsys.do_register(sock.ch)

    # make him exist in the game for functions to look him up
    mudsys.try_enter_game(sock.ch)

    # run the init_player hook
    hooks.run("init_player", hooks.build_info("ch", (sock.ch,)))
    
    # attach him to his account and save the account
    sock.account.add_char(sock.ch)
    mudsys.do_save(sock.account)
    mudsys.do_save(sock.ch)

    # clear their screen
    sock.ch.act("clear")
    
    # send them the motd
    sock.ch.page(mud.get_motd())

    # make him look at the room
    sock.ch.act("look")

    # run our enter hook
    hooks.run("enter", hooks.build_info("ch rm", (sock.ch, sock.ch.room)))

def cg_name_prompt(sock):
    sock.send_raw("What is your character's name? ")

def cg_sex_prompt(sock):
    sock.send_raw("What is your sex (M/F)? ")

def cg_race_prompt(sock):
    sock.send("Available races are: ")
    sock.send(mud.list_races(True))
    sock.send_raw("\r\nPlease enter your choice: ")

def cg_finish_prompt(sock):
    sock.send_raw("{c*** Press enter to finish character generation:{n ")



################################################################################
# character generation hooks
################################################################################
def char_gen_hook(info):
    '''Put a socket into the character generation menu when character generation
       hooks are called.
    '''
    sock, = hooks.parse_info(info)
    sock.push_ih(mudsys.handle_cmd_input, mudsys.show_prompt, "playing")
    sock.push_ih(cg_finish_handler, cg_finish_prompt)
    sock.push_ih(cg_race_handler, cg_race_prompt)
    sock.push_ih(cg_sex_handler, cg_sex_prompt)
    sock.push_ih(cg_name_handler, cg_name_prompt)

def guest_gen_hook(info):
    sock, = hooks.parse_info(info)
    sock.push_ih(mudsys.handle_cmd_input, mudsys.show_prompt, "playing")

    # make the guest character
    ch = mudsys.create_player("Guest")

    # oops...
    if ch == None:
        sock.send("Sorry, there were issues creating a guest account.")
        sock.close()

    mudsys.attach_char_socket(ch, sock)
    ch.rdesc = "a guest player is here, exploring the world."
    ch.name  = ch.name + str(ch.uid)
    ch.race  = "human"

    # log that the character created
    mud.log_string("Guest character created (id %d)." % ch.uid)

    # make him exist in the game for functions to look him up
    mudsys.try_enter_game(ch)

    # run the init_player hook
    hooks.run("init_player", hooks.build_info("ch", (ch,)))

    # clear our screen
    ch.act("clear")

    # send them the motd
    ch.page(mud.get_motd())
    
    # make him look at the room
    ch.act("look")

    # run our enter hook
    hooks.run("enter", hooks.build_info("ch rm", (ch, ch.room)))



################################################################################
# loading and unloading the module
################################################################################
hooks.add("create_character", char_gen_hook)
hooks.add("create_guest",     guest_gen_hook)

def __unload__():
    '''removes the hooks for character generation'''
    hooks.remove("create_character", char_gen_hook)
    hooks.remove("create_guest",     guest_gen_hook)

