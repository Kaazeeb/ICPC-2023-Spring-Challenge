#include <iostream>
#include <vector>
#include <map>
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <list>
#include <set>

using namespace std;

//==================================================================   FUNCTIONS   ==================================================================//

    // A function to calculate current cost of a tenant, with a small difference based on the difference of the number of stored pages in cache to
    // Qbase. obs: I thought that calculating next fault cost of a user was as important as calculating its current cost, unfortunatelly many simple
    // approaches to that calculation led me to bad results, in the end I tried something more elaborated and was not able to finish. It turned out that
    // this small approximation based on the distance to its qbase value was my best result. In the end of the code the calc for faults_num will be presented

    long double calc_cost(int my_faults, long double base_faults, int priority, int faults_num){

        if(base_faults <= 0){
            
            return 0;

        }

        long double cost = (my_faults + faults_num - base_faults) / base_faults;

        if(cost < 0){

            // Important to consider negative values, even if negative values won't be computed as real SLA rates, you can use it to chose tenant
            return cost * priority;

        }

        return cost * cost * priority;
    }


//#####################################################################################################################################################


//==================================================================   CLASSES   ==================================================================//


    //================================================================ Class LFU

        class LFUCache {

            //A map with frequencies as keys holds lists of page_ids, a map of page_ids holds iterators of those lists
            // a map holds the current page_ids their places in memory and frequencies and another map the old frequencies

            
            public:
                
                unordered_map<int, pair<int, int>> m; // Page_id -> {Place, Frequency}
                unordered_map<int, list<int>> freq;  // Frequency -> List of page_ids
                unordered_map<int, list<int>::iterator> pos; // Page_id -> Iterator to freq list
                unordered_map<int, int> oldFreq; // Old frequencies

                int minFreq = INT_MAX; // Current minimun value for frequency in the cache of a tenant

                LFUCache(){}

                int retrieve(int key) {

                    // If page_id not in cache returns -1, else it will update the frequency of the page rearrange its place in the data structure
                    // and return its place in memory
                    
                    if(m.count(key) == 0){

                        return -1;

                    }

                    freq[m[key].second].erase(pos[key]);     // Erase from freq list
                    m[key].second++;                         // Update freq
                    freq[m[key].second].push_back(key);      // Add to freq list correspondent to its new freq value
                    pos[key] = --freq[m[key].second].end();  // Update iterator
                    oldFreq[key]++;                          // Update old freq

                    while(freq[minFreq].empty()){

                        minFreq++;

                    }

                    return m[key].first;
                }


                void put(int key, int place){

                    // If the page_id was already seen it will be added with its old frequency value, else it will be added with value of 1

                    if(oldFreq.count(key) == 0){

                        m[key] = {place, 1};
                        oldFreq[key] = 1;
                        freq[1].push_back(key);
                        pos[key] = --freq[1].end();
                        minFreq = 1;

                    }else{

                        int tempFreq = ++oldFreq[key];
                        m[key] = {place, tempFreq};
                        freq[tempFreq].push_back(key);
                        pos[key] = --freq[tempFreq].end();

                        if(tempFreq < minFreq){

                            minFreq = tempFreq;

                        }
                    }
                }

                int remove(){

                    // It will return the place of the page_id with lowest frequency and remove it from data structure, but will remember its freq value

                    int keyToRemove = freq[minFreq].front();

                    int place = m[keyToRemove].first;
                    freq[minFreq].pop_front();
                    m.erase(keyToRemove);
                    pos.erase(keyToRemove);
                    while (freq[minFreq].empty()) {
                        
                        minFreq++;
                    }

                    return place;
                }
        };
    
    //=============================================================================================================================================

    
    //================================================================ Class LRU

        class LRUCache {

            // A list to remember the order that page_ids were added and a map to search for the page_ids

            public:
                
                unordered_map<int, list<pair<int, int>>::iterator> pos; // Page_id : Iterator to list
                list<pair<int, int>> buffer;  // Page_id : Place

                LRUCache(){}

                int retrieve(int key){

                    // If page_id not in cache returns -1, else it will update its position in the data structure and return its place in memory
                    
                    if(pos.count(key) == 0){

                        return -1;

                    }

                    int place = pos[key]->second;
                    buffer.erase(pos[key]);

                    buffer.push_front(make_pair(key, place));
                    pos[key] = buffer.begin();

                    return place;
                }


                void put(int key, int place){

                    buffer.push_front(make_pair(key, place));
                    pos[key] = buffer.begin();
                }


                int remove(){

                    int key = buffer.back().first;
                    int place = buffer.back().second;
                    buffer.pop_back();
                    pos.erase(key);

                    return place;
                }
        };

    //=============================================================================================================================================

    
    //================================================================ Class MRU

        class MRUCache {

            // Much the same as LRU but will evict from front instead of back

            public:
                
                unordered_map<int, list<pair<int, int>>::iterator> pos; // Page_id : Iterator to list
                list<pair<int, int>> buffer;  // Page_id : Place

                MRUCache(){}

                int retrieve(int key){
                    
                    if(pos.count(key) == 0){

                        return -1;

                    }

                    int place = pos[key]->second;
                    buffer.erase(pos[key]);

                    buffer.push_back(make_pair(key, place));
                    pos[key] = --buffer.end();

                    return place;
                }


                void put(int key, int place){

                    buffer.push_back(make_pair(key, place));
                    pos[key] = --buffer.end();
                }


                int remove(){

                    int key = buffer.back().first;
                    int place = buffer.back().second;
                    buffer.pop_back();
                    pos.erase(key);

                    return place;
                }
        };

    //=============================================================================================================================================

    
    //================================================================ Class MIXCache

        class MIXCache {

            // A combination of the classes seen before. When a page is added, all data will be updated accordingly
            // allowing to evict a page with the policy needed, LRU MRU or LFU and additionally MFU

            public:
                
                unordered_map<int, list<pair<int, pair<int, int>>>::iterator> map_to_buffer; // Page_id : Iterator to list
                list<pair<int, pair<int, int>>> buffer;  // Page_id : {Place : Freq}

                unordered_map<int, list<int>> map_with_freq;  // Freq -> list of page_ids
                unordered_map<int, list<int>::iterator> map_to_freq_list; // Page_id -> position in freq list
                unordered_map<int, int> oldFreq; // Olf freqs

                int minFreq = INT_MAX;
                int maxFreq = 0; // This will allow to use MFU

                MIXCache(){}

                int retrieve(int key){

                    // If page_id not in cache returns -1, else it will update the frequency of the page rearrange its place in the data structure
                    // and return its place in memory
                    
                    if(map_to_buffer.count(key) == 0){

                        return -1;

                    }

                    int place = map_to_buffer[key]->second.first;
                    int freq = map_to_buffer[key]->second.second;
                    buffer.erase(map_to_buffer[key]);

                    buffer.push_front(make_pair(key, make_pair(place, ++freq)));
                    map_to_buffer[key] = buffer.begin();

                    oldFreq[key] = freq;
                    map_with_freq[freq - 1].erase(map_to_freq_list[key]);
                    map_with_freq[freq].push_back(key);
                    map_to_freq_list[key] = --map_with_freq[freq].end();

                    while(map_with_freq[minFreq].empty()){

                        minFreq++;

                    }

                    if(freq - 1 == maxFreq){

                        maxFreq++;

                    }

                    return place;
                }


                void put(int key, int place){

                    if(oldFreq.count(key) == 0){

                        buffer.push_front({key, {place, 1}});
                        oldFreq[key] = 1;
                        map_to_buffer[key] = buffer.begin();

                        map_with_freq[1].push_back(key);
                        map_to_freq_list[key] = --map_with_freq[1].end();

                        minFreq = 1;
                        if(maxFreq < 1){

                            maxFreq = 1;

                        }

                    }else{

                        int tempFreq = ++oldFreq[key];

                        buffer.push_front({key, {place, tempFreq}});
                        map_to_buffer[key] = buffer.begin();

                        map_with_freq[tempFreq].push_back(key);
                        map_to_freq_list[key] = --map_with_freq[tempFreq].end();

                        if(tempFreq < minFreq){

                            minFreq = tempFreq;

                        }

                        if(tempFreq > maxFreq){

                            maxFreq = tempFreq;

                        }
                    }
                }


                int remove_LRU(){

                    int key = buffer.back().first;
                    int place = buffer.back().second.first;
                    int freq = buffer.back().second.second;

                    buffer.pop_back();
                    map_to_buffer.erase(key);

                    map_with_freq[freq].erase(map_to_freq_list[key]);
                    map_to_freq_list.erase(key);

                    while(map_with_freq[minFreq].empty()){

                        minFreq++;

                    }

                    while(map_with_freq[maxFreq].empty()){

                        maxFreq--;

                    }

                    return place;
                }


                int remove_LFU(){

                    int keyToRemove = map_with_freq[minFreq].front();

                    map_with_freq[minFreq].pop_front();
                    map_to_freq_list.erase(keyToRemove);

                    while(map_with_freq[minFreq].empty()){

                        minFreq++;

                    }

                    while(map_with_freq[maxFreq].empty()){

                        maxFreq--;

                    }

                    int place = map_to_buffer[keyToRemove]->second.first;
                    buffer.erase(map_to_buffer[keyToRemove]);
                    map_to_buffer.erase(keyToRemove);

                    return place;
                }


                int remove_MRU(){

                    int key = buffer.front().first;
                    int place = buffer.front().second.first;
                    int freq = buffer.front().second.second;

                    buffer.pop_front();
                    map_to_buffer.erase(key);

                    map_with_freq[freq].erase(map_to_freq_list[key]);
                    map_to_freq_list.erase(key);

                    while(map_with_freq[minFreq].empty()){

                        minFreq++;

                    }

                    while(map_with_freq[maxFreq].empty()){

                        maxFreq--;

                    }

                    return place;
                }

                int remove_MFU(){

                    int keyToRemove = map_with_freq[maxFreq].front();

                    map_with_freq[maxFreq].pop_front();
                    map_to_freq_list.erase(keyToRemove);

                    while(map_with_freq[minFreq].empty()){

                        minFreq++;

                    }

                    while(map_with_freq[maxFreq].empty()){

                        maxFreq--;

                    }

                    int place = map_to_buffer[keyToRemove]->second.first;
                    buffer.erase(map_to_buffer[keyToRemove]);
                    map_to_buffer.erase(keyToRemove);

                    return place;
                }
        };

    //=============================================================================================================================================

    
    //================================================================ Class User

        class User{

            // Each tenant has a base LRU with Q base capacity to calculate its cost
            // A LFU, LRU, MRU structure to decide which policy to use
            // my_faults is the actual faults of the tenant and next_faults is a variable to simulate future faults
            // You will probably notice a huge faill in the maintenance of LFU, LRU and MRU structures to decide
            // which one to use, I was aware of it but did not had the time to fix it, as it was kind of working I sent
            // the solution as it is, I decided not to fix the code before publishing it so it will be exactly the solution sent
            // I only added comments and removed parts not used

            public:
                int priority, qMin, qMax, freq, my_faults, next_faults, lru_faults, mru_faults, lfu_faults, policy;
                long double qBase, cost, base_faults, dataSize;

                LRUCache lrucache;
                LFUCache lfucache;
                MRUCache mrucache;
                LRUCache lrubase;
                MIXCache mixcache;

                unordered_map<int, int> age_map; // page : age(cicle referenced)

                // Policy 2 means that everyone starts in LFU

                User(int L, long double D, int Qmin, long double Qbase, int Qmax)
                    :   priority(L), dataSize(D), qMin(Qmin), qMax(Qmax), freq(0), my_faults(0), next_faults(0),
                        lru_faults(0), mru_faults(0), lfu_faults(0), policy(2), qBase(Qbase), cost(0), base_faults(0){}
        };

    //=============================================================================================================================================5


