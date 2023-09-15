CXXFLAGS += -DSHARED_LOWLEVEL -DCOMPILE_TIME_MIN_LEVEL=LEVEL_DEBUG  -D__LEGION__
#-DDEBUG_LOWLEVEL -DDEBUG_HIGHLEVEL 

obj-$(NAUT_CONFIG_ARCH_X86) = \
	threads.o \
	groups.o \
        tasks.o \
        futures.o \
        bsp.o \
        net_udp_echo.o \
        test.o \
#       lru_cache_test.o

obj-$(NAUT_CONFIG_ARCH_ARM64) = \
	threads.o \
	groups.o \
	tasks.o \
	futures.o \
	bsp.o

obj-$(NAUT_CONFIG_ARCH_RISCV) = \
	threads.o \
	groups.o \
	tasks.o \
	futures.o \
	bsp.o

obj-$(NAUT_CONFIG_ASPACE_CARAT) += skiplist_test.o \
                                   map_test.o

obj-$(NAUT_CONFIG_PROVENANCE) += provenance.o

obj-$(NAUT_CONFIG_TEST_FIBERS) += fibers.o \
								   fibers_random.o

obj-$(NAUT_CONFIG_PROCESSES) += process.o \
								signal.o

obj-$(NAUT_CONFIG_TEST_CACHEPART) += cachepart.o

#obj-$(NAUT_CONFIG_OPENMP_RT_TESTS)  += openmp/
# KARAT TESTING
#obj-y += openmp/

obj-$(NAUT_CONFIG_NDPC_RT_TESTS) += ndpc/
obj-$(NAUT_CONFIG_NESL_RT_TESTS) += nesl/

obj-$(NAUT_CONFIG_X86_64_HOST) += ipi.o
obj-$(NAUT_CONFIG_X86_64_HOST) += benchmark.o

obj-$(NAUT_CONFIG_GEM5) += ipi.o
obj-$(NAUT_CONFIG_GEM5) += benchmark.o

obj-$(NAUT_CONFIG_LEGION_RT) += test_legion.o \
								circuit.o \
								circuit_cpu.o \
								circuit_mapper.o 
								#saxpy_array.o 
								#delaunay.o 
								#hello_world.o \
								#partitioning.o \
								# custom_mapper.o \
								# multiple_partitions.o \
								# privileges.o \
								# physical_regions.o \
								# logical_regions.o \
								# global_vars.o \
								# index_tasks.o \
								# tasks_and_futures.o	\

obj-$(NAUT_CONFIG_ALLOC_TEST) += alloc/

obj-$(NAUT_CONFIG_TEST_ALLOC_IMPLICIT) += allocator_test.o
obj-$(NAUT_CONFIG_TEST_TRACE_ALLOC_IMPLICIT) += allocator_test_trace.o
