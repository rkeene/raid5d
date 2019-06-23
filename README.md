RAID5d
======

Simple RAID5 userspace driver.  It listens on a given TCP port for a connection
from the Network Block Device client and creates a NBD block device.

Components of the RAID5 array are specified after the TCP port number, in the
correct order.  MISSING may be used to indicate a particular component is missing.

Example
-------

      # ./raid5d 6149 /dev/sdc /dev/sdl /dev/sdf MISSING /dev/sde /dev/sdi /dev/sdd /dev/sdj /dev/sdb /dev/sdg /dev/sdh
      # nbd-client bs=4096 127.0.0.1 6149 /dev/nbd1
