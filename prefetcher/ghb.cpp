//
// Created by Umasou on 2021/5/25.
//

#include "cache.h"
#include <map>
#include <vector>
#include <queue>
#include <cstdlib>
#include <iostream>
#include "cache.h"


using namespace std;

#define PREFETCH_LOOK_AHEAD 4
#define PREFETCH_DEGREE 4
#define IT_SIZE 256
#define GHB_SIZE 256
#define random(x)(rand()%x)

struct ghb_entry{
    uint64_t cache_line_addr;
    uint64_t ip;
    ghb_entry *prev_ptr;
    ghb_entry(int64_t addr, int64_t ip):cache_line_addr(addr), ip(ip),prev_ptr(nullptr){}
};

class GHB{
public:
    map<uint64_t, ghb_entry*> it_table;

    queue<ghb_entry*> ghb_table;
public:
    GHB(){

    }

    vector<uint64_t> trigger(uint64_t addr, uint64_t ip){
        //make prefetch
        vector<uint64_t> prefetch_addr;
        uint64_t  delta = getDelta(ip);
        if (0 == delta){
            return prefetch_addr;
        }
        for (int i = 0; i < PREFETCH_DEGREE; ++i) {
            prefetch_addr.push_back(addr);
        }
        //add addr 2 ghb
        ghbAddNewEntry(((addr>>LOG2_BLOCK_SIZE)<<LOG2_BLOCK_SIZE), ip);
        return prefetch_addr;


    }

//    vector<uint64_t> findPrefetchAddress(uint64_t delta, uint64_t origin_addr){
//        vector<uint64_t> prefetch_addr;
//
//    }
private:
    void ghbAddNewEntry(uint64_t addr, uint64_t ip){
        ghb_entry *insert_entry = new ghb_entry(addr>>LOG2_BLOCK_SIZE, ip);
        //insert ghb
        if (GHB_SIZE == ghb_table.size()){
            //del ghb
            ghb_entry *del_entry = ghb_table.front();
            ghb_table.pop();
            //del point
            uint64_t del_entry_ip = del_entry->ip;
            if (it_table.end() != it_table.find(del_entry_ip)){
                ghb_entry *entry_with_same_ip = it_table[del_entry_ip];
                if (entry_with_same_ip == del_entry){
                    it_table.erase(ip);
                }else{
                    for (ghb_entry *ghb_pointer = entry_with_same_ip; ghb_pointer != nullptr; ghb_pointer = ghb_pointer->prev_ptr) {
                        if (ghb_pointer == del_entry){
                            ghb_pointer->prev_ptr = nullptr;
                            break;
                        }
                    }
                }
            }
        }
        ghb_table.push(insert_entry);

        //insert it
        if (it_table.end() != it_table.find(ip)){
            ghb_entry *entry_with_same_ip = it_table[ip];
            it_table[ip] = insert_entry;
            insert_entry->prev_ptr = entry_with_same_ip;
        }
        else{
            if (IT_SIZE == it_table.size()){
                it_table.erase(advance(it_table.begin(), random(IT_SIZE - 1)));
            }
            it_table.insert(make_pair(ip, insert_entry));
        }


    }

    uint64_t getDelta(uint64_t ip){
        ghb_entry *ip_entry = it_table[ip];
        vector<uint64_t>history_addr;
        for (int i = 0; i < 3; ++i) {
            history_addr.push_back(ip_entry->cache_line_addr);
            ip_entry = ip_entry->prev_ptr;
            if (nullptr == ip_entry){
                break;
            }
        }
        if (3 != history_addr.size()){
            return 0;
        }
        uint64_t delta1 = (history_addr[0] - history_addr[1]) % LOG2_PAGE_SIZE;
        uint64_t delta2 = (history_addr[1] - history_addr[2]) % LOG2_PAGE_SIZE;
        if (delta1 != delta2){
            return 0;
        }
        return delta1;
    }
};


GHB *ghb = new GHB();



void CACHE::l2c_prefetcher_initialize()
{
    cout << "CPU " << cpu << " L2C GHB prefetcher" << endl;
}

uint32_t CACHE::l2c_prefetcher_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type, uint32_t metadata_in)
{
    vector<uint64_t> prefetch_addr = ghb->trigger(addr, ip);
    for (int i = 0; i < prefetch_addr.size(); ++i) {
        // check the MSHR occupancy to decide if we're going to prefetch to the L2 or LLC
        if (MSHR.occupancy < (MSHR.SIZE>>1))
            prefetch_line(ip, addr, prefetch_addr[i], FILL_L2, 0);
        else
            prefetch_line(ip, addr, prefetch_addr[i], FILL_LLC, 0);
    }
    return metadata_in;
}

uint32_t CACHE::l2c_prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{
    return metadata_in;
}

void CACHE::l2c_prefetcher_final_stats()
{
    cout << "CPU " << cpu << " L2C GHB prefetcher final stats" << endl;
}
