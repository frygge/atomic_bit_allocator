//
// Created by Joerg P. Schaefer on 29.10.2023.
//

#include <chrono>
#include <bitset>
#include <cstring>
#include <vector>
#include <iostream>
#include "atomic_bit_allocator/atomic_bit_allocator.h"
#include "experiment.h"

using namespace std::chrono_literals;


template<typename bit_allocator = jps::serialized_bit_allocator<uint64_t, jps::_reentrant_lock_free_bit_allocator>>
class ThroughPutMeasurement : public jps::experiment
{
public:
    ThroughPutMeasurement( size_t n_workers, size_t buffer_size = 1024, size_t max_allocation = 8, auto run_time = 1.0s ) :
            jps::experiment( n_workers, run_time, 0.1s ),
            MAX_ALLOC( max_allocation ),
            buffer( buffer_size ),
            bit_allocator_( new ( buffer.data() ) bit_allocator( buffer_size ) )
    {}

    size_t run() {
        return jps::experiment::run( &ThroughPutMeasurement::shoot );
    }
    void shoot() {
        static thread_local auto i = 0ul;
        const auto n = MAX_ALLOC > 1 ?
                       ( i*this->get_worker_id() ) % ( MAX_ALLOC-1 ) + 1
                                     :
                       1;
        const auto p = bit_allocator_->alloc( n );

        bit_allocator_->free( p, n );
    }

    const size_t MAX_ALLOC;

    std::vector<uint8_t> buffer;
    bit_allocator* bit_allocator_;
};


size_t min_workers = 1;
size_t max_workers = 24;
size_t repeat = 1;

template<typename BA = jps::serialized_bit_allocator<uint64_t, jps::_single_threaded_bit_allocator>>
void loop_tests() {
    std::cout << "\t#worker\t#maxlen\t#ops/us" << std::endl;
    for( auto max_alloc = 1; max_alloc <= 8; max_alloc *= 2 ) {
        for( auto t = min_workers; t <= max_workers; ++t ) {
            size_t n_ops = 0;
            for( auto r = 0u; r < repeat; ++r ) {
                ThroughPutMeasurement<BA> test( t, 8192, max_alloc, 500ms );
                n_ops += test.run();
            }
            // ops/100ms = ops/repeat  ==>  ops/s = 10*ops/repeat  ==>  ops/us = 10*ops/repeat/1'000'000 = ops/repeat/100'000
            std::cout << "\t" << t << "\t" << max_alloc << "\t" << double( n_ops ) / ( repeat * 500'000. ) << std::endl;
        }
    }
}

int main( [[maybe_unused]] int argc, [[maybe_unused]] char* argv[] ) {
    std::cout << "=== mutex_based" << std::endl;
    loop_tests<jps::serialized_bit_allocator<uint64_t, jps::_single_threaded_bit_allocator>>();
    std::cout << std::endl;

    std::cout << "=== lock_free" << std::endl;
    loop_tests<jps::serialized_bit_allocator<uint64_t, jps::_reentrant_lock_free_bit_allocator>>();
    std::cout << std::endl;

    return 0;
}
