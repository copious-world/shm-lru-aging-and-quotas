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






# Internals 



## Operational Goals

### `shm_HH` -> Shared Memory Hopscotch Variant


#### Definitions

The storage area for key-value pairs is arranged as four 32 bit words per cell, where cell contains key-value and control word bits per key-value pair. Two words are used for key and value, while two more words are used for bucket membership and memory allocation management. 

A bucket is a cell which may be a base or a member bucket. The base bucket holds a single key-value and uses it control bits to control up to 31 more members as member buckets belong to the base bucket. 

The control bits **cbits** indicate the state of membership of a cell. The base bucket **cbits** provide a membership location map to bucket members, where a bit set high in the ith position indicates a member at offset *i* from the base.

The taken bits **tbits** indicate free and assigned buckets as a map to location from the base. If the ith bit of a base bucket **tbits** is set to zero, then the cell at offset *i* from the base has not been assigned to any base and should be empty. Bits set high in the **tbits** indicate that cell contains a member of the base or some other base within a window whose width is a maximum distance from the base providin the **tbits**.

If a cell is not empty and is not a base, then it is member of some base and is within the window width distance of that base. The **cbits** of the member will contain the offset to the base of which it is a member. The **tbits** of the member will contain some ordering information. 

**Stashing** is an operation that moves either **cbits** or **tbits** to a stash record (taken from a stack of free stash records). The stashing operation finished by writing operation **cbits** or **tbits** into the cell. The operational **cbits** and **tbits**, created during stashing operations, contain flags, semaphores, etc. and must include the index of the stash record for their particular type *cbit* or *tbit*.

Note: **cbits** and **tbits** are not stashed by the same stashing operation, they are of different types. **cbits** and **tbits** may be stashed for a cell during the same time period, but they can separate. The **cbits** are used more for membership and operations that change membership, while **tbits** are used more for blocking out the allocation of cells. **tbits** are used during searching to block memory changes for searches that can run between memory changes and as a result can ignore operational flags that might slow down the search.

**real cbits**: **cbits** that are stashed yield their cell storage to operational **cbits**, but the operational **cbits** will retain an index to the stash record. Some operations need to examine the membership map in the stashed **cbits** or the **cbits** in the cell if they are not stashed. The **cbits** containing the membership map may be refered to as the **real cbits**, stashed or not. **operation cbits** only ever reside in the cell's controll bit word. Methods that use `cbits` and `cbits_op` use the real **cbits** for `cbits` and the operational **cbits** for `cbits_op`. If the **cbits** are not stashed, the `cbits_op` should be zero in methods that use `cbits_op`.

**real tbits**: Similarly to **real cbits**, the real **tbits** are non operational **tbits** or they are the stashed **tbits** during times when the **tbits** must be stashed.

**Immobility**: All base cells are immobile, i.e. the key-value of the cell may not be swapped with another member position. Members may be mobile, but at times are marked as immobile during operations. For instance, a searches result in a found cell being marked **immobile** until the key-value of th cell are fully utilized by the calling methods. Some calling method will require a memory reference to the cell in order to close out operations, e.g. unstash the cell, reduce reference counts, etc. 

#### Adding an Element

There is a three stage process for adding an element. First, a position in one of two memory slices is chosen. Then, the element and its control words are written into the position. In the third phase, operations are completed by service threads which may reinsert usurped elements and will in any case rectify memory storage maps belonging to buckets within the view of the insertion position. The stages are separated in such a way that an application could do the stages strictly sequentially for each element being inserted, or it could batch position selection followed by a batch of value insertions.

Finding a position for an element is a function of its hash index. A bucket for the element will be selected from a slice, 
and that bucket may be *empty*, a *base* for a membership of elements, or a *member* element that will be usurped for the new element. 

The decision to use two slices for partitioning data into a double hash scheme may be fairly pedestrian, but it satisfies the balance of the membership count and includes a random bit generator for breaking ties in the count. 

