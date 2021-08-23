# Tag_Service
A linux kernel module that passes messages between processes, a project for the course of Advanced operating systems, a detailed description of the system (in italian) is included in the "Relazione SOA.pdf" file.

## Installation

The module has been designed for kernel 5.8, it should theoretically work for any linux kernel > 4.6 but it is not guaranteed in any way.

To install and use the module download the repository and use the following commands in the root folder as a root user:

```
make
insmod tag_service.ko
```

To remove and clean the compilation files:

```
rmmod tag_service.ko
make clean
```

## Tests

To run the tests, install the module, go into the user folder and run the file "run_test.sh" as a root user. Note that the root permissions are needed only for the "different_owner_test" and tests can be otherwire run with normal user permissions.

ATTENTION: the "table stess test" will spawn around 2000 thread to test hash table resizing, this could be a problem on weaker machines
