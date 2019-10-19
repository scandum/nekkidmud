Introduction:
------------

This package contains a new build system that I put together for
NakedMud using the python based build tool SCons
(http://www.scons.org). It compiles the unmodified codebase without
errors and uses distutils to determine the python include path (unlike
the bundled make-based system). It will also pass the appropriate
options to the linker (e.g., -Xlinker -export-dynamic). Well, at least
it does the right thing on my system.

Installation:
------------

Extract the tarball into the src directory of your NakedMud. If you
haven't added any C modules, just skip ahead to the build command
list.

You'll have to add the module name to the list of modules in the
SConstruct file (in the source directory: it's SCons' equivalent to
the top-level Makefile):

Have a look for this line in the SConstruct:

modules += ['time', 'socials', 'alias', 'help']

And add your custom modules to the end of the list.

Basic Usage:
-----------

To build, simply run `scons`. To clean, run `scons -c` (unlike Make,
SCons can work out what it's built and delete it). To make a backup,
run `scons backup` (caveat: this won't clean first like the Makefile
does. You'll have to run `scons -c` by hand).

Advanced Usage:
--------------

If, for some reason, a module needs to tinker with the build process,
it an do so by creating a file called SConscript in the top of the
module subdirectory. This file will be parsed by SCons before
compiling anything, and can interact with the build environment using
normal SCons commands. The build environment will need to be imported
with the following line in the SConscript:

  Import('nakedmud')

The presence of a file called .suppress in the top of a module's
subdirectory will also influence the build process:

  - Each .c file named in the .suppress file will not be compiled or
    linked into the NakedMud binary.

  - If the .suppress file contains a line called 'all', no .c file in
    that module directory will be compiled or linked.

Closing Remarks:
---------------

I've tested with Python 2.4.3, NakedMud 3.2.1, NakedMud 3.3a, SCons
0.96.91 and 0.96.92. Hopefully it should be fairly easy to follow.

Oh, and there's no restrictions on its use.

Enjoy,
Jack Kelly

ChangeLog:
---------

Version 1.2 (19 Dec 2006):

* Doesn't guess the LINKFLAGS when sys.platform == 'darwin': it seems
  to cause breakage on OSX 10.4. Apparently it still works without it.

Version 1.1 (10 Nov 2006):

* Builds the list of C files to compile automatically using glob.

* SConscript files in the module subdirectories are now optional. They
  will be sourced if they exist, if a module needs to modify the build
  environment in some strange way. The environment can be imported
  with the line:
    Import('nakedmud')

* The presence of a file called .suppress in the top of a module's
  subdirectory will influence the build process:

  - Each .c file named in the .suppress file will not be compiled or
    linked into the NakedMud binary.

  - If the .suppress file contains a line called 'all', no .c file in
    that module directory will be compiled or linked.

Version 1.0 (29 Oct 2006):

* Initial Version
