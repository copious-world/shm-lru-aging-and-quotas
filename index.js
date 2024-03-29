'use strict';
//
//
const shm = require('./build/Release/shm-typed-lru.node');
const { XXHash32 } = require('xxhash32-node-cmake')
const ftok = require('ftok')   // using this to get a shm segment id for the OS.

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
//
const MAX_EVICTS = 10
const MIN_DELTA = 1000*60*60   // millisecs
const INTER_PROC_DESCRIPTOR_WORDS = 8
const DEFAULT_RESIDENCY_TIMEOUT = MIN_DELTA
//

const SUPER_HEADER = 256                // mutex is stored at the bottome of the buffer...
const MAX_LOCK_ATTEMPTS = 3

const WORD_SIZE = 4
const LONG_WORD_SIZE = 8
const HH_HEADER_SIZE = 64

//
const PID_INDEX = 0
const WRITE_FLAG_INDEX = 1
const INFO_INDEX_LRU = 2
const INFO_INDEX_HH = 3
const NUM_INFO_FIELDS = 4

const LRU_HEADER = 64

const DEFAULT_ELEMENT_COUNT = 1024


var g_app_seed = 0
var g_hasher32 = null


function default_hash(data) {
    if ( !(g_hasher32) ) return(0)
    try {
        let hh = g_hasher32.hash(data)
        return hh            
    } catch (e) {
        console.log(e)
    }
    return 0
}


function init_default(seed) {
    g_app_seed = parseInt(seed,16);
    g_hasher32 = new XXHash32(g_app_seed);
    return default_hash
}


// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
//
/**
 * Max length of shared memory segment (count of elements, not bytes)
 */
const lengthMax = shm.NODE_BUFFER_MAX_LENGTH;

const cleanup = () => {
	try {
		var cnt = shm.detachAll();
		if (cnt > 0)
			console.info('shm segments destroyed:', cnt);
	} catch(exc) { console.error(exc); }
};
process.on('exit', cleanup);
let sigint_proc_stop = false
process.on('SIGINT',() => {
	cleanup()
	if ( sigint_proc_stop === true ) process.exit(0)
	if ( typeof sigint_proc_stop === "function" ) sigint_proc_stop()
})

function set_sigint_proc_stop(func) {
	sigint_proc_stop = func
}



/**
 * 
 * 
 */
class ReaderWriter {

    //
    constructor(conf) {
        //
        let common_path = conf.token_path
        //
        this.tier_count = (conf.tiers.length !== undefined) ? parseInt(conf.el_count) : DEFAULT_ELEMENT_COUNT;
        //
        this.shm_com_key = false
        //
        //
        let proc_count = (conf.proc_count !== undefined) ? parseInt(conf.proc_count) : 0
        proc_count = Math.max(proc_count,2)     // expect one attaching process other than initializer (May be just logging)
        this.proc_count = proc_count
        //
        this.shm_com_key = ftok(`${common_path}`)
        //
        this.asset_lock = false
        this.com_buffer = []
        this.nprocs = 0
        this.proc_index = -1
        this.pid = process.pid
        this.resolver = null
        //
    }



    /**
     * 
     * @param {object} conf 
     */
    init_shm_communicator(conf) {
        //
        let sz = INTER_PROC_DESCRIPTOR_WORDS
        let proc_count = this.proc_count
        let NTiers = this.tier_count

        let mpath_match = -1
        if ( (typeof conf.token_path !== "undefined") ) {  // better idea
            this.initializer = conf.am_initializer
            if ( this.initializer === undefined ) this.initializer = false
        }
        //
        if ( this.initializer ) {
            this.com_buffer = shm.create(proc_count*sz*NTiers + SUPER_HEADER*NTiers,'Uint32Array',this.shm_com_key)
        } else {
            this.com_buffer = shm.get(this.shm_com_key,'Uint32Array')
            if ( (mpath_match === -1) && (this.com_buffer === null) ) {
                console.dir(conf)
                console.log("possibly the shared memory region is not initialized")
                throw(new Error("possible configuration error"))
            }
        }

        //
        shm.set_com_buf(this.shm_com_key,SUPER_HEADER,NTiers,INTER_PROC_DESCRIPTOR_WORDS) // 

        const super_header_offset =  SUPER_HEADER*NTiers  // maybe a lock per tier
        //
        for ( let i = 0; i < NTiers; i++ ) {
            //
            this.proc_index = this.initializer ? 0 : 1
            let pid = this.pid
            let p_offset = sz*(this.proc_index)*i + super_header_offset
            this.com_buffer[p_offset + PID_INDEX] = pid
            this.com_buffer[p_offset + WRITE_FLAG_INDEX] = 0
            this.com_buffer[p_offset + INFO_INDEX_LRU] = 0  //??
            this.com_buffer[p_offset + INFO_INDEX_HH] = 0  //??
            //
            // mutex per tier ... but will use on special occasions...
            shm.init_mutex(this.shm_com_key,i,this.initializer)   // put the mutex at the very start of the communicator region.
        }
        //

    }

    
    // ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
    /**
     * 
     * @param {Number} tier 
     * @param {Function} resolve 
     * @param {Function} reject 
     * @param {Number} count 
     */
    try_again(tier,resolve,reject,count) {
        count++
        // try again at least once before stalling on a lock
        setImmediate(() => { 
            let result = shm.try_lock(this.shm_com_key,tier)
            if ( result === true ) {
                resolve(true)
            } else if ( result === false ) {
                if ( count < MAX_LOCK_ATTEMPTS ) {
                    this.try_again(resolve,count)
                }
            } else {
                reject(result)
            }
        })
    }