Once the position for the new element, a key-value pair, has been determined, the next operation will continue with writing a value to the position or writing the value into a bucket member position nearby the postion. Member positions have to be within the number of positions that can be stored in a control word. In this implementation, that distance is 32 with 32 bit control words.

***A bucket may be empty.*** If an bucket is empty, the bucket will become a base bucket. There may be a rare race for by more than one thread to be the one that creates the base. But, just one thread will win that race and other threads will insert into the newly formed bas bucket. Empty buckets an be identified as have zero membership bits or, in other words, their control bits are set to zero. Also, their memory map of *taken bits*, **tbits**, are zero. Optionally, the key and value words of the bucket may also be zero.

***A bucket may be a base.*** If a bucket is a base, it governs one or more members, the `add` operation will usurp the base key value and reinsert the base after claiming a new bit in its **cbits**. The base bucket's control bits **cbits** will indicate either its membership positions taken from the base upward or it may indicate an operational state. All **cbits** indicating membership have their zero bit set to one. Furthermore, **cbits** identify 32 positions in the array, with each one bit being the base plus the position of the bit starting at zero. As memory is interleaved, **cbits** may be a mix of ones and zeros, with ones indicating positions in the array that are members of the base's membership. The zeros belong to other bases or are empty. The zero bit will be set to one for the base itself. If the base is in operation, the zero bit will be zero, also its MSB bit will be zero. If the **cbits** are all zero, the bucket is emtpy. After an `add` operation is complete, following reinsertion of the base, some zero membership bit will be changed to one.

***A bucket may be a member.*** If a bucket is a member, its zero bit will be zero, whether it is operational or not. The top bit, the MSB bit, will be set to one. The MSB bit indicates membership. A non-operational member will store a back reference to its base as an offset to the base. This offset is less than 32, the maximum number of members a bucket can have. If the member bucket is operational, the bucket will have an indcator bit set and its reference number will be an index to a stash of the non-operational values of its control bits. When an element is being added at the position of the member, operational or not, the `add` operations will usurp the member, stealing it way from its base. The new element will become a base at the member position. As an exception to the operational usurpation, if the member is already undergoing the operation of usurpation, the thread colliding with the ongoing operation will give up its attempt to usurp and instead insert into the new base as soon as it becomes available. The usurped member will be used to access the bucket giving up its membership position so that its **cbits** may be changed and the usurped member's key and value will be queued for reinsertion into the changed bucket, the usurped member's base.

A usurped member may be undergoing deletion as its operation. If it is possible that the deletion operation can be stopped, the bucket may be turned into a base without carring out the full process of deletion. A base that is being deleted is cleared almost immediately; and so, an element attempting to insert at that base may wait to perform its insertion. In another case, a deletion of a primary key-value following the addition of a second key-value to a single element base may work to prevent reinsertion of the primary key-value.

In the following, ***elements*** are the key-value pairs. The bucket is empty, a base, or a member.

#### Stashing Cells

Operations effecting a change in the membership of a cell stash the control bits of the cell, marking the cell as operational.



#### States of Operation

1. Add Element to Empty
2. Add Collision with Space
3. Add Collision to Full
4. Add Collision and Empty in the Same Cycle
5. Add to Empty and Get in the Same Cycle
6. Add to Collision and Get in the Same Cycle
7. Add to Collision and Update in the Same Cycle
8. Add to Empty and Delete in the Same Cycle (special case)
9. Add to Collision and Delete Member in the Same Cycle (crop in the same cycle)
10. Add to Collision and Delete Base in the Same Cycle
11. Add by Usurp and Get in the Same Cycle
12. Add by Usurp and Update in the Same Cycle
13. Add by Usurp and Delete in the Same Cycle (special case)
14. Add by Usurp and Delete Base in the Same Cycle (delete immediately destroys add)
15. Get Multiple without other Operations
16. Get One or More with Delete Member in the Same Cycle (crop in the same cycle)
17. Get One or More with Delete Base in the Same Cycle
18. Get One or More with Update in the Same Cycle
19. Collision Update Multiple
20. Delete Multiple Members
21. Delete Base Only
22. Delete Base and Members in the Same Cycle
23. Delete Collision (special case check)
24. Usurp Delete in Progress


