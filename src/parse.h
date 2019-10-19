#ifndef PARSE_H
#define PARSE_H
//*****************************************************************************
//
// parse.h
//
// Player commands usually each take their own strict format. For instance,
// the give command takes the format give <player> <object>. However, having to
// parse out these arguments by hand in each command is a bit of a hassle and
// something we can probably cut out most of the legwork for. That's what the
// functions in this header are designed to do.
//
//*****************************************************************************

//
// format is a space-separated list of arguments to be parsed out. The following
// argument types are valid:
//   ch             someone the looker can see, with the specified keyword.
//   obj            something the looker can see, with the specified keyword
//   room           a room in the game with the specified key
//   exit           an exit/dir the looker can see, with the specified keyword
//   word           any single word (ends at whitespace or \0)
//   int            any integer value
//   double         any double value
//   bool           any boolean value (true, false, yes, no, 0, 1)
//   string         all text left to be parsed, up until the \0 delimeter
//
// Any arguments following a | are optional. They are things that will be parsed
// if args has enough arguments in it, but which will be left null otherwise.
//
// If multiple types can plausibly be returned (e.g. trying to close a door or
// an exit), the types can be enclosed in { and } and separated by spaces. If
// the { } syntax is supplied, an additional integer pointer must be supplied 
// to parse_args. It will contain the type of thing parsed, as represented by 
// one of these definitions:
#define PARSE_NONE                  -1
#define PARSE_EXIT                   0
#define PARSE_CHAR                   1
#define PARSE_OBJ                    2
#define PARSE_ROOM                   3
#define PARSE_STRING                 4
#define PARSE_DOUBLE                 5
#define PARSE_INT                    6
#define PARSE_BOOL                   7
// Types will be checked by the order they are provided in. So, for instance
// {room ch obj} would check for a room first, then a ch, and otherwise an obj.
// NOTE: Using double, integer, and boolean values in the { } syntax is frowned
// upon, as assigning non-pointer values to points (which the other types must
// be) can result in wacky and random errors. Stick to using exit, char, obj,
// room, and word within the { } notation.
//
// Optional or mandatory 'flavor' syntax can be provided as well. For instance,
// if you would like people to be able to put "the" in front of object names.
// Mandatory 'flavor' syntax must be surrounded by < and >. Optional 'flavor'
// syntax must be surrounded by [ and ].
//
// .room can be tagged to the end of chs and objs to specify that other 
// chs/objs in the room are looked for.
//
// .world can be tagged to the end of chs and objs to specify that other
// chs/objs in the world are looked for.
//
// .inv can be tagged to the end of objs to specify that objects in the person's
// inventory are looked for.
//
// .eq can be tagged to the end of objs to specify that objects worn by the
// character are looked for.
//
// .multiple can be tagged to the end of a ch, obj, or exit lookup. It 
// signifies that multiple items might be returned. If the .multiple syntax is
// supplied, an additional boolean pointer argument must be supplied to 
// parse_args after the pointer to the thing to be assigned a value. After 
// parse_args finishes, this boolean value will have true/false if a list was 
// returned, or a single item. This argument must be supplied after any multi
// type arguments (use of { and }) in the argument list.
//
// .noself can be tagged to the end of any ch lookup. It signifies that the
// character cannot specify themselves as an argument. If it is used in
// conjunction with .multiple, the looker will be taken out of any list 
// returned.
//
// .invis_ok can be tagged to the end of any ch, obj, or exit lookup. It 
// signifies that the normal constraint of being able to see the target is 
// nullified.
//
// both ch and obj markers require at least ONE argument tagged to the end of
// them, to specify where the are looked for (e.g. ch.room, obj.inv). A ch or
// an obj by itself will not suffice.
//
// parse_args returns true/false if it succeeded or failed. If show_errors is
// true, syntax and type errors and failures to find targets are reported to
// the looker in a  player-friendly format. such as "could not find person, 
// 'bob'" or "the room 'city_square@midgaard' does not exist!"
//
// ********************************* IMPORTANT *********************************
//
// IT IS THE DUTY OF THE USER TO FREE ANY LISTS RETURNED BY PARSE_ARGS. ALSO,
// IF ANY WORDS OR STRINGS ARE QUERIED FOR, THE MEMORY PASSED IN AS args *WILL*
// BE MANGLED BEYOND RECOGNITION TO PREVENT PEOPLE FROM ALSO HAVING TO FREE ANY
// STRING VALUES RETURNED.
//
// ********************************* IMPORTANT *********************************
//
// examples:
//   tell         ch.world.noself string
//   close        [the] {exit obj.room.inv}
//   goto         {room ch.world obj.world}
//   remove       [the] obj.eq.multiple.invis_ok
//   put          [the] obj.inv.multiple [in the] obj.room.inv
//   give         [the] obj.inv.multiple [to] ch.room
//   set          ch.world.multiple word string
//   transfer     ch.world.multiple.noself | [to] room
bool parse_args(CHAR_DATA *looker, bool show_errors, const char *cmd, 
		char *args, const char *syntax, ...);

//
// the same as above, but returns a Python list version of the arguments
void *Py_parse_args(CHAR_DATA *looker, bool show_errors, const char *cmd, 
		    char *args, const char *syntax);

#endif // PARSE_H
