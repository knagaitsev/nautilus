/* 
 * This file is part of the Nautilus AeroKernel developed
 * by the Hobbes and V3VEE Projects with funding from the 
 * United States National  Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  The Hobbes Project is a collaboration
 * led by Sandia National Laboratories that includes several national 
 * laboratories and universities. You can find out more at:
 * http://www.v3vee.org  and
 * http://xstack.sandia.gov/hobbes
 *
 * Copyright (c) 2020, Drew Kersnar <drewkersnar2021@u.northwestern.edu>
 * Copyright (c) 2020, Gaurav Chaudhary <gauravchaudhary2021@u.northwestern.edu>
 * Copyright (c) 2020, Souradip Ghosh <sgh@u.northwestern.edu>
 * Copyright (c) 2020, Brian Suchy <briansuchy2022@u.northwestern.edu>
 * Copyright (c) 2020, Peter Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2020, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Drew Kersnar, Gaurav Chaudhary, Souradip Ghosh, 
 * 			Brian Suchy, Peter Dinda 
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */


//This header file holds the functions that will be injected into any program using Carat. The functions are responsible for building the runtime tables
//as well as holding the PDG (will have to figure this out).

//Determine C vs C++ (Probably C++)


#include "runtime_tables.h" // CONV [.hpp] -> [.h]



//Alloc addr, length
nk_slist_uintptr_t *allocationMap = NULL; // FIX (for pairs, maps)

allocEntry* StackEntry;
uint64_t rsp = 0;

uint64_t escapeWindowSize = 0;
uint64_t totalEscapeEntries = 0;
void*** escapeWindow = NULL; // CONV [nullptr] -> [NULL]

// CONV [class with an init constructor] -> [init function] FIX do we need to call this somewhere?
// TODO: throw this in init, eventually call this with every new address space
void texasStartup() {
	user_init();
	texas_init();
} 

// CONV [class constructor] -> [function that returns an instance]
allocEntry* allocEntry(void* ptr, uint64_t len){
	allocEntry* newAllocEntry = (allocEntry*) CARAT_MALLOC(sizeof(allocEntry)); // maybe FIX
	newAllocEntry->pointer = ptr;
	newAllocEntry->length = len;
	newAllocEntry->allocToEscapeMap = nk_slist_build(uintptr_t, 6);// maybe FIX the 6
	return newAllocEntry;
}


int64_t doesItAlias(void *allocAddr, uint64_t length, uint64_t escapeVal){
	uint64_t blockStart = (uint64_t) allocAddr;
	if ((escapeVal >= blockStart) && (escapeVal < (blockStart + length))) { // CONV [no ()] -> [()]
		return escapeVal - blockStart;
	}
	else{
		return -1;
	}
}

void AddToAllocationTable(void *address, uint64_t length){
	CARAT_PRINT("In add to alloc: %p, %lu, %p\n", address, length, allocationMap);

	allocEntry *newEntry = allocEntry(address, length); // CONV [calling class constructor] -> [calling function that returns an instance]
	CARAT_PRINT("Adding to allocationMap\n");

	// CONV [map::insert] -> [nk_slist_add]
	nk_pair_uintptr_t_uint64_t *pair = NK_PAIR_BUILD(uintptr_t, uintptr_t, ((uintptr_t) address), ((uintptr_t) newEntry));
	nk_slist_add(uintptr_t, allocationMap, ((uintptr_t) pair)); // FIX: add_or_panic

	/*
	
	For Brian --- sanity check

	Brian --- we're assuming you wanted to avoid
	a 2logn operation instead of logn

	// insert
	if (map.find(key) == nullptr)
		map[key] = val;

	// insert_or_assign
	map[key] = val;

	*/

	CARAT_PRINT("Returning\n");

	return;
}

void AddCallocToAllocationTable(void *address, uint64_t len, uint64_t sizeOfEntry){
	uint64_t length = len * sizeOfEntry;
	allocEntry *newEntry = allocEntry(address, length); // CONV [class constructor] -> [function that returns an instance]

	// CONV [map::insert_or_assign] -> [nk_slist_add_by_force]
	nk_pair_uintptr_t_uint64_t *pair = NK_PAIR_BUILD(uintptr_t, uintptr_t, ((uintptr_t) address), ((uintptr_t) newEntry));
	nk_slist_add_by_force(uintptr_t, allocationMap, ((uintptr_t) pair));// FIX: add_or_panic
	
	return;
}

void HandleReallocInAllocationTable(void *address, void *newAddress, uint64_t length){
	allocationMap->erase(address); // FIX
	allocEntry *newEntry = allocEntry(newAddress, length); // CONV [class constructor] -> [function that returns an instance]

	// CONV [map::insert_or_assign] -> [nk_slist_add_by_force]
	nk_pair_uintptr_t_uint64_t *pair = NK_PAIR_BUILD(uintptr_t, uintptr_t, ((uintptr_t) newAddress), ((uintptr_t) newEntry));
	nk_slist_add_by_force(uintptr_t, allocationMap, ((uintptr_t) pair));// FIX: add_or_panic
	
	return;
}



