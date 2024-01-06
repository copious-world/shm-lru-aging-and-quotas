
//
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <type_traits>
#include <iostream>

#include <deque>
#include <map>

using namespace std;



static_assert(sizeof(uint64_t) == sizeof(atomic<uint64_t>), 
        "atomic<T> isn't the same size as T");

static_assert(atomic<uint64_t>::is_always_lock_free,  // C++17
        "atomic<T> isn't lock-free, unusable on shared mem");


// more like original
const uint64_t ui_53_1s = (0x0FFFFFFFFFFFFFFFF >> 11);
const double d_53_0s = (double)((uint64_t)1 << 53);

inline uint64_t rotl64 ( uint64_t x, int8_t r )
{
  return (x << r) | (x >> (64 - r));
}




double map_to_unit(uint64_t x) {
    double v = (x & ui_53_1s)/d_53_0s;
    return v;
}

uint64_t mix32_64(uint32_t host_hash,uint32_t khash) {
    uint64_t k = khash;
    k ^= k >> 33;
    k *= (0xff51afd7ed558ccd);
    k ^= k >> 33;
    k *= (0xc4ceb9fe1a85ec53);
    k ^= k >> 33;

    uint64_t h = host_hash;
    h ^= h >> 33;
    h *= (0xff51afd7ed558ccd);
    h ^= h >> 33;
    h *= (0xc4ceb9fe1a85ec53);
    h ^= h >> 33;

    return k + h;
}


double O_hash_mix_and_map(uint32_t host_hash,uint32_t khash) {
    uint64_t int_combo = mix32_64(host_hash,khash);
    double mapped = map_to_unit(int_combo);
    return mapped;
}





typedef struct Bucket_t {
    double                  _weight;
    uint16_t                _host_id;
    uint32_t                _host_hash; // simulate the hash of the host name...
    double                  _score; // temporary for each hash each round
    void                    *_host_access_object;
} wrh_Bucket;


deque<wrh_Bucket> sg_all_wrh_hosts;
uint16_t sg_host_count = 0;


void initialize_host_buckets(uint16_t N,double *weights,uint16_t *host_ids) {
    sg_all_wrh_hosts.resize(N);
    double *end_ws = weights + N;
    sg_host_count = N;
    //
    deque<wrh_Bucket>::iterator dit = sg_all_wrh_hosts.begin();
    uint32_t prev_rand = 1;
    while ( weights < end_ws ) {
        wrh_Bucket &b = *dit++;
        b._weight = *weights++;
        b._host_id = *host_ids++;
        b._host_access_object = nullptr;
        srand(time(nullptr) + prev_rand);
        b._host_hash = max(rand(),1);
        prev_rand = b._host_hash%83;
        // cout << b._host_hash << "," << prev_rand << endl;
    }

    //cout << endl;
}


bool add_host(uint16_t host_id,double weight) {
    auto test = [&](wrh_Bucket &b) { return b._host_id == host_id; };
    if ( find_if(sg_all_wrh_hosts.begin(),sg_all_wrh_hosts.end(),test) != sg_all_wrh_hosts.end() ) {
        return false;
    }
    wrh_Bucket host_b;
    sg_host_count++;
    host_b._host_id = sg_host_count;
    host_b._weight = weight;
    host_b._host_access_object = nullptr;
    //
    srand(time(nullptr) + 17717*rand());
    host_b._host_hash = max(rand(),1);
    //
    sg_all_wrh_hosts.push_back(host_b);
    //
    return true;
}

bool remove_host(uint16_t host_id) {
    auto test = [&](wrh_Bucket &b) { return b._host_id == host_id; };
    auto b = find_if(sg_all_wrh_hosts.begin(),sg_all_wrh_hosts.end(),test);
    if ( b != sg_all_wrh_hosts.end() ) {
        sg_all_wrh_hosts.erase(b);
        return true;
    }
    return false;
}



/*

// hashFloat64 calculates hash of key||seed and converts
// result into float64 number in [0:1).
func hashFloat64(key string, seed string) float64 {
	h, err := blake2xb.New(16) // XXX: how many calls does rand make?
	if err != nil {
		panic(err)
	}
	h.Write([]byte(key))
	h.Write([]byte(seed))
	rnd := rand.NewWithReader(h)
	return rnd.Float64()
}

*/


double score(double weight,double hash) {
    double S = -weight/(log(hash));
    return S;
}


