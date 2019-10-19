"""
colour.py

NakedMud's base colour module. Contains functions for outbound text processing
to add ASCII colour codes to the text.
"""
import mud, mudsock, hooks



# symbols and values we need for processing colours
base_colour_marker   = '{'
colour_start         = '\x1b['
cDARK                = '0'
cLIGHT               = '1'

# colour symbols
c_none    = 'n'
c_dark    = 'd'
c_red     = 'r'
c_green   = 'g'
c_yellow  = 'y'
c_blue    = 'b'
c_magenta = 'p'
c_cyan    = 'c'
c_white   = 'w'

# maps of colour symbols to their ASCII number type thing
base_colours = { c_none    : '0',
                 c_dark    : '30',  
                 c_red     : '31',  
                 c_green   : '32',  
                 c_yellow  : '33',  
                 c_blue    : '34',  
                 c_magenta : '35',  
                 c_cyan    : '36',  
                 c_white   : '37' }



################################################################################
# colour processing hooks
################################################################################
def process_colour_hook(info):
    """When outbound text is being processed, find colour codes and replace them
       by the proper colour escape sequences."""
    sock,  = hooks.parse_info(info)
    buf    = sock.outbound_text
    newbuf = []

    # go through our outbound text and process all of the colour codes
    i = 0
    while i < len(buf):
        if buf[i] == base_colour_marker and i + 1 < len(buf):
            i = i + 1
            char  = buf[i]

            # upper case is bright, lower case is dark
            shade = cLIGHT
            if char == char.lower():
                shade = cDARK

            # if it's a valid colour code, build it
            if base_colours.has_key(char.lower()):
                newbuf.append(colour_start + shade + ';' + \
                              base_colours[char.lower()] + 'm')

            # if it was an invalid code, ignore it
            else:
                newbuf.append(base_colour_marker)
                if not char == base_colour_marker:
                    i = i - 1

        else:
            newbuf.append(buf[i])
        i = i + 1

    # replace our outbound buffer with the processed text
    sock.outbound_text = ''.join(newbuf)



################################################################################
# initializing and unloading our hooks
################################################################################
hooks.add("process_outbound_text",   process_colour_hook)
hooks.add("process_outbound_prompt", process_colour_hook)

def __unload__():
    '''detaches our colour module from the game'''
    hooks.remove("process_outbound_text",   process_colour_hook)
    hooks.remove("process_outbound_prompt", process_colour_hook)
