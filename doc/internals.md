# Internals 



## Operational Goals




## submodules


### `shm_tiers_and_procs` -> API facing mutli-threaded application of the shared LRU tiers

API facing mutli-threaded application of the shared LRU tiers.



### `shm_LRU` -> Shared Memory LRU KV Store

Shared Memory LRU object management table applying `shm_HH` and timing buffers.

Each instance of this object is a single tier for the application facing class, `shm_tiers_and_procs`.



### `shm_HH` -> Shared Memory Hopscotch Variant


#### Definitions

**key-value**: The table stores key-value pairs along with control words. The term **key-value** will be used to refer to the stored item, being its key and value, and this will be used in a subjective sense. So, the writing below will say for instance that a key-value is being stored, or that a key-value is being searched, etc.

The storage area for key-value pairs is arranged as four 32 bit words per cell, where a cell contains a key-value along with control word bits per key-value pair. Two words are used for key and value, while two more words are used for bucket membership and memory allocation management. The key and value are not likely stored in adjacent memory locations, the key and the membership bits will be stored in one 64 bit word, while the value and a memory allocation map will be stored in another 64 bit word. The 64 bit words will be part of one 128 bit cell, the whole being 16 bytes.

A bucket is a cell which may be a base or a member bucket. The base bucket holds a single key-value and uses it control bits to control up to 31 more members as member buckets belong to the base bucket.

**input hash**: When a new key-value is added to the table, the hash key being input is provided as a 64-bit word containing two 32-bit words, high and low, with the high word being the full 32-bit hash returned by the has function taking the text representation of an object being stored. The low word is the modulus of the 32-bit hash taken against the length of the table. The lower word is expected to leave its two MSB bits clear for augentation.

**augmented hash**: An augmented hash is an input hash that has its MSB set and its second MSB will be the index of a slice, 0 or 1.

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

**details**

* The application calls `prepare_for_add_key_value_known_refs`
    * `prepare_for_add_key_value_known_refs` calls `_get_member_bits_slice_info`
* The application calls `add_key_value_known_refs`
    * `add_key_value_known_refs` calls `_first_level_bucket_ops`

`_get_member_bits_slice_info` stashes the real **cbits+** of the bucket determined by the hash and returns an atomic reference to the bucket control bits **cbits**. `_get_member_bits_slice_info` uses or constructs operation **cbits** contain an index to the *cbits* stash object containing the real **cbits** of the bucket.

The bucket is empty. So, this will be the first time the **cbits** will be stashed. The operational **cbits** will force a collision to be a follower of the thread that creates the stash. The real **cbits** will be zero. `_first_level_bucket_ops` attempts to give bucket mastership to the calling thread by setting `ROOT_EDIT_CBIT_SET` in the operational **cbits**. This case has no contention, so the bits will be set and the thread will continue.

As bucket master the thread stores the key with operative **cbits** in the cell (one atomic store operation). It then stores the value with **tbits** set to one. It sets the real **cbits** = 0x1 in the stash. Afte that it queues the bucket to the completion service, passing a real **cbits** = 0x1.

A completion thread will dequeue the operational data for the bucket in `value_restore_runner` and use it to call `complete_bucket_creation` under the `HH_FROM_EMPTY` case. The completion involves constructing **tbits** based on the **tbits** of other base elements within the window view of the new base. 

Without races to place an element in the bucket, the queue count for the stash will be zero and the thread will release the bucket from the master's hold. Note that the last thread inserting a value using the same stashed element used for the base creation may release the master's hold and does not have to be the master. Only, the reference count of queued work has to go to zero. The method `release_bucket_state_master` calls `_unstash_base_cbits`. The stash object is returned to the free list and the operation is complete.

**notes**

* The calling interface will have returned from its operation after inserting the new key-value and object data into the LRU component. The LRU component will have returned to finding work after calling `wakeup_value_restore` which enqueues the key-value for completion.


