'''
account_handler.py

The login and creation of accounts, and handles all account procedures, loading
and deleting, of characters.
'''
import mud, mudsys, char, hooks, account, socket, event, telnetlib, utils

# control sequences for squelching passwords
squelch   = telnetlib.IAC + telnetlib.WILL + telnetlib.ECHO
unsquelch = telnetlib.IAC + telnetlib.WONT + telnetlib.ECHO



def check_acct_name(name):
    '''Makes sure an account name is valid'''
    return (len(name) > 3 and len(name) < 13 and
            name[0].isalpha() and name.isalnum())

def login_method_prompt(sock):
    sock.send_raw("Choose an option: ")

def acct_new_password_prompt(sock):
    sock.send_raw("\r\nWhat is your new password? " + squelch)

def acct_password_prompt(sock):
    sock.send_raw("What is your password? " + squelch)

def acct_wait_dns_prompt(sock):
    sock.send_raw(" Resolving your internet address, please have patience... ")

def try_create_account(sock, name, psswd):
    if mudsys.account_exists(name):
        return False
    elif mudsys.account_creating(name):
        return False
    elif not check_acct_name(name):
        return False
    else:
        # creating a new account
        mud.log_string("Account '" + name + "' is trying to create.")

        # create our new account
        acct = mudsys.create_account(name)
        if acct == None:
            return False
        else:
            mudsys.attach_account_socket(acct, sock)
            mudsys.set_password(acct, psswd)
            sock.pop_ih()
            sock.push_ih(acct_menu_handler, acct_main_menu)

            #sock.push_ih(acct_finish_handler, acct_finish_prompt)

            # log that the account created
            mud.log_string("New account '" + acct.name + "' has created.")

            # register and save the account to disk
            mudsys.do_register(acct)

            return True
    return False

def try_load_account(sock, name, psswd):
    '''Attempt to load an account with the given name and password.'''
    if not mudsys.account_exists(name):
        return False
    else:
        acct = mudsys.load_account(name)
        if not mudsys.password_matches(acct, psswd):
            return False

        # successful load
        mudsys.attach_account_socket(acct, sock)
        sock.pop_ih()
        sock.push_ih(acct_menu_handler, acct_main_menu)
        return True
    return False

def login_method_handler(sock, arg):
    args = arg.split()
    if len(args) == 0:
        return
    args[0] = args[0].lower()

    if "create".startswith(args[0]):
        if len(args) != 3:
            return
        elif mudsys.account_exists(args[1]):
            sock.send("{cAn account by that name already exists.{n\r\n")
        elif not check_acct_name(args[1]):
            txt = "{cThat is an invalid account name. Your account name must "\
                  "only consist of characters and numbers, and it must be " \
                  "4 and 12 characters in length. The first character MUST be "\
                  "a letter. Please pick another.{n"
            sock.send(mud.format_string(txt, False))

        elif not try_create_account(sock, args[1], args[2]):
            sock.send("Your account was unable to be created.")

    elif "load".startswith(args[0]):
        if len(args) != 3:
            return
        elif not try_load_account(sock, args[1], args[2]):
            sock.send("{cInvalid account name or password.{n\r\n")
    elif "guest".startswith(args[0]):
        if (mudsys.sys_getval("lockdown") != '' and
            not utils.is_keyword(mudsys.sys_getval("lockdown"), "player")):
            sock.send("{cThe mud is currently locked out to new players.{n\r\n")
        else:
            sock.pop_ih()
            hooks.run("create_guest", hooks.build_info("sk", (sock,)))

def acct_wait_dns_handler(sock, arg):
    # do nothing
    return

def acct_new_password_handler(sock, arg):
    '''asks a new account for a password'''
    sock.send_raw(unsquelch)
    if len(arg) > 0:
        # put in mudsys to prevent scripts from messing with passwords
        mudsys.set_password(sock.account, arg)
        sock.pop_ih()

def acct_password_handler(sock, arg):
    '''asks an account to verify its password'''
    # password functions put in mudsys to prevent scripts from
    # messing with passwords
    sock.send_raw(unsquelch)
    if not mudsys.password_matches(sock.account, arg):
        sock.send("Incorrect password.")
        sock.close()
    else:
        # password matches, pop our handler and go down a level
        sock.pop_ih()