#### (1) Add Element to Empty

> Operation: The slice selection phase discovers an empty cell and stashes it. The operation of stashing marks the cell's **cbits** as operational. In the writing phase, one thread assumes control of the bucket as master, forcing other threads to use the position as a collision with member. In the completion phase, the **tbits** map of the new base cell is created for the first time. After that, other base cells within the view of the bucket have their **tbits** updated. 

**notes:** 

* The act of writing to the cell and inserting a key-value pair prevents other buckets from using the **tbits** to reserve a position for another insertions going on within the same window. 


#### (2) Add Collision with Space

> Operation: The slice selection phase routes the hash to a cell that already has members. The cell **cbits** are stashed or the stash reference count is incremented at which point the cell becomes operational. Threads attempt to gain control of the bucket as master, forcing other threads to delay insertion via queue serialization. During the writing phase performed by the bucket master, the existing base key-value pair is usurped and stashed with the new key-value pair written to the base. In the completion phase, the stashed key-value pair is reinserted by reserving a position from among the zero **tbits**. A zero bit in the **tbits** indicates an unclaimed bucket. The reservation puts the key-value into the empty bucket and marks the **cbits** and **tbits** precluding other threads from reserving the same spot. After the reservation, the **tbits** of all bases within the window of the base receive the bit update marking the position. The base **cbits** are also updated with the new bit postion.



#### (3) Add Collision to Full

> Operation: During a process of reinsertion, the resrevation method may discover that all bits within its window have been taken. When all the positions are taken by all the memberships of all the bases within a window taken from the collision base of insertion, the base **tbits** will all be set to one. The reinsertion abandons reseving a postion and then moves into a mode for searching for an oldest element within the window of the base. The oldest member found among the bases within the view of the window will be usurped. If the element is from the collision base of the new key-value pair, then the stashed key-value pair will be written to the discovered position and the old value will be shunted to another tier or seconary storage. If the usurped oldest element is from a base different from the collision base, the usurped element will be queued for reinsertion back into its base bucket. 


#### (4) Add Collision and Empty in the Same Cycle

> Operation: In this case, two threads obtain the same empty cell in the same slice as their point of insertion at the same time. One of the threads, the earlier one, will operate in the mode of *Add Element to Empty*. The later thread will operate in the mode of *Add Collision with Space*. The later thread may take on the role of bucket master and usurp the previously added element placed at the base by the ealier thread. But, the other more likely path will be that the later element will be queued for insertion as the stashed element in the same thread queue as the earlier element.

**notes:** 

* The likelihood of this case is low given that three hash collisions have to occur almost simultaneoulsy. The first to collisions will be relegated to separate slices. The third collision will occur after either earlier thread by random selection.

* This is strictly a case of race conditions, meaning that these threads will interact within once cycle of clearing a communication buffer by multiple threads.


#### (5) Add to Empty and Get in the Same Cycle

> Operation: If a `get` operation is earlier than the insertion, then it will fail. Otherwise, once the earlier `add` operation has stashed the bucket and marked the bucket for insertion, the later `get` thread may check the key-value as soon as it can be seen. Several conditions can indicate the presence of the key-value: 1) the key is other than zero; 2) The real **cbits** will be equal to **1**; 3) operational **cbits** indicate that newely created base bucket is undergoing the process of completion.

#### (6) Add to Collision and Get in the Same Cycle

> Operation: There will be members in the base bucket. The `get` operation search may use the real **cbits** to select cells for key examination. In the case that searching with the real **cbits** don't yield up a matching key, the `get` operation may use the operational **cbits** to check on a stashed key-value. Give a key-value is stashed, the data object it references will still be stored. 