#### (2) Add Collision with Space

> Operation: The slice selection phase routes the hash to a cell that already has members. The cell **cbits** are stashed or the stash reference count is incremented at which point the cell becomes operational. Threads attempt to gain control of the bucket as master, forcing other threads to delay insertion via queue serialization. During the writing phase performed by the bucket master, the existing base key-value pair is usurped and stashed with the new key-value pair written to the base. In the completion phase, the stashed key-value pair is reinserted by reserving a position from among the zero **tbits**. A zero bit in the **tbits** indicates an unclaimed bucket. The reservation puts the key-value into the empty bucket and marks the **cbits** and **tbits** precluding other threads from reserving the same spot. After the reservation, the **tbits** of all bases within the window of the base receive the bit update marking the position. The base **cbits** are also updated with the new bit postion.


**details**

* The application calls `prepare_for_add_key_value_known_refs`
    * `prepare_for_add_key_value_known_refs` calls `_get_member_bits_slice_info`
* The application calls `add_key_value_known_refs`
    * `add_key_value_known_refs` calls `_first_level_bucket_ops`

`_get_member_bits_slice_info` stashes the real **cbits+** of the bucket determined by the hash and returns an atomic reference to the bucket control bits **cbits**. `_get_member_bits_slice_info` uses or constructs operation **cbits** contain an index to the *cbits* stash object containing the real **cbits** of the bucket.

The bucket is not empty, and if the **cbits** are not stashed, the **cbits** loaded from the base bucket cell will be the real **cbits**. The first thread to the base bucket cell will stash the **cbits**, followers will increment the reference count of the stash object.

`_first_level_bucket_ops` attempts to give bucket mastership to the calling thread by setting `ROOT_EDIT_CBIT_SET` in the operational **cbits**. When the adding a collision, the bucket master also or's in the bit `EDITOR_CBIT_SET`, which serves to alert search operations to look in the stash for an element which may bave been the base element. Both fast and swappy searches may check on base replacements if their searches are unfruitful.

Threads that do not attain the bucket mastership will queue their insertion behind the base reinsertion and skip the usurpation of the base.

The thread that attains bucket mastership replaces the base key and value with the new key-value. But, it stashes the old key-value and queues the work of reinserting the key-value to a completion thread. A thread running `value_restore_runner` will dequeue this work and run it under the case `HH_FROM_BASE_AND_WAIT`.  In this case, the thread attempts to reserve a new positino in the array for the dequeued key-value by calling `handle_full_bucket_by_usurp`. A successful attempt results in the addition of bit to the base real **cbits** with the key-value occupying the cell whose offset from the base is the same as the offset of the new bit in **cbits** from zero. The **tbits** are also changed by updating all base **tbits** in the window range around the new element.


#### (3) Add Collision to Full

> Operation: During a process of reinsertion, the resrevation method may discover that all bits within its window have been taken. When all the positions are taken by all the memberships of all the bases within a window taken from the collision base of insertion, the base **tbits** will all be set to one. The reinsertion abandons reseving a postion and then moves into a mode for searching for an oldest element within the window of the base. The oldest member found among the bases within the view of the window will be usurped. If the element is from the collision base of the new key-value pair, then the stashed key-value pair will be written to the discovered position and the old value will be shunted to another tier or seconary storage. If the usurped oldest element is from a base different from the collision base, the usurped element will be queued for reinsertion back into its base bucket.

**details**

* The application calls `prepare_for_add_key_value_known_refs`
    * `prepare_for_add_key_value_known_refs` calls `_get_member_bits_slice_info`
* The application calls `add_key_value_known_refs`
    * `add_key_value_known_refs` calls `_first_level_bucket_ops`

`_get_member_bits_slice_info` stashes the real **cbits+** of the bucket determined by the hash and returns an atomic reference to the bucket control bits **cbits**. `_get_member_bits_slice_info` uses or constructs operation **cbits** contain an index to the *cbits* stash object containing the real **cbits** of the bucket.