def find_reconnect(name):
    '''searches through the character list for a PC whose name matches the
       supplied name'''
    name = name.lower()
    for ch in char.char_list():
        if ch.is_pc and name == ch.name.lower():
            return ch
    return None

def acct_load_char(sock, name):
    '''loads a character attached to the account. Argument supplied must be a
       name of the corresponding character'''
    chars = sock.account.characters()
    
    if not name.lower() in [n.lower() for n in sock.account.characters()]:
        sock.send("A character by that name does not exist on your account.")
    else:
        # first, try a reconnect
        ch = find_reconnect(name)

        # no reconnect? Try loading our character
        if not ch == None:
            # swap the sockets
            old_sock = ch.sock

            # attach and detach put in mudsys to prevent scripts from
            # messing around with the connections between chars and sockets
            mudsys.detach_char_socket(ch)
            mudsys.attach_char_socket(ch, sock)
            
            if old_sock != None:
                old_sock.close()
            mud.log_string(ch.name + " has reconnected.")
            # ch.act("clear")
            ch.send("You take over a body already in use.")
            ch.act("look")
            hooks.run("reconnect", hooks.build_info("ch", (ch,)))
            sock.push_ih(mudsys.handle_cmd_input, mudsys.show_prompt, "playing")

        else:
            # load our character. Put in mudsys to prevent scripts from using it
            ch = mudsys.load_char(name)
            if ch == None:
                sock.send("The player file for " + name + " is missing.")
            elif (mudsys.sys_getval("lockdown") != '' and
                  not ch.isInGroup(mudsys.sys_getval("lockdown"))):
                sock.send("You are currently locked out of the mud.")
                mud.extract(ch)
            else:
                # attach put in mudsys to prevent scripts from messing with
                # character and socket links
                mudsys.attach_char_socket(ch, sock)
                if mudsys.try_enter_game(ch):
                    mud.log_string(ch.name + " has entered the game.")
                    sock.push_ih(mudsys.handle_cmd_input, mudsys.show_prompt,
                                 "playing")
                    # ch.act("clear")
                    ch.page(mud.get_motd())
                    ch.act("look")
                    hooks.run("enter", hooks.build_info("ch rm", (ch, ch.room)))
                else:
                    sock.send("There was a problem entering the game. " + \
                              "try again later.")
                    # detach put in mudsys t prevent scripts from messing with
                    # character and socket links
                    mudsys.detach_char_socket(ch, sock)
                    mud.extract(ch)

def acct_menu_handler(sock, arg):
    '''parses account commands (new character, enter game, quit, etc)'''
    if len(arg) == 0:
        return

    args = arg.split()
    args[0] = args[0].upper()
    
    if args[0].isdigit():
        opts = sock.account.characters()
        arg  = int(args[0])
        if arg < 0 or arg >= len(opts):
            sock.send("Invalid choice!")
        else:
            acct_load_char(sock, opts[arg])
    elif args[0] == 'L':
        if len(args) == 1:
            sock.send("Which character would you like to load?")
        else:
            acct_load_char(sock, args[1])
    elif args[0] == 'Q' or "QUIT".startswith(args[0]):
        sock.send("Come back soon!")
        mudsys.do_save(sock.account)
        sock.close()
    elif args[0] == 'P':
        # sock.push_ih(acct_confirm_password_handler,acct_confirm_password_prompt)
        sock.push_ih(acct_new_password_handler, acct_new_password_prompt)
        sock.push_ih(acct_password_handler, acct_password_prompt)
    elif args[0] == 'N':
        if "player" in [x.strip() for x in mudsys.sys_getval("lockdown").split(",")]:
            sock.send("New characters are not allowed to be created at this time.")
        else:
            hooks.run("create_character", hooks.build_info("sk", (sock,)))
    else:
        sock.send("Invalid choice!")

def display_acct_chars(sock):
    '''shows the socket a prettied list of characters tied to the account it
       has attached. Prints three names per line.'''
    num_cols   = 3
    print_room = (80 - 10*num_cols)/num_cols
    fmt        = "  {c%2d{n) %-" + str(print_room) + "s"

    sock.send("{w\r\nPlay a Character:")
    i = 0
    for ch in sock.account.characters():
        sock.send_raw(fmt % (i, ch))
        if i % num_cols == num_cols - 1:
            sock.send_raw("\r\n")
        i = i + 1

    if not i % num_cols == 0:
        sock.send_raw("\r\n")

