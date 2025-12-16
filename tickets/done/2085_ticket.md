# Ticket: Fix test BatchInsertWithWrongDimension

*Important*: Read README.md and tickets/README.md if not yet done.

**Priority**: High

## Description

The batch_insert method of the database should be documented accoring to behaviour:

- If one of the records in batch has mismatching dimension than all records are discarded and error is returned.
- Check this method for particial inserts: they should be avoided. 
- Update all related documentation. 

## Acceptance Criteria

- [ ] Test BatchInsertWithWrongDimension is OK
