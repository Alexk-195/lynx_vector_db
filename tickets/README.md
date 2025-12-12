# File-Based Ticketing System

## Overview

This directory contains a file-based ticketing system for tracking tasks, issues, and features. The system is designed to be simple, git-friendly, and easy for AI agents to manage.

## Directory Structure

```
tickets/
├── README.md              # This file
├── ticket_template.md     # Template for new tickets
├── 1030_ticket.md         # Open ticket (example)
├── 1040_ticket.md         # Open ticket (example)
├── 1030_ticket_data.json  # Additional files for ticket 1030 (example)
└── done/
    ├── 1000_ticket.md         # Completed ticket (moved from tickets/)
    ├── 1000_ticket_result.md  # Result summary for ticket 1000
    └── 1000_ticket_notes.txt  # Additional files (moved from tickets/)
```

**Note**: When a ticket is completed, it is **moved** (not copied) to the `done/` subfolder along with all its related files.

## Ticket Numbering

- **Starting number**: 1000
- **Main tickets**: Use numbers in steps of 10 (1000, 1010, 1020, 1030, ...)
- **Sub-tickets**: Use numbers ending in 1, 2, 3, etc. for breaking down larger tickets
  - Example: Ticket 1020 breaks down into 1021, 1022, 1023, ...
  - Or: Ticket 1010 breaks down into 1011, 1012, 1013, ...
- **Numbering does not need to be continuous** - gaps are fine

### Numbering Examples

```
1000 - Main feature: Implement user authentication
  1001 - Sub-task: Design database schema
  1002 - Sub-task: Implement login API
  1003 - Sub-task: Add password hashing

1010 - Main feature: Add payment processing
  1011 - Sub-task: Integrate payment gateway
  1012 - Sub-task: Implement webhooks
  1013 - Sub-task: Add transaction logging

1020 - Bug fix: Memory leak in background worker

1030 - Documentation: Update API reference
```

## Ticket Lifecycle

### 1. Creating a Ticket

Create a new file in the `tickets/` directory using the template:

```bash
cp tickets/ticket_template.md tickets/1000_ticket.md
```

Then edit `tickets/1000_ticket.md` with your ticket details. See [ticket_template.md](ticket_template.md) for the standard template format.

### 2. Working on a Ticket

While working on a ticket, you can:
- Update the ticket file with progress notes
- Create additional files with the ticket prefix: `1000_ticket_*.ext`
- Create sub-tickets if needed (1001, 1002, etc.)

### 3. Completing a Ticket

When a ticket is completed:

1. **Move the ticket** to `tickets/done/`:
   ```bash
   mv tickets/1000_ticket.md tickets/done/
   ```

2. **Create a result summary** in `tickets/done/1000_ticket_result.md`:

```markdown
# Ticket 1000 Result: [Title]

**Completed**: YYYY-MM-DD
**Resolved by**: [Name]

## Summary

[Brief summary of what was accomplished]

## Changes Made

- Change 1
- Change 2
- Change 3

## Commits

- [hash] Commit message 1
- [hash] Commit message 2

## Testing

[How the changes were tested]

## Notes

[Any additional context, lessons learned, or future considerations]
```

3. **Move any additional files** to `tickets/done/`:
   ```bash
   mv tickets/1000_ticket_* tickets/done/
   ```

## Additional Files

All files related to a ticket should be prefixed with the ticket number:

```
1000_ticket.md           # Main ticket file
1000_ticket_design.md    # Design document
1000_ticket_data.json    # Test data
1000_ticket_notes.txt    # Additional notes
1000_ticket_diagram.png  # Diagrams or images
```

This applies to both open tickets (in `tickets/`) and completed tickets (in `tickets/done/`).

## Future Categories

If additional ticket categories are needed in the future (e.g., bugs, features, research), create subfolders under `tickets/`:

```
tickets/
├── bugs/
│   └── 1000_ticket.md
├── features/
│   └── 1010_ticket.md
├── research/
│   └── 1020_ticket.md
└── done/
```

## AI Agent Usage

This ticketing system is designed to be easily managed by AI agents:

- **Creating tickets**: Agents can create tickets for tracking work
- **Breaking down work**: Large tickets can be split into sub-tickets
- **Tracking progress**: Agents can update tickets with progress notes
- **Completing work**: Agents can move tickets to done/ and create result summaries
- **Git-friendly**: All changes are tracked in version control

## Ticket Queries

To find tickets:

```bash
# List all open tickets
ls tickets/*ticket.md

# List completed tickets
ls tickets/done/*ticket.md

# Search for tickets by keyword
grep -r "keyword" tickets/

# Count open tickets
ls tickets/*ticket.md | wc -l

# Count completed tickets
ls tickets/done/*ticket.md | wc -l
```

## Best Practices

1. **Be descriptive**: Use clear, concise titles for tickets
2. **Link related tickets**: Use ticket numbers to reference related work
3. **Update regularly**: Keep tickets up-to-date with current status
4. **Write good summaries**: Result files should clearly explain what was done
5. **Clean up**: Move completed tickets and all their files to done/
6. **Use sub-tickets**: Break complex work into manageable pieces
7. **Skip numbers**: Leave gaps in numbering for future insertions
