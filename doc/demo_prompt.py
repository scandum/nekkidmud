'''
demo_prompt.py

Provides a short demo on how to write prompts and input handlers in Python, as
well as how the default prompt can be overwritten in Python.
'''
import mud, mudsys, socket

def new_prompt(sock):
    '''this prompt will be used to override the default NM prompt'''
    sock.send_raw("\r\nnew prompt> ")

def example_prompt(sock):
    '''used as an example of how to interact with the input handler stack'''
    sock.send_raw("\r\nexample prompt> ")

def example_handler(sock, arg):
    '''used as an example of how to interact with the input handler stack'''
    if arg.upper() == 'Q':
        sock.pop_ih()
    else:
        sock.send("You typed " + arg + ". Q to quit.")

def cmd_pyih(ch, cmd, arg):
    '''used as an example of how to interact with the input handler stack'''
    ch.socket.push_ih(example_handler, example_prompt)
    ch.send("New input handler set.")


# add the command that allows us to interact with the input handler stack
mudsys.add_cmd("pyih", None, cmd_pyih, "admin", False)

# override our default prompt with a new one
mudsys.show_prompt = new_prompt
