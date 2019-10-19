################################################################################
#
# Python scripting stuff.
#
# To embed python as a scripting language, you will probably have to toy around
# with the make process. Sadly, this process can be very different from computer
# to computer. I've tried to set the defaults to what I've seen to be the most
# common settings, but chances are you will have to tweak a couple things in
# this file. There are three variables in this makefile you will have to edit.
# They are PYTHONTOP, LIBS, and C_FLAGS. I'll try to walk you through 
# everything you'll need to do to get these variables all proper-like:
#
# STEP 1: Locating Python
#   Figure out where your python directory is. If you do not know how to do
#   this, you can do a "locate" for one of the Python headers:
#     > locate Python.h
#
#   wait for the path to Python.h to show up. You'll have to doctor it a bit;
#   from the path, you will want to figure out the top directory for python, and
#   where all of the header files are located. If the path looks something like:
#     /usr/lib/python2.4/include/Python.h
#
#   PYTHONTOP would be /usr/lib/python2.4
#   and C_FLAGS should include -I$(PYTHONTOP)/include
#
#   If you are not running Python v2.4 or newer, it is strongly suggested you
#   upgrade before continuing (see http://www.python.org).
#
#
# STEP 2: Getting the linking requirements for python.
#   This is a relatively easy step; start up python (from shell, type "python")
#   and do this:
#
#   >>> import distutils.sysconfig
#   >>> distutils.sysconfig.get_config_var('LINKFORSHARED')
#
#   a string will print that looks something like:
#   '-Xlinker -export-dynamic'
#
#   copy this string (minus the two surrounding ') to the LIBS variable at the
#   end of this makefile.
#
#
# STEP 3: Compiling
#   Try compiling. If you get a bunch of errors, go onto step 4.
#
#
# STEP 4: Finding the runtime library.
#   Some operating systems will require that you take extra steps to link in 
#   the python runtime library. This is the step people seem to be having most
#   trouble with, so you may want to pay special attention to this step. What
#   you are going to need to do is find the location of your python runtime 
#   library and add it to the LIBS variable like you did for the other 
#   libraries in step 2. Change your directory to PYTHONTOP and type:
#     find . -name "libpython*a" -print
#
#   When it spits out the address to the python library, add a string like this
#   to the LIBs variable at the bottom of this file:
#     -L/<path> -lpythonXXX
#
#   Where <path> is the path to the library (minus the library file itself), 
#   and XXX is the version number of your python. So, for instance, on my
#   computer, I had to add the following line to LIBS:
#     -L/usr/local/lib/python2.4/config -lpython2.4
#
#   If this is not working for you, there are other ways to link in the Python
#   runtime library. For instance, you might be able to simply add the whole
#   pathname of the library to your LIBS variable. The above example would
#   translate into:
#     /usr/local/lib/python2.4/config/libpython2.4.a
#
#   There are many ways to skin this cat. Google should be able to provide you
#   with a couple others if these two are not working for you. I would also be
#   more than willing to assist you if you are running into problems.
#
#   At this point, you should attempt to compile your code. If you are still
#   getting errors, move on to step 5.
#
# 
# STEP 5: Adding more libraries.
#   Still won't compile? You may need to add some extra libraries that Python
#   requires. These are usually quite easy to find; if you are getting messages
#   messages like "undefined references to sin", "undefined reference to
#   pthread_start" etc... you will want to add the libraries that these 
#   functions belong to. for each one that comes up, do a "man XXX" and read
#   the man file until you figure out which library the function belongs to.
#   Ones that I found were neccessary for me to include on Fedora were -lm,
#   -lutil and -ldl . This, of course, will vary from OS to OS. Add all of the
#   libraries you need to the end of LIBS, where you put the libraries from
#   STEP 2. Try Recompiling. If things still aren't working for you, proceed
#   to STEP 6.
#
#
# STEP 6: On your own
#   If you've gotten to this step, I don't really have much more to say. I 
#   would suggest finding some Python BBs and explaining your problem. Perhaps
#   someone a bit more experienced will be able to help out.
#
################################################################################
SRC  += scripts/scripts.c       \
	scripts/pychar.c        \
	scripts/pyobj.c         \
	scripts/pymud.c         \
	scripts/pymudsys.c      \
	scripts/pyhooks.c       \
	scripts/pyroom.c        \
	scripts/pyexit.c        \
	scripts/pyaccount.c     \
	scripts/pysocket.c      \
	scripts/script_editor.c \
	scripts/pyplugs.c       \
	scripts/pyevent.c       \
	scripts/pystorage.c     \
	scripts/pyauxiliary.c   \
	scripts/triggers.c      \
	scripts/trigedit.c      \
	scripts/trighooks.c     \
	scripts/pyolc.c


# the top level directory of python.
PYTHONTOP = /usr/include

C_FLAGS  += -I$(PYTHONTOP)/python2.7 -Wno-char-subscripts
# C_FLAGS  += -I$(PYTHONTOP)/python3.5 -Wno-char-subscripts

# libraries we have to include.
LIBS     += -Xlinker -export-dynamic -lm -ldl -lutil -L/usr/lib/python2.7/config -lpython2.7

# the folder where python headers are located
#C_FLAGS  += -I$(PYTHONTOP)/python2.4

# libraries we have to include.
#LIBS     += -Xlinker -export-dynamic -lm -ldl -lutil -L/usr/lib/python2.4/config -lpython2.4
