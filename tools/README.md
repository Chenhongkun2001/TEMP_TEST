# Brief sepcification of tools

**Env Setup Tool (environmentSetup.sh)**: To assign the build tool chain (e.g., compiler or linker)
- usage: source environmentSetup.sh

**Code Formatter (uncrustify.cfg)**: All the .c and .h (except auto generated files) should be formatted by uncrustify (using the .cfg)
- usage: uncrustify -c my.cfg --no-backup foo.c
- notice: uncrustify can be obtained at https://github.com/uncrustify/uncrustify