#### (7) Add to Collision and Update in the Same Cycle

> Operation: Updates use the same search as `get`. Update does not stash an element, it mearly overwrites a cell's value and in some cases it may overwrite a cell's key. The update make overwrite key-value in stash. The `add` completion will result reserve a place for whatever value is in stash, unless the key is set to `UINT32_MAX`, indicating deletion.

#### (8) Add to Empty and Delete in the Same Cycle (special case)

> Operation: The delete operation is an update operation in which an key-value element has its key set to `UINT32_MAX`. If the element is found in searching, it will be left marked as mobile and submitted to the cropper. The mobility marking signals to other users of the cell that it is likely to be swapped during its inspection.

#### (9) Add to Collision and Delete Member in the Same Cycle (crop in the same cycle)

> Operation: Cells are not available for writing until they are removed from all the base memory maps, **tbits**, within a window around the cell, which is twice the width of a base's window (32 bits, hencce 64 bits). The cropper swaps discarded elements with elements farthest from the elements' base, thereby compressing a base bucket. Swapping occurs, the **cbits** don't change until the **cbits** at the end are removed. If new key-values are being added while cropping occurs, reservation for new elements will may attempt to interfere with the swap and take over the position or it may attempt to interfere with clearing an already swapped position from the base **cbits**. The cropper must check to see if the key of an element being erased remains `UINT32_MAX`.

#### (10) Add to Collision and Delete Base in the Same Cycle

> Operation: When a base is being deleted, a swap is made with a member, if one exists, and the base and the key-value within the moved to the member position from the base is changed to having a key of `UINT32_MAX`. And, the bucket is submitted to cropping. The `add` operation may attempt to remove the position from its cropping queue and take the cell before the real **cbits** are changed. If that is not possible, the `add` will fail to become the bucket master and queue its new element for position reservation. If the `add` operation can access the base prior to the swap, then it can become the bucket master and prevent the swap and operate in the mode of `Add Collision with Space`, but it will not queue the old key-value for reinsertion.

**notes**

* the delete operation must check that the base key is the same if it has be delayed in becoming the bucket master.
* the delete operation must flag the base as being made ready for delete.
* delete clears the base entirely if there is only one element in the bucket.

#### (11) Add by Usurp and Get in the Same Cycle

> Operation: The search method does not have to operate as if a swappy operation is taking place. The search operation will load key and **cbits** of each member in a 64 bit load. If member bucket is stashed, it is necessarilly being usurped, which means that it will become a base. The search operation may check the key-value in any case until the real **cbits** of the cell indicate the single member to the new bucket.

#### (12) Add by Usurp and Update in the Same Cycle

> Operation: `update` uses the same search as `get`, only, the member **cbits** indicate that the bucket has become immobile and in operation, i.e. stashed. `update` must change the stashed key-value prior to its reinsertion or must wait until after reinsertion. If `update` is ahead of the `add` operation usurpation, then it must increment the reader semaphore in the base bucket operational **cbits** and the `add` operation usurpation must wait until readers are done with the original base bucket of the member. 

#### (13) Add by Usurp and Delete in the Same Cycle (special case)

> Operation: This mode operates in the same manner as *Add by Usurp and Update* except that, if possible, that the key matches the delete key then usurpation may skip the reinsertion of the stashed key value. `del` must find the stashed key and change the key in stash, and `add` by usurpation must check that the key is `UINT32_MAX`.

#### (14) Add by Usurp and Delete Base in the Same Cycle (delete immediately destroys add)

> Operation: if `del` is first, then the key will be set to `UINT32_MAX`. `add` by usurpation may succeed in blocking the submission to cropping. But, even if the element is submitted to cropping, the cropper will not find the element to crop, albeit that it might do unnecessary work. The `add` using the usurpation mode must check to see if the key has changed or if the member **cbtits** indicate special mobility as in the case of `Add by Usurp and Delete`. If `add` is first, then the member should be stashed and the `del` operation must take the stashed key-value and place it in the base.