/*
 * 1) Search for address escaping within allocation table (an ordered map that contains all current allocations <non overlapping blocks of memory each consisting of an  address and length> made by the program
 * 2) If found (the addressEscaping variable falls within one of the blocks), then a new entry into the escape table is made consisting of the addressEscapingTo, the addressEscaping, and the length of the addressEscaping (for optimzation if consecutive escapes occur)
 */
void AddToEscapeTable(void* addressEscaping){
	if(totalEscapeEntries >= escapeWindowSize){
		processEscapeWindow();
	}
	escapeWindow[totalEscapeEntries] = (void**)addressEscaping;
	totalEscapeEntries++;
	CARAT_PRINT(stderr, "CARAT: %016lx\n", __builtin_return_address(0));

	return;
}

allocEntry* findAllocEntry(void *address){

	// CONV [brian] -> [better than brian] 
	
	__auto_type prospective = nk_slist_better_lower_bound(uintptr_t, allocationMap, (uintptr_t) address);

	/* 
	 * [prospective] -> [findAllocEntry return]:
	 * 1) the left sentinal (see below) -> NULL
	 * 2) a nk_slist_node that does contain the address -> allocEntry *
	 * 3) a nk_slist_node that does NOT contain the address -> NULL
	 */ 

	// better_lower_bound may return the left sential if the address is 
	// lower than all addresses that exist in the skip list (edge case)
	if (nk_slist_is_left_sentinal(prospective)) { return NULL; }

	// Gather length of the block
	allocEntry *prospective_entry = ((allocEntry *) prospective->second); 
	uint64_t blockLen = prospective_entry->length; 

	// Could be in the block (return NULL otherwise)
	if (doesItAlias(prospective->first, blockLen, (uint64_t) address) == -1) { return NULL; }

	return prospective->second;
}

void processEscapeWindow(){
	CARAT_PRINT("\n\n\nI am invoked!\n\n\n");
	
	nk_slist_uintptr_t *processedEscapes = nk_slist_build(uintptr_t, 6); // Confirm top gear	

	for (uint64_t i = 0; i < totalEscapeEntries; i++)
	{
		void **addressEscaping = escapeWindow[i];

		// CONV [processedEscapes.find() ... != processedEscapes.end()] -> [nk_slist_find]
		if (nk_slist_find(((uintptr_t) addressEscaping))) { continue; }	

		// CONV [map::insert] -> [nk_slist_add]
		nk_slist_add(uintptr_t, processedEscapes, ((uintptr_t) addressEscaping));

 		if (addressEscaping == NULL) { continue; }

		// FIX --- Dereferencing a void * in C --- ????
		allocEntry *alloc = findAllocEntry(*addressEscaping); // CONV [auto] -> [allocEntry*]

		if(alloc == NULL){ // CONV [nullptr] -> [NULL]
			continue;
		}

		// We have found block
		// 
		// Inserting the object address escaping along with the length it is running for
		// and the length (usually 1) into escape table with key of addressEscapingTo
		// 
		// One can reverse engineer the starting point if the length is longer than 1
		// by subtracting from value in dereferenced addressEscapingTo
		
		// CONV [map::insert] -> [nk_slist_add]
		nk_slist_add(uintptr_t, (alloc->allocToEscapeMap), ((uintptr_t) addressEscaping));
	}

	totalEscapeEntries = 0;
}


// This function will remove an address from the allocation from a free() or free()-like instruction being called
void RemoveFromAllocationTable(void *address){
	// CONV [map::erase] -> [nk_slist_remove]
	nk_slist_remove(uintptr_t, allocationMap, (uintptr_t) address)
}

void ReportStatistics(){
	// CONV [map::size] -> [nk_slist_get_size]
	// NOTE --- nk_slist_get_size ISN'T implemented yet
	CARAT_PRINT("Size of Allocation Table: %lu\n", nk_slist_get_size(allocationMap)); 
}


uint64_t getrsp(){
	uint64_t retVal;
	__asm__ __volatile__("movq %%rsp, %0" : "=a"(retVal) : : "memory");
	return retVal;
}


void texas_init(){
	rsp = getrsp();
	
	allocationMap = nk_slist_build(uintptr_t, 12); // CONV [new map<void *, allocEntry *>] -> [nk_slist_build(uintptr_t)]
	
	escapeWindowSize = 1048576;
	totalEscapeEntries = 0;

	//This adds the stack pointer to a tracked alloc that is length of 32GB which encompasses all programs we run
	void* rspVoidPtr = (void*)(rsp-0x800000000ULL);
	StackEntry = allocEntry(rspVoidPtr, 0x800000000ULL); // CONV [constructor] -> [function that returns an instance]

	// CONV [map::insert_or_assign] -> [nk_slist_add]
	nk_pair_uintptr_t_uint64_t *pair = NK_PAIR_BUILD(uintptr_t, uintptr_t, ((uintptr_t) rspVoidPtr), ((uintptr_t) StackEntry));
	nk_slist_add(uintptr_t, allocationMap, ((uintptr_t) pair)); // FIX: add_or_panic
	
	escapeWindow = (void ***) CARAT_MALLOC(escapeWindowSize, sizeof(void *)); // CONV[calloc] -> [CARAT_MALLOC]

	CARAT_PRINT("Leaving texas_init\n");
	return;
}


