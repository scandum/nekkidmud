#!/usr/bin/python
################################################################################
#
# autorun.py
#
# This autorun script was designed to be used with NakedMud. It will 
# automagically restart your MUD any time it crashes. If the MUD is manually
# shut down, the autorun script will terminate. Accepts a port argument.
#
# Geoff Hollis
# http://www.ualberta.ca/~hollis/nakedmud.html
#
################################################################################
import sys               # for sys.argv
from time import sleep   # we delay before restarting
from os   import system  # to start running the MUD

def main(argv = sys.argv[1:]):
    '''
    handles the autorunning of the mud. Can accept 1 optional argument that
    specifies the port number to run under
    '''
    restart_delay = 5     # how long do we delay before a restart (seconds)
    path = './NakedMud'   # the path to the MUD binary
    port = 4000           # the default port we will be running the MUD under

    # parse out our port number if one was supplied
    if len(argv) > 0:
        port = int(argv[0])

    # the command we execute to boot up the MUD
    cmd  = "%s %d" % (path, port)

    # now, while we have not exited without an error, run the MUD 
    # and reboot it every time we exit with an error (we crash)
    while True:
        # run the MUD
        status = system(cmd)

        # exited normally
        if status == 0:
            break;
        else:
            # We should probably see if we can figure out what kind of
            # error caused the crash here, and report it

            # wait out our delay, then restart the MUD
            sleep(restart_delay)

# start us if we're run as a script
if __name__ == "__main__":
    sys.exit(main())
