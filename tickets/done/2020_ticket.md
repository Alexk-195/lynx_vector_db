# Ticket: Add iterator for all records

*Important*: Read README.md and tickets/README.md if not yet done.


**Created**: 2925-12-12
**Assigned**: Claude

## Description

Add to DB interface file lynx.h functions which return iterators to get all records. 

Think of locks, read lock should be hold for iterators life. 

Implement the functions in all database types: flat, hsnw, ivf

Break down ticket if needed. 

Add tests

## Acceptance Criteria

- [ ] all records can be read with iterator. Tests and docu are updated. 



## Notes




