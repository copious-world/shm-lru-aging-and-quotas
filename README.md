# shm-lru-aging-and-quotas

Shared memory LRU using aging tiers and quotas against rates of query


## Purpose

This module provide a multi-process shared LRU based on on atomic basket queue access and atomic HopScotch. 

Each LRU-HopScotch pair will correspond to a single tier of usage activity. This module provides the management of presence of a data object in a tier depending on its usage.

If a data object is accessed too aggresively, this module will move the object to a tier of suspects and progressively resist its queries and provide expulsion hints to clients.

Finally, a number of parameters may be configured for tuning the module as the application may require.


## Basic Structure

**queues:** With this stack, a version of the Basket Queue is provided for maintaining the element data area, both the LRU and the free memory. The LRU and free memory reside in the same fixed sized region for a tier. The object storage region is kept separately.

The elements of these lists only store offsets to the object storage regions.  The object storage region does not have to map precisely to a tier LRU; however, this mapping will likely be maintained for simplicity. Suffice it to say that if there is a queue node in free memory, then there is a hole in the object storage where a new element may be stored.

**com buffer:** One difference from other basket queue operations is that each process will provide a communication slot for new data. The slot will retain an atomic flag indicating if the data has been consumed. Each proess will queue their own new entries based on the slot. A consumer of the slots will coalesce the slots into a basket. Later consumption of the queue will rely on the basket grouping for LRU eviction.

**tiers:** Again, each LRU HopScotch pair will correspond to a single tier of usage activity. As the usage of ceratain data objects decrease, they will be evicted from their current activity tier and marshalled to a lower activity tier until they are ready to leave the aegis of the processor, mutlicore CPU.

**quotas:** If a data object is accessed too aggresively, this module will move the object to a tier of suspects and progressively resist its queries. The module will look at the active queue to find an object. If it does not find it their it may inquire other threads about the object from lower activity tables and then finally look in the suspect table. Suspects may find their way back into the active table or may be reported to clients and removed from searching.

**parameters:** Some parameters will be configured at initialization. Such parameters as the number of cores assigned to certain functional roles are among the intialization parameters. Some parameters will be live. Among the parameters changing might actually be the assignment of cores to functional roles. But, most likely, parameters such as activity thresholds per tier may be fluid during the usage of the module.

### <u>Some shared data structures</u>

Some data structures will be kept by each process to maintain efficient and sound access to shared memory regions, mutexes, condition variables, and other shared references. Each process will build up in processes data structures as part of initialization. Later there may be cases where these tables will be updated by all processes when a change is made to shared structures through some sort of administrative activity.

**regions:** Each process will learn of a number of share memory regions during initialization. One process will have the duty of creating these regions. But, those processes that attach will read the shared memory assignments, attach and create an in process map to the structures.

**com buffer:** A shared memory region, the `com_buffer` will be a known and common data structure. Each processes will keep a reference to this table and will not require a lookup in a map (or set or list) to access it. 

Each process will have an entry in the com table. A space in the data structure occupying the entry will be available for adding new entries to the LRU.

**mutexes:** A number of mutexes may be available via the `com_buffer` for cases where high level locking is required.

### <u>Access to the data structures</u>

The data structure usage will reside largely in the module implementation (initially in C++). As this module targets JavaScript, the application code, the client, will work with the more abstract ***set***, ***get***, ***del***, etc. operations provided as synchronous and asynchronous methods.

The clients will be required to call on the initialization of the module in attaching mode or initializing mode. Initialization is required to enable the other methods. Clients will not access the buffers of the module. (Some methods be provided to do some operations directly; but, it will be recommended to avoid the frequent use of those methods.)

Clients will have access to methods for changing configurations. Each change will have to be broadcast to other processes so that they can adjust. In many applications, just one process will have the duty of updating data structures.


## Get going


## Some Refs

* *Balanced Allocations*, Azar et. al. - Reasoning leading to a preferenced for double hashing.
* Two-way Linear Probing Revisited - Again double hashing

* RDCSS - 