    /**
     * 
     * @param {Number} tier - index of the tier for accessing the com key
     * @param {Number} count - 
     * @returns {Promise}
     */
    async access(tier,count) {
        if ( count === undefined ) count = 0
        return new Promise((resolve,reject) => {
            this.asset_lock = false
            let result = shm.try_lock(this.shm_com_key,tier)
            if ( result === true ) {
                this.asset_lock = true
                resolve(true)
            } else if ( result === false ) {
                this.try_again(resolve,reject,count)
            } else {
                reject(result)
            }
        })
    }

    /**
     * 
     * @param {Number} tier - index of the tier for accessing the com key
     */
    lock_asset(tier) {
        if ( this.proc_index >= 0 && this.com_buffer.length ) {
            if ( this.asset_lock ) return; // it is already locked
            //
            let result = shm.lock(this.shm_com_key,tier)
            if ( result !== true ) {
                console.log(shm.get_last_mutex_reason(this.shm_com_key,tier))
            }
        }
    }

    /**
     * 
     * @param {Number} tier - index of the tier for accessing the com key
     */
    unlock_asset(tier) {
        if ( this.proc_index >= 0 && this.com_buffer.length ) {
            let result = shm.unlock(this.shm_com_key,tier)
            if ( result !== true ) {
                console.log(shm.get_last_mutex_reason(this.shm_com_key,tier))
            }
        }
    }
    //
}


/**
 * 
 */
class Tier {

    constructor() {
        this.lru_buffer = false
        this.hh_bufer = false
        this.count = 0
        this.lru_key = false
        this.hh_key = false
    }

    /**
     * 
     * @param {Number} sz - size of the element being stored
     * @param {Number} hss - hop scotch scale
     * @param {Number} rec_size - size of the element being stored
     * @returns {Array} - [this.lru_key,this.hh_key] which are the region keys 
     */
    init(sz,hss,rec_size) {
        this.lru_buffer = shm.create(sz); // create another tieir
        this.lru_key = this.lru_buffer.key
        this.count = shm.initLRU(this.lru_key,rec_size,sz,true)
        //
        sz = (hss*this.count*(WORD_SIZE + LONG_WORD_SIZE) + HH_HEADER_SIZE)
        this.hh_bufer = shm.create(sz); 
        this.hh_key = this.hh_bufer.key
        shm.initHopScotch(this.hh_key,this.lru_key,true,(this.count*hss))
        //
        return [this.lru_key,this.hh_key]  // LRU is memory storing the objects in an LRU, HH is the hopscotch tables
    }

    /**
     * 
     * @param {Number} sz - size of the region
     * @param {Number} hss - hop scotch scale
     * @param {Number} rec_size - size of the element being stored
     * @param {Number} lru_key 
     * @param {Number} hh_key 
     */
    attach(sz,hss,rec_size,lru_key,hh_key) {
        this.lru_buffer = shm.get(lru_key); //
        this.count = shm.initLRU(lru_key,rec_size,sz,false)
        this.hh_bufer = shm.get(hh_key);
        shm.initHopScotch(hh_key,lru_key,false,(this.count*hss))
        this.hh_key = hh_key
        this.lru_key = lru_key
    }



}



// proc_names -- the list of js file names that will be attaching to the regions.
// initializer -- true if master of ceremonies
//
// note: the initializer has to be called first before others.