double hash_mix_and_map(uint32_t host_hash,uint32_t khash) {
    uint64_t int_combo = ((khash << (host_hash%13)) ^ host_hash) + host_hash*17;
    uint32_t int_clip = int_combo & (0xFFFFFFFFFFFFFFFF);
    double mapped = double(int_clip)/(double)(UINT32_MAX);
    return mapped;
}



int test_wrh(void) {
    cout << "This is a test" << endl;
                // {10.0,25.0,50,75.0,100.0,125.0,150.0,175.0,200.0,225.0};
    double initial_weights[10] = {7.0,12.0,17,22.0,29.0,34.0,39.0,44.0,49.0,54.0};
    uint16_t host_ids[10] = {1,2,3,4,5,6,7,8,9,10};

    initialize_host_buckets(10,initial_weights,host_ids);

    add_host(11,200.0);
    remove_host(3);
    if ( remove_host(3) ) {
        cout << "OOPS! (3)  is still there to remove.";
    } else {
        cout << "test pass";
    }
    add_host(12,800.0);
    
    cout << endl;
    //

    srand(time(nullptr) + 137);

    map<int,uint32_t> lucky_count;
    map<int,double> lucky_weight;
    map<int,map<int,uint32_t>> second_lucky_count;

    //
    for_each(sg_all_wrh_hosts.begin(),sg_all_wrh_hosts.end(),[&](wrh_Bucket &b) { lucky_count[b._host_id] = 0; lucky_weight[b._host_id] = b._weight; } );

    for ( uint32_t i = 0; i < 10000000; i++ ) {
        uint32_t test_hash = rand();

//        cout << "test_hash:  " << test_hash << endl;

        auto score_and_report = [&](wrh_Bucket &b) {
            double d_hash = O_hash_mix_and_map(b._host_hash,test_hash);    // O_hash_mix_and_map
            //
            auto mapping = log(double(d_hash));
            b._score = score(b._weight,d_hash);
            //cout << b._host_id << "\t" << b._weight << "  ::  " << b._score  << " :map: " << d_hash << ", " << mapping << endl;
        };

        for_each(sg_all_wrh_hosts.begin(),sg_all_wrh_hosts.end(),score_and_report);

        auto score_check = [](const wrh_Bucket& left, const wrh_Bucket& right) {
            return (left._score < right._score);
        };

        auto winner = max_element(sg_all_wrh_hosts.begin(),sg_all_wrh_hosts.end(),score_check);
        //
        // cout << endl << "and the winnner is...." << endl;
        // score_and_report(*winner);
        lucky_count[(*winner)._host_id]++;
        //
        deque<wrh_Bucket> tmp_all_wrh_hosts;
        auto wid = (*winner)._host_id;
        tmp_all_wrh_hosts.resize(sg_all_wrh_hosts.size());
        copy_if(sg_all_wrh_hosts.begin(),sg_all_wrh_hosts.end(),tmp_all_wrh_hosts.begin(),[&](const wrh_Bucket& item){ return item._host_id != wid; });

        auto snd_place = max_element(tmp_all_wrh_hosts.begin(),tmp_all_wrh_hosts.end(),score_check);
        //
        second_lucky_count[(*winner)._host_id][(*snd_place)._host_id]++;
        //
    }

    for ( auto p : lucky_count ) {
        cout << p.first << " : " << p.second << " W: " << lucky_weight[p.first] << endl;
        //
        uint32_t max = 0;
        int id = -1;
        for ( auto q : second_lucky_count[p.first] ) {
            uint32_t count = second_lucky_count[p.first][q.first];
            if ( max < count ) {
                max = count;
                id =  q.first;
            }
            //cout << "\t" << q.first <<  " S: " << count << " ID " << q.first << endl;
        }
        cout << "\tID: " << id <<  "\tS: " << max << endl;
        //
    }

    return 0;
}


int test_HD_separation() {

    double d_orthoganality = 0.0;

    cout << "D-ORTHOGANALITY:" << endl;
    for ( uint8_t i = 0; i < 16; i++ ) {
        //
        double D = 10.0*(i+1); // dimension
        //
        double d_orthoganality_lb = sqrt(D)*(sqrt(D)- 6)/(2*D);
        double d_orthoganality_ub = sqrt(D)*(sqrt(D) + 6)/(2*D);
        //
        d_orthoganality = (d_orthoganality_lb + d_orthoganality_ub)/2.0;
        //
        cout << "\tLB: " << d_orthoganality_lb << "\t(" << (d_orthoganality_ub - d_orthoganality)  << ")\t" << "UB: " << d_orthoganality_ub; 
        cout << endl;
    }

    return 0;
}


