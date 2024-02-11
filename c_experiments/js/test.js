

const { spawn } = require('child_process');
//const shm = require('./build/Release/shm-lru-aging-and_quotas.node');
const { XXHash32 } = require('xxhash32-node-cmake')


const shm_mod = {}


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




class SHM_CLASS {
    constructor() {}
}

const shm = new SHM_CLASS();

// ---- ---- ---- ---- ---- ---- -

const fs = require('fs');

let g_sleepy_calls = 0;

function sleepy(secs) {
    let p = new Promise( (resolve,reject) => {
        let sleepy_call = (g_sleepy_calls++ + 1)
        setTimeout(() => {
            console.log(`shutting down: ${sleepy_call}`)
            resolve(true) 
        },secs*1000)
    })
    return p
}



class TableMaster {

    constructor(conf) {
        //
        this.master_token = default_hash(conf.identity_phrase)
        //
        let proc_count = conf.num_procs
        let sz = shm_mod.com_buffer_size();   // communication buffer -- implementation tells us what it needs
        //
        this.com_buffer = shm_mod.create(this.master_token,(proc_count*sz))
        //
        this.tier_tokens = {}
        this.tier_hh_tokens = {}
        this.tier_refs = {}
        let count = conf.element_count
        let hss = this.hop_scotch_scale
        for ( let tier_conf of tiers ) {
            //
            let key = tier_conf.key;
            let lru_key = default_hash(key)
            this.tier_tokens[tier_conf.tier] = lru_key
            //
            let sz = shm_mod.lru_buffer_size(count);    // communication buffer -- implementation tells us what it needs
            this.tier_refs.lru_buffer =  shm_mod.create(lru_key,sz)
            this.tier_refs.count = shm_mod.initLRU(lru_key,this.record_size,sz,true)

            key = tier_conf.hh_key;
            let hh_key = default_hash(key)
            //
            sz = shm_mod.hop_scotch_buffer_size(count); // communication buffer -- implementation tells us what it needs
            this.tier_refs.hh_bufer = shm_mod.create(hh_key,sz)
            //
            this.tier_hh_tokens[tier_conf.tier] = hh_key
            shm_mod.initHopScotch(hh_key,this.lru_key,true,(this.count*hss))
        }
        //
    }

    init(conf) {
        return true
    }

    async shut_down() {  // mostly turn off the shared
        return sleepy(1)
    }


}


let g_spawn_count = 0

class TierOwner {

    constructor(conf) {

        this.tier_token = default_hash(conf.identity_phrase)
        this.com_buffer = shm_mod.attach(this.tier_token)
        if ( this.com_buffer === null ) {
            console.dir(conf)
            console.log("module_path DOES NOT match with master_of_ceremonies OR master_of_ceremonies not yet initialized")
            throw(new Error("possible configuration error"))
        }
        //
        this.tier_tokens = {}
        this.tier_hh_tokens = {}
        this.tier_refs = {}
        let count = conf.element_count
        let hss = this.hop_scotch_scale
        //
        for ( let tier_conf of tiers ) {
            //
            let key = tier_conf.key;
            let lru_key = default_hash(key)
            //
            this.tier_tokens[tier_conf.tier] = lru_key
            //
            this.tier_refs.lru_buffer = shm_mod.attach(lru_key);
            let sz = shm.lru_buffer_size(count);    // communication buffer -- implementation tells us what it needs
            this.tier_refs.count = shm.initLRU(lru_key,this.record_size,sz,false)
            //
            key = tier_conf.hh_key;
            let hh_key = default_hash(key)
            this.tier_refs.hh_bufer = shm_mod.attach(hh_key);

            sz = shm.hop_scotch_buffer_size(count); // communication buffer -- implementation tells us what it needs
            this.tier_hh_tokens[tier_conf.tier] = hh_key
            shm.initHopScotch(hh_key,this.lru_key,false,(this.count*hss))
       }

    }

    init(conf) {

        return true
    }

    spawn() {
        return g_spawn_count++
    }

    async shut_down() {
        return sleepy(1)
    }

}


let argv = process.argv

let file_name = "./confs/test.json"
if ( argv.length > 2) {
    console.log(argv.length, argv[2])
    file_name = argv[2]
}

let conf = "unknown"

try {
    let conf_json = fs.readFileSync(file_name).toString()
    conf = JSON.parse(conf_json)    
} catch (e) {
    console.log(`cannot find ${file_name}`)
}

console.dir(conf)


init_default(conf.seed)

let master_proc = conf.master // the master initiates the shared memory regions, tables, shared atomics, etc.

let master = new TableMaster(master_proc)
let child_refs = new Map()
if ( master.init(conf) ) {
    for ( let child of conf.children ) {
        let tier_owner = new TierOwner(child)
        tier_owner.init(conf)
        child_refs.set(tier_owner.spawn(),tier_owner);
    }
} else {
    console.log("did not intialize")
}


// do stuff

console.dir(child_refs)
console.log(child_refs.entries())
// 

let shut_down_promises = []
for ( let [key,tier_owner] of child_refs.entries() ) {
    console.log(`key ${key}`)
    shut_down_promises.push(tier_owner.shut_down())
}

( async () => {
    await Promise.all(shut_down_promises)
    await master.shut_down()    
})()












// class SHM_CLASS {

// #include <stdio.h>                                                                                                                  

// #define SHMMAX_SYS_FILE "/proc/sys/kernel/shmmax"

// int main(int argc, char **argv)
// {
//     unsigned int shmmax;
//     FILE *f = fopen(SHMMAX_SYS_FILE, "r");

//     if (!f) {
//         fprintf(stderr, "Failed to open file: `%s'\n", SHMMAX_SYS_FILE);
//         return 1;
//     }

//     if (fscanf(f, "%u", &shmmax) != 1) {
//         fprintf(stderr, "Failed to read shmmax from file: `%s'\n", SHMMAX_SYS_FILE);
//         fclose(f);
//         return 1;
//     }

//     fclose(f);

//     printf("shmmax: %u\n", shmmax);

//     return 0;
// }