The bucket is not empty, and if the **cbits** are not stashed, the **cbits** loaded from the base bucket cell will be the real **cbits**. The first thread to the base bucket cell will stash the **cbits**, followers will increment the reference count of the stash object.

`_first_level_bucket_ops` attempts to give bucket mastership to the calling thread by setting `ROOT_EDIT_CBIT_SET` in the operational **cbits**. When the adding a collision, the bucket master also or's in the bit `EDITOR_CBIT_SET`, which serves to alert search operations to look in the stash for an element which may bave been the base element. Both fast and swappy searches may check on base replacements if their searches are unfruitful.

Threads that do not attain the bucket mastership will queue their insertion behind the base reinsertion and skip the usurpation of the base.

The thread that attains bucket mastership replaces the base key and value with the new key-value. But, it stashes the old key-value and queues the work of reinserting the key-value to a completion thread. A thread running `value_restore_runner` will dequeue this work and run it under the case `HH_FROM_BASE_AND_WAIT`.  In this case, the thread attempts to reserve a new positino in the array for the dequeued key-value by calling `handle_full_bucket_by_usurp`.

A failed attempt routs the thread into a search for an element that may be aged out, an oldest element, that may be moved to another tier or that may be reinserted back into its base that it came from. If the oldest element in the window belongs to another base than the collision base, then the other base, the yielding base, will be returned from a call to `handle_full_bucket_by_usurp` for handling in `value_restore_runner`. The usurped element will be removed from the yielding base and the new key-value will be put in its place. Consequently, the **cbits** of both the collision base and the yielding base will be updated, yet **tbits** will not have to change. That is, the collision **cbits** will gain a bit and the yielding **cbits** will lose a bit. The usurped element will be queued by a call to `wakeup_value_restore` for reinsertion into the yielding base, which may (or not) have a free position in its window above the collision base window.

This reinsertion of the usurped key-value into the yielding base sets in motion an operation similar to the hopscotch free space movement, but differs in that the reinsertion cascades from bucket to bucket until either a yielding bucket has a space beyond the window of the usurper, or until a usurped key-value is sent on to storage handling older key-value entries.

#### (4) Add Collision and Empty in the Same Cycle

> Operation: In this case, two threads obtain the same empty cell in the same slice as their point of insertion at the same time. One of the threads, the earlier one, will operate in the mode of *Add Element to Empty*. The later thread will operate in the mode of *Add Collision with Space*. The later thread may take on the role of bucket master and usurp the previously added element placed at the base by the ealier thread. But, the other more likely path will be that the later element will be queued for insertion as the stashed element in the same thread queue as the earlier element.

**details**

* The application calls `prepare_for_add_key_value_known_refs`
    * `prepare_for_add_key_value_known_refs` calls `_get_member_bits_slice_info`
* The application calls `add_key_value_known_refs`
    * `add_key_value_known_refs` calls `_first_level_bucket_ops`

`_get_member_bits_slice_info` stashes the real **cbits+** of the bucket determined by the hash and returns an atomic reference to the bucket control bits **cbits**. `_get_member_bits_slice_info` uses or constructs operation **cbits** contain an index to the *cbits* stash object containing the real **cbits** of the bucket.

The bucket is empty. So, this will be the first time the **cbits** will be stashed. The operational **cbits** will force a collision to be a follower of the thread that creates the stash. The real **cbits** will be zero. `_first_level_bucket_ops` attempts to give bucket mastership to the calling thread by setting `ROOT_EDIT_CBIT_SET` in the operational **cbits**.

A thread that tries and fails to set `ROOT_EDIT_CBIT_SET` will cycle and try again, but will find that empty bucket has become a base into which it must insert the nex key-value.

The thread that becomes bucket master will store the key with operative **cbits**, then it will store the value with **tbits** set to one. It sets the real **cbits** = 0x1 in the stash. Afte that it queues the bucket to the completion service, passing a real **cbits** = 0x1.

