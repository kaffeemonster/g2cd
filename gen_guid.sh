#! /bin/bash
#
# gen_guid.sh
#
# little helper script to generate a guid for g2cd
# not nice, just works with bash, but does it job, mostly
#
# Copyright (c) 2004, Jan Seiffert
#
# This file is part of g2cd.
#
# g2cd is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# any later version.
#
# g2cd is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with g2cd; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#
# $Id: gen_guid.sh,v 1.2 2004/12/18 18:06:13 redbully Exp redbully $
#
# set the Language to default
export LANG=C

# do we have the output-file
if [[ -z $1 ]]; then
	echo "Usage: $0 output-file";
	exit;
fi

# search md5sum
# NOTE: if you don't have md5sum, you could use another hashing, if:
#       1) it has 128bit length
#       2) you can make the output look like what we need
PATH_TO_MD5=`which md5sum`
if [[ ! -x $PATH_TO_MD5 ]] ; then
	echo "The program md5sum could not be found.";
	exit;
fi

# see if we could reach ifconfig to obtain MAC-Address
PATH_TO_IFCONFIG=`which ifconfig`

# ifconfig?
if [[ -x $PATH_TO_IFCONFIG ]] ; then
	# OK, we have ifconfig, try to grep the MAC-Addr. and put it to our data
	$PATH_TO_IFCONFIG | grep -E -o -e '([0-9A-Fa-f]{2,2}:){5,5}[0-9A-Fa-f]{2,2}' > /tmp/$0$$;
else
	# no ifconfig, take 17 random bytes
	dd if=/dev/random bs=1 count=17 > /tmp/$0$$;
fi

# fill it up with additional 32+15 bytes of random
dd if=/dev/random bs=1 count=47 >> /tmp/$0$$

# hash it (nobody wants his MAC-addr plain in a GUID) and pretty-print it
cat /tmp/$0$$ | md5sum -b | sed -r -e "s/([0-9A-Fa-f]{2,2})/\1:/g" | sed -r -e "s/: [* ]-$//g" > $1

#remove the tmp-file 
rm -f /tmp/$0$$
