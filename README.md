# atomic_bit_allocator

This repository provides a header-only implementation for a serialized, lock-free bitmap bit allocator.
An example use-case is storing a bitmap in a file, as its serialization allows directly storing and re-reading the bitmap later.
A second example involves writing custom memory managers that use a bitmap to manage equally sized memory chunks.