/**
 * 
 * This class defines the relationship between the hash table and its use as an LRU. 
 * This module accepts the applications hash of the data being used to identify the data.
 * The hash must be 32 bits or less. Because, the size of the hash table is likely to be 
 * smaller than the range of the hashes, this class augments the hash key by 
 * taking the modulus of the hash with respect to the number of elements that the table will accomodate.
 * 
 * As long as the max table size remains fixed, the augmented hash will not have to change. 
 * Objects may be updated against the old hash. If an application wishes to change the hash of an object to reflect
 * the change of the data, the object should first be removed from the hash table and then be reinserted.
 * 
 * This will be configured with a number of processes known prior to statup.
 * Various states of behavior may be quiessent until other processes join the use of the regions,
 * but a number (max) of processes is expected to join at some point in time. 
 * 
 */
class ShmLRUCache extends ReaderWriter {

    constructor(conf) {
        super(conf)
        this.count = DEFAULT_ELEMENT_COUNT
        this.conf = conf

        //
        // Provide a hash function by default or externally.
        try {
            this.hasher = conf.hasher ? (() =>{ hasher = require(conf.hasher); return(hasher.init(conf.seed)); })()
                                      : (() => {
                                            return(init_default(conf.seed))
                                        })()
        } catch (e) {
            this.hasher = init_default(conf.seed)
        }
        //
        this.eviction_interval = null
        this._use_immediate_eviction = false
        this.hop_scotch_scale = conf.hop_scotch_scale ? parseInt(conf.hop_scotch_scale) : 2
        this._max_evicts = (conf.max_evictions !== undefined) ? parseInt(conf.max_evictions) : false
        //
        this.tiers = new Array(this.tier_count)
        for ( let i = 0; i < this.tier_count; i++ ) this.tiers[i] = new Tier()
        //
        if ( typeof conf._test_use_no_memory === "undefined" ) {
            this.init_shm_communicator(conf)
            this.init_cache(conf)    
        }
    }
    


    /**
     * 
     * @param {object} conf 
     */
    init_cache(conf) {
        //
        this.record_size = parseInt(conf.record_size)
        this.count = (conf.el_count !== undefined) ? parseInt(conf.el_count) : DEFAULT_ELEMENT_COUNT;
        //
        let NN = this.tier_count
        let sz = ((this.count*this.record_size) + LRU_HEADER)
        let hss = this.hop_scotch_scale
        let rec_size = this.record_size
        let p_offset = SUPER_HEADER  // even is the initializer is not at 0, all procs can read from zero
        //
        if ( this.initializer ) {
            //
            this._use_immediate_eviction = conf.immediate_evictions ? conf.immediate_evictions : false
            //
            for ( let i = 0; i < NN; i++ ) {
                let tier = this.tiers[i]
                let [lru_key,hh_key] = tier.init(sz,hss,rec_size)
                //
                this.com_buffer[p_offset*i + INFO_INDEX_LRU] = lru_key
                this.com_buffer[p_offset*i + INFO_INDEX_HH] = hh_key
            }
            // Eviction -- only the process that manages memory can setup periodic eviction.
            if ( conf.evictions_timeout ) {
                this.setup_eviction_proc(conf)
            }
            //
        } else {
            //
            for ( let i = 0; i < NN; i++ ) {
                let tier = this.tiers[i]
                let lru_key = this.com_buffer[p_offset*i + INFO_INDEX_LRU]
                let hh_key = this.com_buffer[p_offset*i + INFO_INDEX_HH]
                //
                tier.attach(sz,hss,rec_size,lru_key,hh_key)
            }
            //
        }
    }

    // ---- ---- ---- ---- ---- ---- ---- ---- ----

    hash(value) {
        let hh = this.hasher(value)
        return( this.augment_hash(hh) )
    }

    hash_pair(value) {
        let hh = this.hasher(value)
        return( this.augmented_hash_pair(hh) )
    }

    // ----
    pure_hash(value) {
        let hh = this.hasher(value)
        return hh
    }

    // ----
    /**
     * 
     * The application determines the key, which should be a numeric hash of the value. 
     * The hash of the value should be almost unique for smooth operation of the underlying hash table data structure.
     * 
     * This method returns an augmented hash, which is really the hash annotated with its
     * modulus determined by the hash table size.
     * 
     * @param {Number} key 
     * @param {object} value 
     * @returns {string} - a hyphenated string where the left is the modulus of the hash and the right is the hash itself.
     */
    augment_hash(hash) {
        let hh = hash
        let top = hh % this.count
        let augmented_hash_token = `${top}-${hh}`
        return( augmented_hash_token )
    }

