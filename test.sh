#!/bin/sh 
######################################################################
# test.sh - test the integrity of the unbini and bini tools
# Copyright (C) 2007 Christopher Wellons <ccw129@psu.edu>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of the
# License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.
######################################################################
#
# This bash script will test the tools on a given BINI file (provided
# as the first and only argument). It will convert the BINI file to
# text, back into another BINI file, then back to text. 
#
# The string tables for each BINI file are compared as well as the
# number of secions, entries and values. The text versions are also
# compared.

# Check for arguments
if [ $# -eq 0 ]; then
    echo 1>&2 Usage: $0 FILE
    exit 127
fi

# Return value (assume success)
RET_VAL=0

# So we don't have to use "./" on everything
PATH=$PATH:.

# Test each argument
while [ $# -ge 1 ]; do

echo Checking $1

# BINI -> text
unbini -qs -o temp1.ini.txt $1 > temp1_sum
unbini -qtn $1 | sort > temp1_tab

# text -> BINI
bini -q temp1.ini.txt

# BINI -> text
unbini -qs -o temp2.ini.txt temp1.ini > temp2_sum
unbini -qtn temp1.ini | sort > temp2_tab

# Grab file sizes
ls -l $1        | awk '{print $5}' > temp1_size
ls -l temp1.ini | awk '{print $5}' > temp2_size

# Compare summary info
if ! diff temp1_sum temp2_sum &> /dev/null; then
    echo Summary diff : $1
    diff temp1_sum temp2_sum
    RET_VAL=-1
fi

# Compare string tables
if ! diff temp1_tab temp2_tab &> /dev/null; then
    echo String table diff : $1
    diff temp1_tab temp2_tab
    RET_VAL=-1
fi

# Compare file sizes
if ! diff temp1_size temp2_size &> /dev/null; then
    echo Size diff : $1
    RET_VAL=-1
fi

# Compare unbini outputs
if ! diff temp1.ini.txt temp2.ini.txt &> /dev/null; then
    echo txt diff : $1
    diff temp1.ini.txt temp2.ini.txt
    RET_VAL=-1
fi

# Remove temporary files
rm temp1_sum
rm temp1_tab
rm temp1_size
rm temp2_sum
rm temp2_tab
rm temp2_size
rm temp1.ini.txt
rm temp1.ini
rm temp2.ini.txt

shift
done

exit $RET_VAL
