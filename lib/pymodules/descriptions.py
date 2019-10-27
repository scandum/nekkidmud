import mud, mudsock, mudsys

def finish_desc_editor(sock, editor_output):
    '''this function is passed to edit_text, and called after a socket exits the
       text editor.'''
    # make sure our character has not somehow vanished.
    # If it is still around, change its description to the output of the editor
    if sock.ch != None:
        sock.ch.desc = editor_output

def cmd_describe(ch, cmd, arg):
    '''Allows a character to set his or her own description.'''
    # first, make sure we have a socket. The text editor pushes an input
    # handler onto the socket's stack. Thus, the socket needs to exist.
    if ch.sock != None:
        ch.sock.edit_text(ch.desc, finish_desc_editor)

# add the 'describe' command to the game
mudsys.add_cmd("describe", None, cmd_describe, "player", True)