#include <bitset>
#include <random>

template<uint32_t N>
unsigned int hamming(bitset<N> &a,bitset<N> &b) {
    bitset<N> diff = a ^ b;
    return diff.count();
}

random_device rdv;  // a seed source for the random number engine
mt19937 gen_v(rdv()); // mersenne_twister_engine seeded with rd()

template<uint32_t N>
void flip_random_positions(bitset<N> &t_bits,uint32_t k,uint32_t index,uint32_t n_vec,bitset<N> &prev) {
    //
    uint16_t comp_size = (N/n_vec);
    uint16_t start = index*comp_size;
    if ( (start <= N/2) && start > (N/2 - 2) ) {
        start -= (comp_size + k)/4;
        k = 3*k/4;
    } else if ( start > N/2) {
        start -= comp_size/2;
        k = k/3;
    } else if ( start > k/2 ) {
        start -= k/2;
    } else {
        k = k/2;
        comp_size >>= 1;
    }
    //
    uint16_t stop = comp_size + k + start;
    uniform_int_distribution<uint32_t> pdist(start,stop);

    cout << k << " comp_size: " << comp_size << " start: " << start << " stop: " << stop << endl;
    // return;
    //
    bool taken[N];
    for ( uint32_t i = 0; i < N; i++ ) {
        taken[i] = false;
    }
    //
    /*
    for ( uint32_t j = 0; j < start/2; j++ ) {
        t_bits[j] = prev[j];
    }
    */
    //
    for ( uint32_t i = 0; i < k; i++ ) {
        uint32_t p = pdist(gen_v) % N;
        if ( !(taken[p]) ) {
            if ( p >= start ) { t_bits.flip(p); }
            //t_bits.flip(p);
            taken[p] = true;
        } else i--;
    }
    cout << endl;
    //
}


template<uint32_t D,uint32_t CompSize,uint32_t VSetCard>
int gen_vector_set() {
    //
    uint32_t N = VSetCard;
    uint32_t d = D;
    uint32_t m = CompSize;
    //
    uint32_t kbits_to_flib = d/m;

    random_device rd;  // a seed source for the random number engine
    mt19937 gen(rd()); // mersenne_twister_engine seeded with rd()
    uniform_int_distribution<uint64_t> distrib(11, (((uint64_t)1 << (d-1) )- 13));
 
    // Use distrib to transform the random unsigned int
    // generated by gen into an int in [1, 6]
    for ( int n = 0; n != 10; ++n ) distrib(gen);

    bitset<D> current_B(0);
    current_B = distrib(gen);
    //
    //cout << current_B << endl;
    deque<bitset<D>> HD_vectors;
    //
    HD_vectors.push_back(current_B);
    map<bitset<D>,bool> used;

    for ( int i = 0; i < N; i++ ) {
        bitset<D> t_bits(0);
        //
        flip_random_positions<D>(t_bits,kbits_to_flib,i,N,current_B);
        current_B = current_B ^ t_bits;

        if ( (i == 2) && find_if(HD_vectors.begin(),HD_vectors.end(),[&](bitset<D> &qry) {
                        bitset<D> check = current_B ^ qry;
                        //cout << check.count() << " :check: " << check << endl;
                        if ( check.count() == 0 ) return true;
                        return false;
                    }) != HD_vectors.end() ) {
            i--; continue;
        }


        //
        HD_vectors.push_back(current_B);
    }

    current_B = HD_vectors.at(0);
    bitset<D> prev_B = HD_vectors.at(0);
    //
    int64_t h_prev = 0;

    for ( auto b : HD_vectors ) {
        int64_t h_current = hamming<D>(current_B,b);
        int64_t forward_diff = (h_current - h_prev);
        cout <<  ":: " << h_current << " df: " << forward_diff << " +/-:: " << (bool)(forward_diff >= 0) << endl;  // b <<
        prev_B = b;
        h_prev = h_current;
    }

    return 0;
}


int main(int arcg, char **argv) {
    //test_HD_separation();
    gen_vector_set<2048*4,16,50>();
    return(0);
}