# Freelancer BINI Tools (Rewrite)

[Freelancer][wiki] is a 2003 space trading and combat simulation video
game. Much of the game data is stored in "binary INI" or BINI files
using a `.ini` file extension. These tools convert back and forth
between the BINI format and a traditional text-based INI file. The text
INI file can be edited with any normal text editor, then converted back
to the BINI format for the game.

By default, each tool reads from standard input and writes to standard
output.

    $ unbini <market_commodities.ini     >market_commodities.txt.ini
    $ bini   <market_commodities.txt.ini >market_commodities.ini

Alternatively, the input file can be passed as an argument and the
output file chosen with the `-o` switch.

    $ unbini -o market_commodities.txt.ini market_commodities.ini 
    $ bini   -o market_commodities.ini     market_commodities.txt.ini 

These tools can be compiled using *any* ANSI C compiler, including GCC,
Clang, and Visual Studio. On Windows, everything necessary for building
testing, and debugging is available in [w64devkit][w64devkit].

## Text format

Comments begin with a semicolon (`;`) and run to the end of the line.
These are discarded and *not* stored in the BINI format.

Integer and float values are formatted just as you'd expect. Anything
accepted by `strtol()` and `strtod()` is permissible. Otherwise a token
will be treated as a string. To store a numeric-looking value as a
string, it must be wrapped in double quotes.

For example, the `iron` entry below has four values: a float, an
integer, and two strings.

```ini
[Commodities]
iron = 1.42, 300, icons\iron.bmp, "+1"
```

The white space around a string is automatically trimmed. If the string
contains a comma (`,`), newline, double quote, or has significant
leading or trailing white space, then it *must* be wrapped in double
quotes. Quotes within a quoted string are escaped with an additional
quote.

```ini
[Weapons]
red_laser_beam   = 255, 0,   0, "Laser Beam, ""Red"""
green_laser_beam = 0,   255, 0, "Laser Beam, ""Green"""
```

Section and entry names generally do not need quoting. However, if a
section name contains a double quote, newline, brackets (`[`, `]`), or
has significant leading or trailing white space, it must also be quoted.

```ini
["My [Fancy] Title"]
```

Similar, an entry name containing a double-quote, newline, brackets
(`[`, `]`), equal sign (`=`), or that has significant leading or
trailing white space must be quoted.

```ini
"==SPECIAL==" = 1.0
```

## BINI Format

The BINI file format consists of a 12-byte header, a sequence of three
kinds of *packed* structures, and finally a string table. All values are
stored in little-endian byte order.

The header has the magic bytes `"BINI"`, a format version number (1),
and a 32-bit offset into the file for the strings table.

```c
struct bini_header {
    uint32_t magic;    /* 0x494e4942 "BINI" */
    uint32_t version;  /* 0x00000001 */
    uint32_t stroff;   /* file offset to string table */;
};
```

Immediately following the header is each section, e.g. `[section_name]`.

```c
struct bini_section {
    uint16_t name;     /* string table offset */
    uint16_t nentry;   /* number of entries to follow */
};
```

Immediately following the section structure is the given number of
entries, e.g. `key = value0, value1, value2`.

```c
struct bini_entry {
    uint16_t key;      /* string table offset */
    uint8_t nvalue;    /* number of values to follow */
};
```

Finally a type-tagged structure for each value in the entry.

```c
#define VALUE_INTEGER  1
#define VALUE_FLOAT    2
#define VALUE_STRING   3
struct bini_value {
    uint8_t type;
    union {
        int32_t i32;
        float f32;
        uint16_t string;  /* string table offset */
    } value;
};
```

Immediately following the last section is the string table. Strings are
null-terminated and concatenated into an "argz" vector. The string
offsets are byte addresses in this table. Freelancer doesn't have any
particular encoding for these strings.


[w64devkit]: https://github.com/skeeto/w64devkit
[wiki]: https://en.wikipedia.org/wiki/Freelancer_(video_game)