**notes**

* effectively this means swap the base with the stash.


#### (15) Get Multiple without other Operations

> Operation: Operations that change the structure of the **cbits** will mark the base as being operated upon by a *swappy oepration*. Searches will search checking for swaps. Otherwise, operation that change the structure of the **cbits** will have to wait for readers running in a less safe mode, where these readers have incremented a semaphore (reference count) in the operational **tbits** of the base. The real **tbits** of the base will be stashed while the semaphore is above zero. Once the real **tbits** are *unstashed* (restored), the mutating operations may mark the base *swappy* and procede.

#### (16) Get One or More with Delete Member in the Same Cycle (crop in the same cycle)

> Operation: The delete operation begins with marking the operational **cbits** with a flag indicating that a ***swappy operation*** is about to take place. Similar to *Get Multiple without other Operations*, the ***swappy operation*** flag tells searches to proceed carefully through the bucket using the real **cbits**. The searchers must mark cells that they examine as immobile prior to checking keys and release them prior to examining the remaining keys. The cropper must wait until cells are remobilized before continuing with compressing the bucket. 

#### (17) Get One or More with Delete Base in the Same Cycle

> Operation: The `get` operations may proceed using a *swappy* search path. If the base key, which may be stashed, is the match of their search, they must abandon their search. Otherwise, they must check the mobility of the bucket, and if it is the subject of a swap, it must check the key in the base when it encounters a mobilized bucket whose key is `UINT32_MAX`.

#### (18) Get One or More with Update in the Same Cycle

> Operation: This operation is just another search contender and if no *swappy* operations are in progress on the base bucket, then the update may increment the reader semaphore along with other readers. All readers will decrement their semaphore when they are done using the bucket reference. 

#### (19) Collision Update Multiple

> Operation: If the same key is being updated in value, then the last update operation will determine the value with other threads having no impact. If the key changes, then late threads will fail to find the key. Some application may want to change keys, but for the case of the module only `del` changes the key.

#### (20) Delete Multiple Members

> Operation: Members are not considered to be the base. These are just updates that change the key to `UINT32_MAX`. If more than one member is deleted at a time, then they are all updated as long as their keys remain visible to the ongoing searches.

#### (21) Delete Base Only

> Operation: For a single element bucket, the whole bucket cell must be set to zero and the stashes must be cleared of the element. The **tbits** within the view of the bucket will have the bucket position cleared.

#### (22) Delete Base and Members in the Same Cycle

> Operation: It is left up to the search methods to find the buckets to update with a `UINT32_MAX` key. But, the one special case is that the base cannot swap with buckets whose keys have been set to a `UINT32_MAX`. Finally, searches operating in a *swappy* mode must check the base when they encounter `UINT32_MAX` key. If the base deletion encounters a membership with all positions set to `UINT32_MAX` keys, then the whole base and all of its buckets may be cleared immediately and the base must be removed from a future cropping operation. If the cropping operation is taking place, the base delete must wait.

#### (23) Delete Collision (special case check)

> Operation: If the same key is being deleted, then both keys will attempt to update the same cell with a `UINT32_MAX` key. If they are operating on the base, the threads beyond the first must abondon their effort once they check the state of swappiness and planned mobility.

#### (24) Usurp Delete in Progress

> Operation: See `Add by Usurp and Delete`. The `add` by usurpation process must avoid submitting the stashed key-value to reinsertion.




### `shm_LRU` -> Shared Memory LRU KV Store

Shared Memory LRU object management table applying `shm_HH` and timing buffers.

Each instance of this object is a single tier for the application facing class, `shm_tiers_and_procs`.




### `shm_tiers_and_procs` -> API facing mutli-threaded application of the shared LRU tiers

API facing mutli-threaded application of the shared LRU tiers.

