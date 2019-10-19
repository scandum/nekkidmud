################################################################################
#
# example_module.mk
#
# The intent of this makefile is to act as a template makefile for people
# creating new modules for NakedMud(tm). The module installation process has
# been made as simple as I (Geoff Hollis) can make it; for the make file, all
# you really have to do is add a couple lines to SRC so that the main makefile
# knows that you have source files you want to compile. You are also able to
# add new libraries and includes if you need to. To make sure your module is
# compiled, you will have to add an entry for it in the Makefile within src/
#
# Questions and comments should be directed to Geoff Hollis (hollis@ualberta.ca)
#
################################################################################

# If your module has any source files that need to be compiled (it should!
# that's what modules are for!) You will want to add their path relative to
# the main src directory, here. I have provided an example.
SRC     += example_module/foo.c example_module/bar.c

# If your module requires that you use some special flags for compiling, you
# can add them here. For example, if you need to include a directory containing
# headers your module requires. They will be picked up by the main makefile. 
# This line can remain blank if you do not need any flags.
C_FLAGS += 

# Does your module need some libraries? If so, add them here. If not, this line
# can remain blank.
LIBS    += 
