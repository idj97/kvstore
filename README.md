Key value database (Work in progress!)

Toy key value database backed by clustered b+ tree index implementation. B+ tree is backed by ordered slotted page implementation for efficient handling of variable length keys and data. 
Slotted page follows NSM (N-ary storage model) and supports inter-page dynamic memory allocation. Dynamic allocation incurs extra memory and storage overhead used to track chunks of free space  but, in theory, allows more efficient inserts, updates, and deletes.  
Additionally, slotted pages using dynamic allocation suffer from fragmentation so to mitigate it there is defragmentation operation.  

setup:
- gcc 11.4.0
- make
- unity test library
- valgrind
