Key value database (Work in progress!)

Toy key value database backed by clustered b+ tree index implementation. BTree is backed by ordered slotted page implementation for efficient handling of variable length keys and data. 
Implementation of follows NSM (N-ary storage model) and supports inter-page dynamic memory within the page. Inter-page dynamic memory allocation incurs extra memory and storage overhead used for free space tracking but, in theory, allows more efficient inserts, updates, and deletes.  
Slotted pages using dynamic memory allocation suffer from fragmentation so defragmentation is implemented to mitigate it, improve overall performance of page operations and delay page split operation.  

setup:
- gcc 11.4.0
- make
- unity test library
- valgrind
