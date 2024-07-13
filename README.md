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

**com buffer:** One difference from other basket queue operations is that each process will provide a communication slot for new data. The slot will retain an atomic flag indicating if the data has been consumed. Each process will queue their own new entries based on the slot. A consumer of the slots will coalesce the slots into a basket. Later consumption of the queue will rely on the basket grouping for LRU eviction.

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




## Hash Map Interface

The hash map interface, provides the implementation outline for the customized hop-scotch hash used in the loopup implementation.

```c++

class HMap_interface {
	public:
		virtual void 		value_restore_runner(uint8_t slice_for_thread, uint8_t assigned_thread_id) = 0;
		virtual void		cropper_runner(uint8_t slice_for_thread, uint8_t assigned_thread_id) = 0;

		virtual void		random_generator_thread_runner(void) = 0;
		virtual void		set_random_bits(void *shared_bit_region) = 0;

		virtual uint64_t	update(uint32_t el_match_key, uint32_t hash_bucket, uint32_t v_value) = 0;

		virtual uint32_t	get(uint64_t augemented_hash) = 0;
		virtual uint32_t	get(uint32_t el_match_key,uint32_t hash_bucket) = 0;

		virtual uint32_t	del(uint64_t augemented_hash) = 0;
		virtual uint32_t	del(uint32_t el_match_key,uint32_t hash_bucket) = 0;

		virtual void		clear(void) = 0;
		//
		virtual bool		prepare_for_add_key_value_known_refs(atomic<uint32_t> **control_bits_ref,uint32_t h_bucket,uint8_t &which_table,uint32_t &cbits,uint32_t &cbits_op,uint32_t &cbits_base_ops,hh_element **bucket_ref,hh_element **buffer_ref,hh_element **end_buffer_ref,CBIT_stash_holder *cbit_stashes[4]) = 0;
		virtual void		add_key_value_known_refs(atomic<uint32_t> *control_bits,uint32_t el_key,uint32_t h_bucket,uint32_t offset_value,uint8_t which_table,uint32_t cbits,uint32_t cbits_op,uint32_t cbits_base_ops,hh_element *bucket,hh_element *buffer,hh_element *end_buffer,CBIT_stash_holder *cbit_stashes[4]) = 0;
};


```

### Method parameter pattern

Take note of the parameter pattern. Each method identifies the data access element as by its element match key `el_match_key` or hash key. A second parameter,  `hash_bucket` is also passed and may carry augmenting information. A third parameter is a thread id, which has use in the management of atomic contention.

The first parameter, the hash key is a 32 bit hash, which is to be chosen by the application. For best results, it should have a low collision possibility. But, the key may represent a hash space that is much larger that the table (the usual hash table game). So, the methods expect that application to supply some approximation to the bucket.

The second parameter should be the application's idea of the bucket number as derived from the hash key. This second parameter `hash_bucket` should be augemented except in the case of the element being added. The `hash_bucket` must not be augmented when passed to `add_key_value` and it must be augmented otherwise.

Some code may use a 64 bit value to carry both the 32 bit hash and the augmented bucket index. Or the application may consistently keep these separate. For instance, in a node.js application, integers passed into modules can be expected to be 32 bits. While c++ applications may deal easily with 64 bit words. (This situation may change, but it is illustrative of the way the full 64 bits may be passed around.) The methods use two 32 bit words to accomodate the more general use case, but 64 bit single word overloads of the methods are supplied.

#### Augmentation

What does it mean for the hash bucket to be augmented?

A single 32 bit word is allocate to the hash bucket, but the maximum number of buckets in a tier has to fit within memory on smaller computers. Furthermore, it is unlikely that more than 2^28 elements will be stored in a single tier. So, the top four bits of the the bucket index may be used to indicate a slice of memory (shard) in which parallel hash tables may reside. Four bits are set aside by this implementation; yet, maybe only two will be neeed. The top two bits may have values **0**,**1** or **3**. The **0** value indicates that the entry has yet to be added to the table (so methods will reject using it if the bit is not set.) The **1** value indicates that the entry has been store and that it is to be found in the first sharded table (**0** table). The **3** value indicates that the entry has been stored and has is to be found in the second shard (**1** table).


## The Com Buffer

The communication buffer is managed by *TierAndProcManager* class.  The constructor for this class
takes a reference to the memory area (preferably shared) where the communication buffer resides. The class constructor 
initializes the com buffer area, and that is the last step in its construction. If the com buffer area cannot be 
appropriately initialized, the constructor will set the TierAndProcManager status to **false**, but it does not throw an exception.

One criterion for using the com buffer correctly is that the application allocates the right size for the buffer. The size is not enforced by the constructor parameter. Instead, the application can refer to the static emthod `check_expected_com_region_size`. The region size method will return the minimum size for the com buffer area dependent on the number of writer processes using the buffer and the number of tiers that each process may access.

The `check_expected_com_region_size` includes room for a layout of the com buffer that includes the following in order:

* A constant number of w/r flag pairs X the number of tiers  (i.e. one list of pairs per tier in sequence grouped by tier).
* A sequence of `Com_element` entries for each proc grouped by proc. 
	* For each proc, successive `Com_element`s are stored contiguously each indexed by the tier.


There is one of these per reader/writer process per tier:

```
typedef struct COM_ELEMENT {
	atomic<COM_BUFFER_STATE>	_marker;	// control over accessing the message buffer
	uint8_t						_proc;		// the index of the owning proc
	uint8_t						_tier;		// the index of the managed tier (this com element)
	uint8_t						_ops;		// read/write/delete and other flags. 
	//
	uint64_t					_hash;		// the hash of the message
	uint32_t					_offset;	// offset to the LRU element... where data will be stored
	uint32_t					_timestamp;	// control over accessing the message buffer
	char						_message[MAX_MESSAGE_SIZE];   // 64*2
} Com_element;

```


The `Com_element` provides parameters for managing data exchange between consumer facing processes and backend operation. The state of read/write sharing is managed by the `_marker`. The `_message` buffer is provided for input and output operations if needed. (In truth, appropriate LRU classes provided offsets to the data storage which the  *TierAndProcManager* owning processe
will have exclusively until `_marker` indicates freedom.)



## Internals 

The method by method documentation is included in the internals file in the doc directory.

The internals documentation includes a description of the multi-thread operations of the main modules:

* `node_tiers_and_procs`
* `node_shm_LRU`
* `node_shm_HH`

Follow this link:

[Internals Doc](./doc/internals.md)

