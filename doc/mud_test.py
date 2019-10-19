from mudsys import add_cmd
import auxiliary
import storage

# Example auxiliary data class. Holds a single string variable that
# people are allowed to get and set the value of
class ExampleAux:
    # Create a new instance of the auxiliary data. If a storage set is supplied,
    # read our values from that
    def __init__(self, set = None):
        if not set:
            self.val = "abcxyz"
        else:
            self.val = set.readString("val")

    # copy the variables in this auxiliary data to another auxiliary data
    def copyTo(self, to):
        to.val = self.val

    # create a duplicate of this auxiliary data
    def copy(self):
        newVal = ExampleAux()
        newVal.val = self.val
        return newVal

    # returns a storage set representation of the auxiliary data
    def store(self):
        set = storage.StorageSet()
        set.storeString("val", self.val)
        return set

# allows people to peek at the value stored in their ExampleAux data
def cmd_getaux(ch, cmd, arg):
    aux = ch.account.getAuxiliary("example_aux")
    ch.send("The val is " + aux.val)

# allows people to set the value stored in their ExampleAux data
def cmd_setaux(ch, cmd, arg):
    aux = ch.account.getAuxiliary("example_aux")
    aux.val = arg
    ch.send("val set to " + arg)

# install our auxiliary data on characters when this module is loaded.
# auxiliary data can also be installed onto rooms and objects. You can install
# auxiliary data onto more than one type of thing by comma-separating them in
# the third argument of this method.
auxiliary.install("example_aux", ExampleAux, "account")

# add in our two commands
add_cmd("getaux", None, cmd_getaux, "admin", False)
add_cmd("setaux", None, cmd_setaux, "admin", False)