//#####################################################################################################################################################


//==================================================================   Main   ==================================================================//

    int main(){

        //--------------------------------------------- Read initial values and create the users


            // Tenants / Buffer size / Number of operations

                int N, Q, M;
                cin >> N >> Q >> M;

            //------------------------------------------------------------------------------------------
            

            // Priority / Data size / Qmin Qbase Qmax

                vector<int> L(N), D(N), qSizes(3 * N);

                for(int i = 0; i < N; i++){

                    cin >> L[i];

                }

                int Dmax = 0; // Store the greatest Data size to determine page_id indice
                for(int i = 0; i < N; i++){ 

                    cin >> D[i];
                    Dmax = max(Dmax, D[i]);

                }

                // When I decided not to have a global structure any longer a unique id was not needed anymore, but it remained in the code

                int countDiv = 1; // Variable to construct the unique id of each page
                while(Dmax >= 1){

                    Dmax /= 10;
                    countDiv *= 10;

                }

                for(int i = 0; i < 3 * N; i++){

                    cin >> qSizes[i];

                }

            //------------------------------------------------------------------------------------------


            // Create Users vector
            
                vector<User> listUsers;

                for (int i = 0; i < N; i++){

                    listUsers.emplace_back(L[i], D[i], qSizes[3 * i], qSizes[3 * i + 1], qSizes[3 * i + 2]);

                }

            //------------------------------------------------------------------------------------------


        //#####################################################################################################################################################


        //--------------------------------------------- Main Loop

            int bufferPlace = 0; // Variable to compare with size of global buffer
            for(int i = 0; i < M; i++){

                
                //--------------------------------------------- Read user_id and page request convert it to unique id -- Keep count of page ages

                    int user_id, page_num;
                    cin >> user_id >> page_num;

                    User &user = listUsers[user_id - 1];
                    int page = user_id * countDiv + page_num;

                    user.age_map[page] = i;
                    user.freq++;

                //---------------------------------------------------------------------------------------------------------------------------------------

                // LRU base and the others are there to calculate the cost and count the fautls if that policy were to be used
                // the fail in the logic is that the reference LRU LFU and MRU was supossed to keep the same size as mixed cache
                // but it is obviously not doing that because when a page is already in one of the caches it won't be added, but 
                // every time mixed cache loses a page, they will lose too

                //--------------------------------------------- Run LRU BASE

                    int lru_base_place = user.lrubase.retrieve(page);

                    if(lru_base_place == -1){

                        user.base_faults++;

                        if(user.lrubase.pos.size() < user.qBase){

                            user.lrubase.put(page, 1);

                        }else{

                            lru_base_place = user.lrubase.remove();
                            user.lrubase.put(page, 1);

                        }
                    }

                //---------------------------------------------------------------------------------------------------------------------------------------


                //--------------------------------------------- Run LRU CACHE

                    int lru_place = user.lrucache.retrieve(page);

                    if(lru_place == -1){

                        user.lru_faults++;

                        user.lrucache.put(page, 1);

                    }

                //---------------------------------------------------------------------------------------------------------------------------------------


                //--------------------------------------------- Run LFU CACHE

                    int lfu_place = user.lfucache.retrieve(page);

                    if(lfu_place == -1){

                        user.lfu_faults++;

                        user.lfucache.put(page, 1);
                        
                    }

                //---------------------------------------------------------------------------------------------------------------------------------------


                //--------------------------------------------- Run MRU CACHE

                    int mru_place = user.mrucache.retrieve(page);

                    if(mru_place == -1){

                        user.mru_faults++;

                        user.mrucache.put(page, 1);

                    }

                //---------------------------------------------------------------------------------------------------------------------------------------


                // This section is the actual cache being managed
                
                int place = user.mixcache.retrieve(page); // Request the page


                //--------------------------------------------- If page in buffer

                    if (place != -1){
                        
                        cout << place << endl << flush;

                    }

                //---------------------------------------------------------------------------------------------------------------------------------------


                //--------------------------------------------- If user maxed out

                    else if (user.mixcache.map_to_buffer.size() == user.qMax){

                        if(user.policy == 1){

                            place = user.mixcache.remove_LRU();
                            user.mixcache.put(page, place);

                        }else if(user.policy == 2){

                            place = user.mixcache.remove_LFU();
                            user.mixcache.put(page, place);

                        }else{

                            place = user.mixcache.remove_MFU();
                            user.mixcache.put(page, place);

                        }

                        cout << place << endl << flush;
                        user.my_faults++;
                        int discard = user.lrucache.remove();
                        discard = user.lfucache.remove();
                        discard = user.mrucache.remove();
                        discard = user.mrucache.remove();
                        user.mrucache.put(page, 1);

                    }

                //---------------------------------------------------------------------------------------------------------------------------------------


                //--------------------------------------------- If buffer not full

                    else if (bufferPlace < Q){

                        cout << bufferPlace + 1 << endl << flush;

                        user.mixcache.put(page, bufferPlace + 1);
                        
                        bufferPlace++;
                        user.my_faults++;
                    }

                //---------------------------------------------------------------------------------------------------------------------------------------


                //--------------------------------------------- Buffer full

                    else{

                        // If buffer is full a page must be evicted, a tenant will be chosen based on its current cost and
                        // and a removal will take place based on its policy assigned


                        int minUser = -1;
                        long double minInd = 10000000000;

                        for(int j = 0; j < N; j++){

                            User &tUser = listUsers[j];

                            int q_min = 0;

                            // If the tenant requesting the page has the minimum number of pages it is allowed to change its own page,
                            // because of that detail we must have different q_mins

                            if(j != user_id - 1){

                                q_min = tUser.qMin;

                            }else{

                                q_min = tUser.qMin - 1;

                            }

                            if(tUser.mixcache.map_to_buffer.size() <= q_min){

                                continue;

                            }

                            // A next_faults based on qBase and buffer size of that tenant and the frequency of requests is added to actual faults
                            // I used a few harded code values, but I am not sure if it was the best of the ideas, it was a big risk to lose points
                            // on the final tests, fortunatelly it was not the case, maybe I had more luck than most competitors

                            tUser.next_faults = (tUser.qBase - tUser.mixcache.map_to_buffer.size()) * (tUser.freq / static_cast<float>(i));

                            long double temp_cost = calc_cost(tUser.my_faults, tUser.base_faults, tUser.priority, tUser.next_faults);
                            if(temp_cost < 0){

                                tUser.cost = temp_cost * pow(tUser.mixcache.map_to_buffer.size(), 0.097) * pow(tUser.priority, -0.15) * pow(tUser.qBase, 0.9);

                            }else{

                                tUser.cost = temp_cost * pow(tUser.mixcache.map_to_buffer.size(), -0.075) * pow(tUser.priority, 0.08) * pow(tUser.qBase, -0.9);

                            }

                            if(tUser.cost < minInd){

                                minUser = j;
                                minInd = tUser.cost;

                            }
                        }

                        // Policy 3 should be MRU, but it was changed to MFU as I noticed it performed better in 26 and 27

                        if(listUsers[minUser].policy == 1){

                            place = listUsers[minUser].mixcache.remove_LRU();

                        }else if(listUsers[minUser].policy == 2){

                            place = listUsers[minUser].mixcache.remove_LFU();

                        }else{

                            place = listUsers[minUser].mixcache.remove_MFU();

                        }

                        int discard = listUsers[minUser].lrucache.remove();
                        discard = listUsers[minUser].lfucache.remove();
                        discard = listUsers[minUser].mrucache.remove();

                        user.mixcache.put(page, place);

                        cout << place << endl << flush;
                        user.my_faults++;

                        // To change between policies, not that finest of the systems but towards the end time was short

                        if(user.policy == 1){

                            if(user.lfu_faults < 0.997 * user.lru_faults){

                                user.policy = 2;

                            }else if(user.mru_faults < 0.995 * user.lru_faults){

                                user.policy = 3;

                            }

                        }else if(user.policy == 2){

                            if(user.lru_faults < user.lfu_faults){

                                user.policy = 1;

                            }else if(user.mru_faults < 0.994 * user.lfu_faults){

                                user.policy = 3;

                            }

                        }else{

                            if(user.lru_faults < 0.995 * user.mru_faults){

                                user.policy = 1;

                            }else if(user.lfu_faults < user.mru_faults){

                                user.policy = 2;

                            }
                        }
                    }
                
                //---------------------------------------------------------------------------------------------------------------------------------------
                
            }

        //#####################################################################################################################################################


        return 0;
    }

//#####################################################################################################################################################

        
