###############################################################################
# makefile for nakedmud
#
# Thanks go out to Tyche for bringing my attention to "Recursive Make Considered
# Harmful" by Peter Miller. Many of the make conventions used in this makefile
# were taken from Miller's article. The reference is:
# 
# Miller, P.A. (1998). Recursive Make Considered Harmful,
#    AUUGN Journal of AUUG Inc., 19(1), pp. 14-25.
#
###############################################################################

# compiler to use
CC = gcc -g

# the name of the binary file created that you will run to start your mud
BINARY  := NakedMud

# the name of the project - essentially, the executable sans capital letters
PROJECT := $(shell echo $(BINARY) | tr A-Z a-z)

# we'd like some messages we print out to be in color. Here is the string for
# the color (cyan) that we print all important messages in. Any echos that use
# color must also have the -e option (e.g. @echo -e "echo here")
COLOR   := \x1B[36m
NOCOLOR := \x1B[0m

# the modules we have installed -- mandatory modules go here
MODULES := dyn_vars set_val olc2 editor items scripts

# optional modules go on this line
MODULES += time socials alias help2 color mth

# flags to use during compilation
C_FLAGS := -Wall -g -ggdb -O2

# extra libraries if required
LIBS    := -lz -lpthread -lcrypt

# each module will add to this from its module.mk file
SRC     := gameloop.c mud.c utils.c interpret.c handler.c inform.c \
	   action.c save.c socket.c io.c strings.c event.c \
	   \
	   races.c \
	   \
	   log.c auxiliary.c \
	   \
	   world.c character.c room.c exit.c extra_descs.c object.c body.c \
	   zone.c room_reset.c account.c \
	   \
	   list.c property_table.c hashtable.c map.c storage.c set.c \
	   buffer.c bitvector.c numbers.c prototype.c hooks.c parse.c \
	   near_map.c command.c filebuf.c



# include the description for each module. These will add to SRC
# C_FLAGS, LIBS, etc...
include $(patsubst %,%/module.mk, $(MODULES))

# determine the object files
O_FILES := $(patsubst %.c,%.o, $(filter %.c, $(SRC)))



################################################################################
# variables involved in creating backups of the mud
################################################################################

# when performing a backup, these are the directories we'd like to include
BACKUP_DIRS := src lib doc html

# the name of the backup file we will be creating (time-stamping added)
BACKUP_FILE := backup/$(PROJECT)-$(shell date +%Y-%m-%d-%Hh%Mm).tar.tgz



################################################################################
# make commands
################################################################################
all: $(O_FILES)
	@$(CC) -o $(BINARY) $(O_FILES) $(LIBS)
	@echo -e "$(COLOR)$(BINARY) successfully compiled."\
		 "To run your mud, use ./$(BINARY) [port] &$(NOCOLOR)\n"\

# back up everything worth backing up
backup: clean
	@echo "Backing up: $(BACKUP_DIRS)"
	@(cd ..; tar -zcf $(BACKUP_FILE) $(BACKUP_DIRS))
	@echo -e "$(COLOR)New backup created: $(BACKUP_FILE)$(NOCOLOR)"

# make the object files. The modules are sort of annoying, in that 
# if we do not use -o, the object files will be compiled in this directory,
# and then every time we re-make, the module .o files will be recompiled
# because they are not in their home directory. So... we have to explicitly
# say where the .o files need to go
.c.o: all
	@echo "Compiling $<"
	@$(CC) -c $(C_FLAGS) -o $(patsubst %.c,%.o, $<) $<

# clear all of the .o files and all of the save files that emacs makes. Also
# clears all of our Python files
clean:
	@rm -f $(BINARY)
	@rm -f *.o $(patsubst %,%/*.o, $(MODULES))
	@rm -f *.d $(patsubst %,%/*.d, $(MODULES))
	@rm -f *~ $(patsubst %,%/*~, $(MODULES))
	@rm -f ../lib/pymodules/*~
	@rm -f ../lib/pymodules/*.pyc
	@echo "$(PROJECT) source files cleaned"

# include all of our dependencies
include $(patsubst %.c,%.d, $(SRC))

# calculate all of our dependencies. It's messy, but it works
%.d: %.c
	@echo "Building dependencies for $(patsubst %.d,%.c, $@)"
	@case $(shell dirname $@) in					 \
	"" | ".") 							 \
	  gcc $(C_FLAGS) -MM $(patsubst %.d,%.c, $@) | 			 \
	    sed -e 's@^\(.*\)\.o:@\1.d \1.o:@' > $@			 \
	;; 								 \
	*) 								 \
	  gcc $(C_FLAGS) -MM $(patsubst %.d,%.c, $@) | 			 \
	    sed -e "s@^\(.*\)\.o:@$(shell dirname $@)/\1.d $(shell dirname $@)/\1.o:@" > $@ 					 			 \
        ;;                                                               \
	esac