Threads losing the race to be the bucket master will use the operative **cbits** in order to idenitfy the stash record for the cell. Among other things, the stash record retains an index for the completion queue already passing the work from the bucket master to completion. The following threads colliding at the bucket will sequence their work on the same queue.

A completion thread will dequeue the operational data for the bucket in `value_restore_runner` and use it to call `complete_bucket_creation` under the `HH_FROM_EMPTY` case. The completion involves constructing **tbits** based on the **tbits** of other base elements within the window view of the new base. 

If no races to place an element in the bucket occurred, the queue count for the stash will be zero and the bucket may be released from the master's hold. Note that the last thread inserting a value using the same stashed element used for the base creation may release the master hold and does not have to be the master. Only, the reference count of queued work has to go to zero. The method `release_bucket_state_master` calls `_unstash_base_cbits`.


**notes:** 

* The likelihood of this case is low given that three hash collisions have to occur almost simultaneoulsy. The first to collisions will be relegated to separate slices. The third collision will occur after either earlier thread by random selection.

* This is strictly a case of race conditions, meaning that these threads will interact within once cycle of clearing a communication buffer by multiple threads.


#### (5) Add to Empty and Get in the Same Cycle

> Operation: If a `get` operation is earlier than the insertion, then it will fail. Otherwise, once the earlier `add` operation has stashed the bucket and marked the bucket for insertion, the later `get` thread may check the key-value as soon as it can be seen. Several conditions can indicate the presence of the key-value: 1) the key is other than zero; 2) The real **cbits** will be equal to **1**; 3) operational **cbits** indicate that newely created base bucket is undergoing the process of completion.

**details** 

The `add` operation runs the process in ***(1) Add Element to Empty***. 

`get` identifies the slice and the bucket it is searching by unloading the slice indicator from the key that is passed to it. This key should be an ***augmented hash*** (see definitions). `get` cannot proceed if the hash key is not augmented.

With low probability, `get` can be invoked with an ***augmented hash*** returned to the application prior to the completion of the `add` operation still in the process of performing `_first_level_bucket_ops`.

For regular `get` operations see *Get Multiple without other Operations*. 

In this case, `get` can only proceed if it can stash **tbits** and create a reader semaphore. But, in the case of `FROM_EMPTY` for the bucket mastship of the adding thread, the `get` will wait until the mastership is released. When the mastership is released, the **tbits** map will have been completed. 

**notes**

* there is opportunity to provide reading without waiting the release of mastership.




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


**details**

`get(uint32_t el_key, uint32_t h_bucket)` calls `selector_bit_is_set(h_bucket,selector)` where, `selector` is the index to the slice. Note that the `selector` is determined by `_get_member_bits_slice_info`.

Next, this operation checks to see if the cell is empty, and if not, it loads **cbits**, real and operational. The operational **cbits** will be zero if no operations is in progress.

Next, the thread continues with the call to `_get_bucket_reference`. The first thing `_get_bucket_reference` is to stash **tbits** with a call to `tbits_add_reader`. If a *swappy* operation is in progress, it will not be blocked from futher operation; however, future ones will while the **tbits** are swapped. `get` operations that follow the first thread that stashes the **tbits** will necessarily increment the operation **tbits** semaphore and will proceed without interference.

There is a chance that the reader will wait provided that there is an operation that set `ROOT_EDIT_CBIT_SET`. The operation will be either a `del` or `add`, specifically an `add` that is `FROM_BASE` or `FROM_EMPTY`.

Successful searches return a mobility locked reference to the element. Each searcher makes a call to `remobilize`. `remobilize` unlocks the mobility of the element if the element is a member. But, this unlocking is possible only if the a ***mobilization semaphore*** has become zero. The ***mobilization semaphore*** occupies the same operational **cbits** position as the thread queue reference count set up in the base **cbits**.


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

