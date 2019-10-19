"""
doc.py

This module allows documentation for Python classes and modules to be viewed
in-game via the 'doc' command. 
"""
import pydoc, os, mudsys, display



################################################################################
# local variables
################################################################################

# where do we store documentation?
HTML_DOC_DIR = "../html/pydocs"

shortcuts = { "ch"      : "char",
              "sock"    : "mudsock",
              }

# just a list of all our builtin modules
builtins = [
    "char",
    "room",
    "obj",
    "exit",
    "account",
    "mudsock",
    "mud",
    "mudsys",
    "hooks",
    "event",
    "auxiliary",
    "storage",
    "olc",
    ]

# append all of our builtins to suggested reading list
suggested_reading = [mod for mod in builtins]

def register_module_doc(modname, package = None, root = "pymodules"):
    """Add a new module name to suggested_reading. If modname is a package,
       recursively add its packages as well
    """
    fname = root + "/" + modname.replace(".", "/")
    suggested_reading.append(modname)
    if os.path.isdir(fname):
        for file in os.listdir(fname):
            if (file.endswith(".py") or not "." in file) and not file[0] in "._":
                module = modname + "." + file.split(".", 1)[0]
                register_module_doc(module, root)
 
# now, append all of our Python packages and modules
for fname in os.listdir("pymodules/"):
    # look for modules and packages
    if (fname.endswith(".py") or not "." in fname) and not fname[0] in "._":
        modname = fname.split(".", 1)[0]
        register_module_doc(modname)



################################################################################
# player commands
################################################################################
def cmd_htmldoc(ch, cmd, arg):
    """Creates html documentation for all registered modules. html files will
       be saved to html/pydocs/
    """
    try:
        os.makedirs(HTML_DOC_DIR)
    except: pass
    doc = pydoc.HTMLDoc()
    for modname in suggested_reading:
        todoc = pydoc.locate(modname)
        if todoc != None:
            fname = HTML_DOC_DIR + "/" + modname + ".html"
            fl    = file(fname, "w+")
            fl.write(doc.page(modname, doc.document(todoc)))
            fl.close()

    builtin_index = doc.multicolumn([doc.modulelink(pydoc.locate(modname)) for modname in builtins], lambda x: x)
    
    # build our index page. That includes things in pymodules/ and builtins
    index_contents ="".join([doc.section("<big><strong>builtins</big></strong>",
                                         'white', '#ee77aa', builtin_index),
                             doc.index("../lib/pymodules/")])

    # go over all of our builtins and add them to the index
    index = file(HTML_DOC_DIR + "/index.html", "w+")
    index.write(doc.page("index", index_contents))
    index.close()
    
    ch.send("html documentation generated for all known modules.")

def cmd_doc(ch, cmd, arg):
    """Return Python documentation for the specified module, class, function,
       etc... for example:
       
       > doc char.Char

       Will return all available documentation for the Char class.
    """
    if arg == "":
        ch.page("\r\n".join(display.pagedlist({ "Topics" : suggested_reading },
                                              header = "Suggested doc readings include:")))
    else:
        # just because sometimes I forget periods
        arg = arg.replace(" ", ".")

        # are we looking for a shortcut value?
        if arg in shortcuts:
            arg = shortcuts[arg]

        # try to find what we're documenting
        todoc = pydoc.locate(arg)
        if todoc == None:
            ch.send("Could not find Python documentation on: '%s'" % arg)
        else:
            doc = pydoc.TextDoc()
            ch.page(doc.document(todoc).replace("{", "{{"))



################################################################################
# initialization
################################################################################
mudsys.add_cmd("doc",     None, cmd_doc,     "wizard", False)
mudsys.add_cmd("htmldoc", None, cmd_htmldoc, "admin",  False)
