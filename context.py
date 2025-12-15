#!/usr/bin/env python3
import os
import glob

def dump_file(filepath):
    """Read and print the contents of a text file."""
    
    if (os.path.exists(filepath)):  
        print("*** Start of file: ", filepath)
        with open(filepath, 'r', encoding='utf-8') as file:
            print(file.read())
        print("*** End of file: ", filepath)
    else:
        print(f"Provide info to user that the file is not found: {filepath}")

print("You are AI agent working on a C++ software project. Here is your context which contains relevant files and infos:")
dump_file("README.md")
dump_file("tickets/README.md")
#dump_file("src/include/lynx/lynx.h")


ticket_files_pat = os.path.join("./tickets", '*_ticket.md')
ticket_files = sorted(glob.glob(ticket_files_pat))

if (len(ticket_files) == 0):
    print("*** There are no open tickets.***")
else:
    print("*** Here is first open ticket: ***")
    dump_file(ticket_files[0])
    if (len(ticket_files) > 1):
        print("*** Other open tickets: ***")
        for i in range(1, len(ticket_files)):
            print(ticket_files[i])

    
print("*** End of context ***")
print("Reply to user that this info was helpful") 