def acct_main_menu(sock):
    '''displays the main menu for the account and asks for a command'''
    line_buf = "%-38s" % " "

    # make the account menu look pretty
    img = ["+--------------------------------------+",
           "|                                      |",
           "|                                      |",
           "|                                      |",
           "|                                      |",
           "|                                      |",
           "|                                      |",
           "|                                      |",
           "|                                      |",
           "|                                      |",
           "|               I M A G E              |",
           "|                H E R E               |",
           "|                                      |",
           "|                                      |",
           "|                                      |",
           "|                                      |",
           "|                                      |",
           "|                                      |",
           "|                                      |",
           "|                                      |",
           "|                                      |",
           "|                                      |",
           "+--------------------------------------+"]

    # the lines for displaying our account options
    opts  = [ ]
    chars = sock.account.characters()
    chars.sort()
    if len(chars) > 0:
        opts.append("  {wPlay A Character:                   ")
        for i in range(len(chars)):
            opts.append("    {n[{c%1d{n] %-30s" % (i,chars[i]))
        opts.append(line_buf)

    # append our other displays
    opts.append("  {wAccount Management:                 ")
    opts.append("    {n[{cP{n]assword change                 ")
    opts.append("    {n[{cN{n]ew character                   ")
    opts.append("    {n[{cL{n]oad character <name>           ")
    opts.append(line_buf)
    opts.append(line_buf)
    opts.append(line_buf)

    # fill up our height to be in line with the image
    while len(opts) < len(img) - 2:
        opts.insert(0, line_buf)

    # append our title
    opts.insert(1, "            {nW E L C O M E  T O        ")
    opts.insert(2, "             {nN A K E D M U D          ")
            
    # display all of our info
    sock.send("")
    for i in range(max(len(opts), len(img))):
        if i < len(opts):
            sock.send_raw(opts[i])
        else:
            sock.send_raw(line_buf)
        if i < len(img):
            sock.send_raw("{n%s" % img[i])
        sock.send("")
        
    sock.send_raw("{nEnter choice, or Q to quit: ")



################################################################################
# events for blocking action when dns lookup is in progress
################################################################################
def dns_check_event(owner, void, info):
    '''After a socket connects, monitor their hostname until dns lookup is
       complete. Then, put the socket into the account handler.
    '''
    sock, = hooks.parse_info(info)
    if sock != None and sock.can_use:
        sock.send(" Lookup complete.")
        sock.send("================================================================================")
        sock.send(mud.get_greeting())
        sock.pop_ih()
        sock.bust_prompt()
        # mud.log_string("new connection from " + sock.hostname)
    else:
        event.start_event(None, 0.2, dns_check_event, None, info)



################################################################################
# account handler hooks
################################################################################
def account_handler_hook(info):
    '''When a socket connects, put them into the account handler menu.'''
    # put a nonfunctional prompt up while waiting for the DNS to resolve
    sock, = hooks.parse_info(info)
    sock.push_ih(login_method_handler, login_method_prompt)
    mud.log_string("new socket, %d, attempting to connect" % sock.uid)
    sock.send(mud.get_greeting())
    sock.send("== Options Are ================================================================")
    sock.send("    Load account   : load   <account> <password>")
    sock.send("    Create account : create <account> <password>")
    sock.send("    Play as guest  : guest")
    sock.send("===============================================================================")
    sock.send("")

    '''
    sock.push_ih(acct_wait_dns_handler, acct_wait_dns_prompt)
    sock.send("================================================================================")
    event.start_event(None, 0.2, dns_check_event, None, info)
    '''

def copyover_complete_hook(info):
    sock, = hooks.parse_info(info)
    sock.push_ih(acct_menu_handler, acct_main_menu)
    sock.push_ih(mudsys.handle_cmd_input, mudsys.show_prompt, "playing")



################################################################################
# loading and unloading the module
################################################################################
hooks.add("receive_connection", account_handler_hook)
hooks.add("copyover_complete",  copyover_complete_hook)

def __unload__():
    '''removes the hooks for account handling'''
    hooks.remove("receive_connection", account_handler_hook)
    hooks.remove("copyover_complete",  copyover_complete_hook)