    /**
     * 
     * The application determines the key, which should be a numeric hash of the value. 
     * The hash of the value should be almost unique for smooth operation of the underlying hash table data structure.
     * 
     * This method returns an augmented hash, which is really the hash annotated with its
     * modulus determined by the hash table size.
     * 
     * @param {Number} key 
     * @param {object} value 
     * @returns {Array} - a hyphenated string where a[0] is the modulus of the hash and a[1] is the hash itself.
     */
    augmented_hash_pair(hash) {
        let hh = hash
        let top = hh % this.count
        return( [top,hh] )
    }
  

    /**
     * Set will automatically go to the first tier. 
     * 
     * About duplicating entries or not with tier awareness...
     * The set is taken as truth. Truth is established by the applications.
     * Only, after the value is set, another thread may discard the aged entry.
     * 
     * Hence, a set is an update that moves an entry to the first tier if the entry is present.
     * After set successfully stores a value, it adds work to a deduplicating thread
     * 
     * @param {string|Array} hash_augmented - hyphentated string or a pair
     * @param {string|Number} value 
     * @returns {Number}
     */
    async set(hash_augmented,value,ith_tier) {
        if ( typeof value === 'string' ) {
            if ( !(value.length) ) return(-1)
            if ( value.length > this.record_size ) return(-1)    
        } else {
            value = (value).toString(16)
        }
        let pair = Array.isArray(hash_augmented) ?  hash_augmented : hash_augmented.split('-')
		let hash = parseInt(pair[0])
		let index = parseInt(pair[1])
        //
        if ( this.proc_index < 0 ) return  // guard against bad initialization and test cases

        let tier = this.tiers[0]
        if ( !(isNaN(ith_tier)) && (ith_tier >= 0) && (ith_tier < this.tiers.length)) {
            tier = this.tiers[ith_tier]
        }

        this.lock_asset(0)
        //
        let status = shm.set(tier.lru_key,value,hash,index)   // SET
        //
        if ( ((status === false) || (status < 0)) && this._use_immediate_eviction) {
            // error condition...
            let time_shift = 0
            let reduced_max = 20
            let status_retry = 1
            while ( reduced_max > 0 ) {
                let [t_shift,rmax] = this.immediate_evictions(tier,time_shift,reduced_max)
                status_retry = shm.set(tier.lru_key,value,hash,index)
                if ( status_retry > 0 ) break;
                time_shift = t_shift
                reduced_max = rmax
            }
            if (  (status_retry === false) || (status_retry < 0) ) {
                throw new Error("evictions fail to regain space")
            }
        }
        this.unlock_asset(0)
        return status
    }

    /**
     * 
     * @param {string|Array} hash_augmented 
     * @returns {string|boolean}
     */
    async get(hash_augmented) {
        let pair = Array.isArray(hash_augmented) ?  hash_augmented : hash_augmented.split('-')
		let hash = parseInt(pair[0])
        let index = parseInt(pair[1])
        if ( this.proc_index < 0 ) return(false)  // guard against bad initialization and test cases
        this.lock_asset()
        let value = shm.get_el_hash(this.lru_key,hash,index)
        this.unlock_asset()
        return(value)
    }

    /**
     * 
     * @param {string} hash_augmented 
     * @returns {boolean|Number}
     */
    async del(hash_augmented) {
        let pair = Array.isArray(hash_augmented) ?  hash_augmented : hash_augmented.split('-')
		let hash = parseInt(pair[0])
        let index = parseInt(pair[1])
        if ( this.proc_index < 0 ) return(false)  // guard against bad initialization and test cases
        this.lock_asset()
        let result = shm.del_key(this.lru_key,hash,index)
        this.unlock_asset()
        return(result)
    }

    /**
     * 
     * @param {string} hash_augmented 
     * @returns 
     */
    async delete(hash_augmented) {
        return this.del(hash_augmented)
    }


    // ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

    /**
     * 
     * @param {object} conf 
     * @returns {boolean}
     */
    setup_eviction_proc(conf) {
        let eviction_timeout = (conf.sessions_length !== undefined) ? conf.sessions_length : DEFAULT_RESIDENCY_TIMEOUT
        let prev_milisec = (conf.aged_out_secs !== undefined) ? (conf.aged_out_secs*1000) : DEFAULT_RESIDENCY_TIMEOUT
        let cutoff = Date.now() - prev_milisec
        let max_evict = (this._max_evicts !== false) ? this._max_evicts : MAX_EVICTS
        let self = this
        if ( this.proc_index < 0 ) return(false)  // guard against bad initialization and test cases
        this.eviction_interval = setInterval(() => {
            let evict_list = shm.run_lru_eviction(this.lru_key,cutoff,max_evict)
            self.app_handle_evicted(evict_list)
        },eviction_timeout)
        return true
    }

    // ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

    immediate_evictions(tier,age_reduction,e_max) {
        let conf = this.conf
        let prev_milisec = (conf.aged_out_secs !== undefined) ? (conf.aged_out_secs*1000) : DEFAULT_RESIDENCY_TIMEOUT
        if ( (typeof age_reduction === 'number') && (age_reduction < prev_milisec)) {
            prev_milisec = age_reduction
        }
        let cutoff = Date.now() - prev_milisec
        let max_evict = (this._max_evicts !== false) ? this._max_evicts : MAX_EVICTS
        if ( (e_max !== undefined) && (e_max < max_evict) ) {  // upper limit on the calling value
            max_evict = e_max
        }
        if ( max_evict <= 0 ) return([0,0])
        if ( this.proc_index < 0 ) return([0,0])  // guard against bad initialization and test cases
        //
        let evict_list = shm.run_lru_eviction(tier.lru_key,cutoff,max_evict)
        if ( evict_list.length ) {
            let self = this
            setTimeout(() => {
                self.app_handle_evicted(evict_list)
            },250)    
        }
        //
        return [(prev_milisec - 100),(max_evict - 2)]
    }


    immediate_mapped_evictions(age_reduction,e_max) {
        let conf = this.conf
        let prev_milisec = (conf.aged_out_secs !== undefined) ? (conf.aged_out_secs*1000) : DEFAULT_RESIDENCY_TIMEOUT
        if ( (typeof age_reduction === 'number') && (age_reduction < prev_milisec)) {
            prev_milisec = age_reduction
        }
        let cutoff = Date.now() - prev_milisec
        let max_evict = (this._max_evicts !== false) ? this._max_evicts : MAX_EVICTS
        if ( (e_max !== undefined) && (e_max < max_evict) ) {  // upper limit on the calling value
            max_evict = e_max
        }
        if ( max_evict <= 0 ) return([0,0])
        if ( this.proc_index < 0 ) return([0,0])  // guard against bad initialization and test cases
        //
        this.lock_asset()
        let evict_list = shm.run_lru_eviction_get_values(this.lru_key,cutoff,max_evict)
        this.unlock_asset()
        //
        return evict_list
    }


    immediate_targeted_evictions(hash_augmented,age_reduction,e_max) {
        let pair = Array.isArray(hash_augmented) ?  hash_augmented : hash_augmented.split('-')
		let hash = parseInt(pair[0])
        let index = parseInt(pair[1])
        if ( this.proc_index < 0 ) return(false)  // guard against bad initialization and test cases
        //
        let conf = this.conf
        let prev_milisec = (conf.aged_out_secs !== undefined) ? (conf.aged_out_secs*1000) : DEFAULT_RESIDENCY_TIMEOUT
        if ( (typeof age_reduction === 'number') && (age_reduction < prev_milisec)) {
            prev_milisec = age_reduction
        }
        let cutoff = Date.now() - prev_milisec
        let max_evict = (this._max_evicts !== false) ? this._max_evicts : MAX_EVICTS
        if ( (e_max !== undefined) && (e_max < max_evict) ) {  // upper limit on the calling value
            max_evict = e_max
        }
        if ( max_evict <= 0 ) return([0,0])
        if ( this.proc_index < 0 ) return([0,0])  // guard against bad initialization and test cases
        //
        this.lock_asset()
        let evict_list = shm.run_lru_targeted_eviction_get_values(this.lru_key,cutoff,max_evict,hash,index)
        this.unlock_asset()
        //
        return evict_list
    }


    // ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

    disconnect(opt) {
        if ( this.eviction_interval !== null ) {
            clearInterval(this.eviction_interval)
        }
        if ( opt === true || ( (typeof opt === 'object') && ( opt.save_backup = true ) )) {
            // save buffers....
        }
        shm.detachAll()
        return(true)
    }

    // ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

    /// handle_evicted(evict_list)
    app_handle_evicted(evict_list) {
        // app may decide to forward these elsewhere are send shutdown messages, etc.
    }


    set_sigint_proc_stop(func) {
        shm.set_sigint_proc_stop(func)
    }

}



module.exports = ShmLRUCache








// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

//Exports
module.exports.create = create;
module.exports.get = get;
module.exports.detach = detach;
module.exports.detachAll = detachAll;
module.exports.getTotalSize = shm.getTotalSize;
module.exports.BufferType = BufferType;
module.exports.LengthMax = lengthMax;
//
module.exports.set_sigint_proc_stop = set_sigint_proc_stop
