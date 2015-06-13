# p9fs: A 9P2000.U kernel filesystem driver for FreeBSD.

This implementation is based on the [Plan9 RFC].  It is intended to provide
the both client and server functionality in a kernel module.

p9fs also targets:
* Providing a means of filesystem passthrough for BHyVe virtual machines.
* FreeBSD guests to talk to the PCI device provided by KVM for that purpose.
  This is exported as a VirtIO driver.

[Plan9 RFC]: http://ericvh.github.io/9p-rfc/rfc9p2000.u.html

# Plan9 filesystem specifications

There are three different filesystem specifications in Plan9:
* 9P2000: Original, used by Plan9
* 9P2000.u: Modification of the original for Unix VFSs
* 9P2000.L: Modification of the original specifically for Linux.

This implementation targets 9P2000.u, but if feasible, may also support
9P2000.L, primarily for compatibility purposes.

# Other References

Some other implementations offer useful documentation and tips:
* [py9p] offers a generic implementation in Python.
* The [diod protocol spec] is helpful.
* There is some [v9fs documentation] too.

[py9p]: http://mirtchovski.com/p9/py9p/
[diod protocol spec]: https://github.com/chaos/diod/blob/master/protocol.md
[v9fs documentation]: http://landley.net/kdocs/Documentation/filesystems/9p.txt
