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

print("You are AI agent working on a C++ software project. Here are some relevant files and infos:")
dump_file("README.md")
dump_file("CONCEPT.md")
dump_file("tickets/README.md")
dump_file("src/include/lynx/lynx.h")


ticket_files_pat = os.path.join("./tickets", '*_ticket.md')
ticket_files = glob.glob(ticket_files_pat)

if (len(ticket_files) == 0):
    print("*** There are no open tickets.***")
else:
    print("*** Here is first open ticket: ***")
    dump_file(ticket_files[0])
    for i in range(1, len(ticket_files)):
        print("*** Other open tickets: ***")
        print(ticket_files[i])

    



