'''
display.py

Various utilities for displaying information such as tables and metered values.
'''
import math, mud



def table_row(info, align="left", caps = "|", space=" ",
              center_filler=" ", width=79):
    '''display a table row like this:
       | info       |
       '''
    # account for color codes
    width += info.count("{")*2

    fmt = "%s%s%%-%ds%s%s" % (caps, space, width-len(caps)*2-len(space)*2,
                              space, caps)
    if align == "center":
        info = info.center(width-len(caps)*2-len(space)*2, center_filler)
    elif align == "right":
        fmt = "%s%s%%%ds%s%s" % (caps, space, width-len(caps)*2-len(space)*2,
                                 space, caps)
    return fmt % info

def table_splitrow(left, right, align="left", caps = "|", space=" ",
                   center_filler=" ", width=79):
    '''display two columns like this:
       | left    | right    |
       '''
    width -= len(caps)*2 + len(space)*4
    linfo = table_row(left,  align, "", "", center_filler, width/2)
    rinfo = table_row(right, align, "", "", center_filler, width/2)
    fmt = "%s%s%%s%s%s%s%%s%s%s" % (caps,space,space,caps,space,space,caps)
    return fmt % (linfo, rinfo)

def table_splitrows(left, right, align="left", caps = "|", space=" ",
                   center_filler=" ", width=79):
    """return a list of split rows"""
    buf = [ ]
    for i in range(max(len(left), len(right))):
        lstr = ""
        rstr = ""
        if i < len(left):
            lstr = left[i]
        if i < len(right):
            rstr = right[i]
        buf.append(table_splitrow(lstr, rstr, align, caps, space, center_filler,
                                  width))
    return buf

def meter(val,char="{p#",empty=" ",lcap="[",rcap="]",align="left",width=20):
    """Return a horizontal meter representing a numeric value ranging between
       [0,1]."""
    width   = width-len(lcap)-len(rcap) + (lcap.count("{")+rcap.count("{"))*2
    hatches = int(math.floor(width*abs(val)))
    hatches = min(hatches, width)
    left    = ""
    right   = ""
    
    # are we dealing with a backwards meter?
    left  = "".join([char for v in range(hatches)])
    right = "".join([empty for v in range(width-hatches)])
    if align == "right":
        left, right = right, left
    return lcap + "{n" + left + right + "{n" + rcap + "{n"

def pagedlist(category_map, order=None, header=None, height=21):
    """Display lists of information as flips within a book. category_map is a
       mapping between section headers and lists of entries to display for that
       category. If you are only displaying one category, have a map from
       the section header, Topics, to your list of entries. If the categories
       should be displayed in a specific (or partially specific) order, that
       can be specified. Header is text that can appear at front of the book
       display.
    """
    buf = [ ]
    
    # split our header into rows if we have one
    hrows = [ ]
    if header != None:
        hrows = mud.format_string(header, False, 76).strip().split("\r\n")

    # build our full list of orderings
    if order == None:
        order = [ ]
    for category in category_map.iterkeys():
        if not category in order:
            order.append(category)

    # build our page entries. This includes categories and category items
    entries = [ ]
    for category in order:
        if not category in category_map:
            continue

        # add a space between categories
        if len(entries) > 0:
            entries.append("")
        entries.append(category.capitalize())
        for item in category_map[category]:
            entries.append("  %s" % item)

    # append our header if we have one
    if len(hrows) > 0:
        buf.append(table_border)
        for hrow in hrows:
            buf.append(table_row(hrow))

    # build our page contents, one page at a time, until we are out of entries
    pages    = [ ]
    last_cat = None
    while len(entries) > 0:
        page = [ ]
        plen = height - 2 # minus 2 for the borders

        # we're still on the first flip of the book; header is displayed
        if len(pages) <= 2:
            plen -= len(hrows) + 1 # plus 1 for the row above it

        # add items to the page until we are full
        while len(entries) > 0 and len(page) < plen:
            entry = entries.pop(0)

            # is this a blank row, and are we at the head of the page?
            if entry == "" and len(page) == 0:
                continue
            
            # is this a category header?
            if not entry.startswith(" "):
                last_cat = entry

            # are we continuing an old category?
            if entry.startswith(" ") and len(page)==0 and last_cat != None:
                page.append("%s (cont.)" % last_cat)
            page.append(entry)

        # did we have anything added to it?
        if len(page) > 0:
            pages.append(page)

    # take our pages by twos and turn them into table rows
    i = 0
    while i < len(pages):
        page1 = pages[i]
        page2 = [ ]
        if i+1 < len(pages):
            page2 = pages[i+1]

        # append the rows and page contents
        buf.append(table_border)
        buf.extend(table_splitrows(page1, page2))
        buf.append(table_border)
        i += 2
    
    buf.append("")
    return buf

# shortcut table elements
table_border = table_row("", "center", "+", "", "-")
table_filler = table_row("")
seperator    = table_row("", "center", "-", "", "-")

