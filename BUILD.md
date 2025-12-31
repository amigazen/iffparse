## Building from Source

### Requirements
- SAS/C compiler 6.58
- NDK 3.2 R4
- An Amiga computer

### Build Options

The SCOPTIONS file contains build options for the main iffparse executable.

The default CPU target is 68020. 

Adding
```
define DEBUG
```

to SCOPTIONS will make a build with more console output to help debug parsing problems.

### Build Commands

```
cd Source
smake
smake install
```

## Installation

1. Find the iffparse executable in SDK/C/ in this distribution and copy it to wherever you want to usually run it from

