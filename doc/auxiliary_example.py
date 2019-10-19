'''
auxiliary_example.py

Provides a simple example of how one might install and interact with
auxiliary data in python. This module installs a new piece of auxiliary data,
and sets up two new commands that allow people to interact with that auxiliary
data.
'''
import auxiliary, storage, mud, mudsys



################################################################################
# auxiliary data implementation
################################################################################
class ExampleAux:
    '''example auxiliary data class. Stores a list of key : value pairs.'''
    def __init__(self, set = None):
        '''Create a new instance of the auxiliary data. If a storage set is
           supplied, read our values from that'''
        # make sure our table of pairs are created
        self.pairs = { }

        # read in our key/value pairs from the list contained in the storage
        # set. NOTE: Storage Lists are NOT Python lists. They are
        # intermediaries for storage, and should not be kept stored on your
        # auxiliary data, only read from or stored to
        if set != None and set.contains("pairs"):
            # notice the sets() at the end? This will return the list as a
            # Python list of storage sets -- the contents of the Storae List
            for one_pair in set.readList("pairs").sets():
                key = one_pair.readString("key")
                val = one_pair.readString("val")
                self.pairs[key] = val

    def copyTo(self, to):
        '''copy the pairs in this aux data to the another.'''
        to.pairs = self.pairs.copy()

    def copy(self):
        '''create a duplicate of this aux data.'''
        newVal = ExampleAux()
        self.copyTo(newVal)
        return newVal

    def store(self):
        '''returns a storage set representation of the auxiliary data'''
        set  = storage.StorageSet()
        list = storage.StorageList()

        # convert our table to a storage list of key:val pairs
        for key, val in self.pairs.iteritems():
            one_pair = storage.StorageSet()
            one_pair.storeString("key", key)
            one_pair.storeString("val", val)
            list.add(one_pair)
        set.storeList("pairs", list)
        return set



################################################################################
# player commands
################################################################################
def cmd_getaux(ch, cmd, arg):
    '''allows people to peek at the value stored in their ExampleAux data'''
    if not arg in ch.aux("example_aux").pairs:
        ch.send("There is no value for '%s'" % arg)
    else:
        ch.send("The val is '%s'" % ch.aux("example_aux").pairs[arg])

def cmd_setaux(ch, cmd, arg):
    '''allows people to set a value stored in their aux data. If no value is
       specified, instead delete a key.'''
    try:
        key, val = mud.parse_args(ch, True, cmd, arg, "word(key) | string(val)")
    except: return

    # are we trying to delete a key?
    if val == None:
        if key in ch.aux("example_aux").pairs:
            del ch.aux("example_aux").pairs[key]
        ch.send("Key deleted.")
    else:
        ch.aux("example_aux").pairs[key] = val
        ch.send("Key '%s' set to '%s'." % (key, val))



################################################################################
# initialization
################################################################################

# install our auxiliary data on characters when this module is loaded.
# auxiliary data can also be installed onto rooms and objects. You can install
# auxiliary data onto more than one type of thing by comma-separating them in
# the third argument of this method.
auxiliary.install("example_aux", ExampleAux, "character")

# add in our two commands
mudsys.add_cmd("getaux", None, cmd_getaux, "admin", False)
mudsys.add_cmd("setaux", None, cmd_setaux, "admin", False)
