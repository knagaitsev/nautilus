<<<<<<< HEAD
export NAUT_ENABLE_INTS=1; make clean ; make -j ; make bitcode ; make isoimage ; make uImage
=======
export NAUT_ENABLE_INTS=1 && make clean && make -j && make bitcode && make final_no_timing && make isoimage && make uImage
>>>>>>> fdf6b0a8 (add easier way to pick out desired benchmark)
